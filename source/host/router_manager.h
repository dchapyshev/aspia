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

#ifndef HOST_ROUTER_MANAGER_H
#define HOST_ROUTER_MANAGER_H

#include <QQueue>

#include <optional>

#include "base/shared_pointer.h"
#include "base/net/tcp_channel.h"
#include "base/peer/host_id.h"
#include "base/peer/user_list.h"
#include "proto/user.h"

class QTimer;

namespace base {
class RelayPeerManager;
} // namespace base

namespace host {

class RouterManager final : public QObject
{
    Q_OBJECT

public:
    explicit RouterManager(QObject* parent = nullptr);
    ~RouterManager() final;

    const QString& address() const { return address_; }
    quint16 port() const { return port_; }
    const QByteArray& publicKey() const { return public_key_; }

    struct ReadyConnection
    {
        base::TcpChannel* tcp_channel = nullptr;
        QString stun_host;
        quint16 stun_port = 0;
        bool peer_equals = false;
    };

    bool hasReadyConnections() const;
    std::optional<ReadyConnection> nextReadyConnection();

public slots:
    void start();
    void onUserListChanged();
    void onOneTimeSessionsChanged(quint32 one_time_sessions);
    void onUserSessionAttached();

signals:
    void sig_routerStateChanged(const proto::user::RouterState& state);
    void sig_credentialsChanged(base::HostId host_id, const QString& one_time_password);
    void sig_clientConnected();
    void sig_removeHost(bool try_to_uninstall);

private slots:
    void onTcpReady();
    void onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onNewPeerConnected();

private:
    void connectToRouter();
    void delayedConnectToRouter();
    void routerStateChanged(proto::user::RouterState::State state);
    void hostIdRequest();
    base::User createOneTimeUser() const;

    base::TcpChannel* tcp_channel_ = nullptr;
    base::RelayPeerManager* peer_manager_ = nullptr;
    QTimer* reconnect_timer_ = nullptr;

    QString address_;
    quint16 port_ = 0;
    QByteArray public_key_;

    QTimer* password_expire_timer_ = nullptr;
    QString one_time_password_;
    quint32 one_time_sessions_ = 0;

    base::SharedPointer<base::UserListBase> user_list_;

    base::HostId host_id_ = base::kInvalidHostId;
    proto::user::RouterState router_state_;

    QQueue<ReadyConnection> channels_;

    Q_DISABLE_COPY_MOVE(RouterManager)
};

} // namespace host

#endif // HOST_ROUTER_MANAGER_H
