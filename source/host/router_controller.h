//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_ROUTER_CONTROLLER_H
#define HOST_ROUTER_CONTROLLER_H

#include <QQueue>
#include <QPointer>
#include <QTimer>

#include "base/net/tcp_channel.h"
#include "base/peer/client_authenticator.h"
#include "base/peer/host_id.h"
#include "base/peer/relay_peer_manager.h"
#include "proto/host_internal.h"

namespace host {

class RouterController final : public QObject
{
    Q_OBJECT

public:
    explicit RouterController(QObject* parent = nullptr);
    ~RouterController() final;

    void start(const QString& address, quint16 port, const QByteArray& public_key);

    const QString& address() const { return address_; }
    quint16 port() const { return port_; }
    const QByteArray& publicKey() const { return public_key_; }
    const base::HostId hostId() const { return host_id_; }
    const proto::internal::RouterState state() const { return router_state_; }

    bool hasPendingConnections() const;
    base::TcpChannel* nextPendingConnection();

signals:
    void sig_routerStateChanged(const proto::internal::RouterState& router_state);
    void sig_hostIdAssigned(base::HostId host_id);
    void sig_clientConnected();

private slots:
    void onTcpConnected();
    void onTcpDisconnected(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onNewPeerConnected();

private:
    void connectToRouter();
    void delayedConnectToRouter();
    void routerStateChanged(proto::internal::RouterState::State state);
    void hostIdRequest();

    QPointer<base::TcpChannel> tcp_channel_;
    QPointer<base::ClientAuthenticator> authenticator_;
    QPointer<base::RelayPeerManager> peer_manager_;
    QPointer<QTimer> reconnect_timer_;

    QString address_;
    quint16 port_ = 0;
    QByteArray public_key_;

    base::HostId host_id_ = base::kInvalidHostId;
    proto::internal::RouterState router_state_;

    QQueue<base::TcpChannel*> channels_;

    Q_DISABLE_COPY(RouterController)
};

} // namespace host

#endif // HOST_ROUTER_CONTROLLER_H
