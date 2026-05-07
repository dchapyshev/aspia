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

#ifndef ROUTER_SESSION_H
#define ROUTER_SESSION_H

#include <QHostAddress>
#include <QObject>
#include <QVersionNumber>

#include "base/logging.h"
#include "base/net/tcp_channel.h"

namespace proto::router {
enum SessionType : int;
} // namespace proto::router

class Session : public QObject
{
    Q_OBJECT

public:
    Session(TcpChannel* channel, QObject* parent);
    virtual ~Session() override;

    void start();

    QVersionNumber version() const;
    QString osName() const;
    QString computerName() const;
    QString architecture() const;
    QString userName() const;
    proto::router::SessionType sessionType() const;

    qint64 sessionId() const { return session_id_; }
    const QHostAddress& address() const { return address_; }
    time_t startTime() const { return start_time_; }
    std::chrono::seconds duration() const;

signals:
    void sig_finished(qint64 session_id);

protected:
    LOG_DECLARE_CONTEXT(Session);

    void sendMessage(quint8 channel_id, const QByteArray& message);
    virtual void onSessionMessage(quint8 channel_id, const QByteArray& buffer) = 0;

private slots:
    void onTcpErrorOccurred(TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);

private:
    const qint64 session_id_;
    time_t start_time_ = 0;

    TcpChannel* tcp_channel_ = nullptr;
    QHostAddress address_;
};

#endif // ROUTER_SESSION_H
