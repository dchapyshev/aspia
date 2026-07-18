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
    // Opens the connection to |router_id| (its config is read from the database) and authenticates.
    // A no-op if it already exists.
    void onConnect(qint64 router_id);

    // Closes the connection to |router_id|.
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

private:
    void onChannelAuthenticated(qint64 router_id);
    void onChannelErrorOccurred(qint64 router_id, TcpChannel::ErrorCode error_code);

    // Active connections keyed by router_id. Each channel owns its authenticator and is parented to
    // this worker.
    QHash<qint64, TcpChannel*> channels_;

    Q_DISABLE_COPY_MOVE(RouterWorker)
};

#endif // CLIENT_WORKERS_ROUTER_WORKER_H
