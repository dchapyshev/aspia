//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef ROUTER_WORKERS_EMAIL_WORKER_H
#define ROUTER_WORKERS_EMAIL_WORKER_H

#include <asio/steady_timer.hpp>

#include <deque>
#include <memory>
#include <unordered_map>

#include "base/scoped_qpointer.h"
#include "base/net/curl_util.h"
#include "base/threading/worker.h"

class QSocketNotifier;

class EmailWorker final : public Worker
{
    Q_OBJECT

public:
    EmailWorker();
    ~EmailWorker() final;

    void send(const QString& to, const QString& subject, const QString& text);

protected:
    // Worker implementation.
    void onPrepare() final;
    void onStart() final;
    void onStop() final;
    void onTimer(TimePoint now) final;

private:
    struct Email
    {
        QString to;
        QByteArray data;
        qsizetype offset = 0;
        ScopedCurlSlist recipients;
    };

    struct SocketNotifiers
    {
        void setInterest(int what);

        ScopedQPointer<QSocketNotifier> read;
        ScopedQPointer<QSocketNotifier> write;
    };

    static int socketCallback(CURL* easy, curl_socket_t socket, int what, void* userp, void* socketp);
    static int timerCallback(CURLM* multi, long timeout_ms, void* userp);
    static size_t readCallback(char* buffer, size_t size, size_t count, void* userp);

    void doSend(const QString& to, const QString& subject, const QString& text);
    void sendFront();
    void scheduleRetry();
    void watchSocket(curl_socket_t socket, int what);
    void setMultiTimeout(long timeout_ms);
    void driveMulti(curl_socket_t socket, int action);

    std::unique_ptr<ScopedCURLM> multi_;
    std::unique_ptr<ScopedCURL> curl_;
    std::unique_ptr<asio::steady_timer> multi_timer_;

    QByteArray from_;
    char error_[CURL_ERROR_SIZE] = {};

    // The front email is the one being sent, or the one waiting for its retry.
    std::deque<Email> queue_;
    TimePoint retry_time_ = TimePoint::max();
    Seconds retry_delay_;

    std::unordered_map<curl_socket_t, SocketNotifiers> sockets_;

    Q_DISABLE_COPY_MOVE(EmailWorker)
};

#endif // ROUTER_WORKERS_EMAIL_WORKER_H
