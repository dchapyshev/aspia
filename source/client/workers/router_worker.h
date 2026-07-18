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

#ifndef CLIENT_WORKERS_ROUTER_WORKER_H
#define CLIENT_WORKERS_ROUTER_WORKER_H

#include <QHash>
#include <QVersionNumber>

#include "base/net/tcp_channel.h"
#include "base/threading/worker.h"

class RouterWorker final : public Worker
{
    Q_OBJECT

public:
    RouterWorker();
    ~RouterWorker() final;

public slots:
    // Starts (and keeps reconnecting) the connection to |router_id|. Its config is read from the
    // database. A no-op if it is already active.
    void onConnect(qint64 router_id);

    // Stops the connection to |router_id| and cancels its reconnect.
    void onDisconnect(qint64 router_id);

    // Sends an outgoing message on the given channel of |router_id|.
    void onSendMessage(qint64 router_id, quint8 channel_id, const QByteArray& buffer);

signals:
    void sig_authenticated(qint64 router_id, const QVersionNumber& peer_version);
    void sig_errorOccurred(qint64 router_id, TcpChannel::ErrorCode error_code);
    void sig_messageReceived(qint64 router_id, quint8 channel_id, const QByteArray& buffer);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;
    void onTimer() final;

private:
    // One router's connection state. |channel| is null while waiting to reconnect;
    // |reconnect_countdown| is the number of one-second ticks left before the next attempt.
    struct Connection
    {
        TcpChannel* channel = nullptr;
        int reconnect_countdown = 0;
    };

    void startConnection(qint64 router_id);
    void onChannelAuthenticated(qint64 router_id);
    void onChannelErrorOccurred(qint64 router_id, TcpChannel::ErrorCode error_code);

    // Wanted connections keyed by router_id; a present key means the router should stay connected.
    QHash<qint64, Connection> connections_;

    Q_DISABLE_COPY_MOVE(RouterWorker)
};

#endif // CLIENT_WORKERS_ROUTER_WORKER_H
