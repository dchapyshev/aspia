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

#ifndef RELAY_WORKERS_RELAY_WORKER_H
#define RELAY_WORKERS_RELAY_WORKER_H

#include <QList>

#include <asio/ip/tcp.hpp>

#include <memory>

#include "base/shared_pointer.h"
#include "base/threading/worker.h"
#include "proto/router_relay.h"

class FloodGuard;
class PendingSession;
class Session;

namespace proto::relay {
class PeerToRelay;
} // namespace proto::relay

class RelayWorker final : public Worker
{
    Q_OBJECT

public:
    RelayWorker();
    ~RelayWorker() final;

public slots:
    // Disconnects the peer session |session_id|.
    void onDisconnectSession(qint64 session_id);

signals:
    // Emitted from the worker thread when a pair of peers has been connected.
    void sig_sessionStarted();

    // Emitted from the worker thread when an active peer session has finished.
    void sig_sessionFinished();

    // Emitted from the worker thread at the configured interval with session statistics.
    void sig_statistics(const proto::router::RelayStatistics& statistics);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;
    void onTimer(TimePoint now) final;

private slots:
    void onPendingSessionReady(const proto::relay::PeerToRelay& message);
    void onPendingSessionFailed();
    void onSessionFinished();

private:
    static void doAccept(RelayWorker* self);
    void removePendingSession(PendingSession* session);
    void removeSession(Session* session);

    Minutes idle_timeout_;

    SharedPointer<bool> alive_guard_ { new bool(true) };

    // Created in the worker thread so the socket binds to its io_context.
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;

    QList<PendingSession*> pending_sessions_;
    QList<Session*> active_sessions_;

    // Statistics are disabled while |stat_interval_| is zero.
    Seconds stat_interval_ { Seconds::zero() };
    TimePoint next_idle_check_;
    TimePoint next_stat_time_;
    TimePoint start_time_;

    // Anti-flood gate: per-address rate limit + global pending cap + rate-limited logging.
    std::unique_ptr<FloodGuard> flood_guard_;

    Q_DISABLE_COPY_MOVE(RelayWorker)
};

#endif // RELAY_WORKERS_RELAY_WORKER_H
