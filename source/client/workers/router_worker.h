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

#include <QVersionNumber>

#include <optional>

#include "base/net/tcp_channel.h"
#include "base/threading/worker.h"

class RouterWorker final : public Worker
{
    Q_OBJECT

public:
    RouterWorker();
    ~RouterWorker() final;

public slots:
    // Starts (and keeps reconnecting) the connection to the router. Its config is read from the
    // database. A no-op if it is already active.
    void onConnect();

    // Stops the connection to the router and cancels its reconnect.
    void onDisconnect();

    // Drops the current connection to the router and lets the timer bring it back, the same way a
    // channel error does. A router that is not connected yet is started the same way.
    void onReconnect();

    // Sends an outgoing message on the given channel of the router.
    void onSendMessage(quint8 channel_id, const QByteArray& buffer);

signals:
    void sig_authenticated(const QVersionNumber& peer_version);
    void sig_errorOccurred(TcpChannel::ErrorCode error_code);
    void sig_messageReceived(quint8 channel_id, const QByteArray& buffer);
    void sig_reconnecting();

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;
    void onTimer(TimePoint now) final;

private:
    // The connection state. |channel| is null while waiting to reconnect; |reconnect_countdown| is
    // the number of one-second ticks left before the next attempt.
    struct Connection
    {
        TcpChannel* channel = nullptr;
        int reconnect_countdown = 0;
    };

    void startConnection();
    void onChannelAuthenticated();
    void onChannelErrorOccurred(TcpChannel::ErrorCode error_code);

    // Present while the router should stay connected.
    std::optional<Connection> connection_;

    Q_DISABLE_COPY_MOVE(RouterWorker)
};

#endif // CLIENT_WORKERS_ROUTER_WORKER_H
