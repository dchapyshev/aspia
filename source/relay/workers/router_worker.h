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

#ifndef RELAY_WORKERS_ROUTER_WORKER_H
#define RELAY_WORKERS_ROUTER_WORKER_H

#include "base/serialization.h"
#include "base/scoped_qpointer.h"
#include "base/net/tcp_channel.h"
#include "base/threading/worker.h"
#include "proto/router_relay.h"

class RouterWorker final : public Worker
{
    Q_OBJECT

public:
    RouterWorker();
    ~RouterWorker() final;

signals:
    // Asks the relay side to disconnect the peer session |session_id|.
    void sig_disconnectSession(qint64 session_id);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;
    void onTimer(TimePoint now) final;

private slots:
    void onTcpConnected();
    void onTcpErrorOccurred(TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onSessionStarted();
    void onSessionFinished();
    void onSessionStatistics(const proto::router::RelayStatistics& statistics);

private:
    void connectToRouter();
    void delayedConnectToRouter();
    void expireKey(quint32 key_id);
    void refreshKeyPool();

    // Router settings.
    QString router_address_;
    quint16 router_port_ = 0;
    QByteArray router_public_key_;

    // Peers settings.
    QString peer_address_;
    quint16 peer_port_ = 0;
    quint32 max_peer_count_ = 0;

    ScopedQPointer<TcpChannel> tcp_channel_;

    Parser<proto::router::RouterToRelay> incoming_message_;
    Serializer<proto::router::RelayToRouter> outgoing_message_;

    int session_count_ = 0;

    Q_DISABLE_COPY_MOVE(RouterWorker)
};

#endif // RELAY_WORKERS_ROUTER_WORKER_H
