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

#ifndef CLIENT_ROUTER_CONNECTION_H
#define CLIENT_ROUTER_CONNECTION_H

#include <QObject>

#include "base/net/tcp_channel.h"
#include "client/config.h"

class QTimer;

namespace proto::router {
class ConnectionOffer;
class HostList;
class HostResult;
class RelayList;
class RelayResult;
class User;
class UserList;
class UserResult;
} // namespace proto::router

namespace client {

class RouterConnection final : public QObject
{
    Q_OBJECT

public:
    enum class Status
    {
        OFFLINE,
        CONNECTING,
        ONLINE
    };
    Q_ENUM(Status)

    explicit RouterConnection(const RouterConfig& config, QObject* parent = nullptr);
    ~RouterConnection() final;

    static RouterConnection* instance(qint64 router_id);

    Status status() const { return status_; }
    const RouterConfig& config() const;
    qint64 routerId() const;
    QVersionNumber version() const;

public slots:
    // Generic methods.
    void onConnectToRouter();
    void onDisconnectFromRouter();
    void onUpdateConfig(const client::RouterConfig& config);

    // Administrator methods.
    void onRelayListRequest();
    void onHostListRequest();
    void onUserListRequest();
    void onAddUser(const proto::router::User& user);
    void onModifyUser(const proto::router::User& user);
    void onDeleteUser(qint64 entry_id);
    void onDisconnectHost(qint64 session_id);
    void onRemoveHost(qint64 session_id, bool try_to_uninstall);
    void onDisconnectRelay(qint64 session_id);
    void onDisconnectPeer(qint64 relay_entry_id, quint64 peer_session_id);

    // Manager methods.
    // TODO

    // Client methods.
    void onConnectionRequest(qint64 request_id, quint64 host_id);
    void onCheckHostStatus(qint64 request_id, quint64 host_id);

signals:
    // Generic signals.
    void sig_statusChanged(qint64 router_id, client::RouterConnection::Status status);
    void sig_errorOccurred(qint64 router_id, base::TcpChannel::ErrorCode error_code);

    // Administrator signals.
    void sig_relayListReceived(const proto::router::RelayList& list);
    void sig_hostListReceived(const proto::router::HostList& list);
    void sig_userListReceived(const proto::router::UserList& list);
    void sig_userResultReceived(const proto::router::UserResult& result);
    void sig_hostResultReceived(const proto::router::HostResult& result);
    void sig_relayResultReceived(const proto::router::RelayResult& result);

    // Client signals.
    void sig_connectionOffer(const proto::router::ConnectionOffer& offer);
    void sig_hostStatus(qint64 request_id, bool online);

private slots:
    void onTcpReady();
    void onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);

private:
    void setStatus(Status status);
    void sendMessage(quint8 channel_id, const QByteArray& data);

    RouterConfig config_;
    base::TcpChannel* tcp_channel_ = nullptr;
    QTimer* reconnect_timer_ = nullptr;
    Status status_ = Status::OFFLINE;

    Q_DISABLE_COPY_MOVE(RouterConnection)
};

} // namespace client

#endif // CLIENT_ROUTER_CONNECTION_H
