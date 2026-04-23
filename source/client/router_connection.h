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

namespace proto::router {
class HostList;
class RelayList;
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

    Status status() const { return status_; }
    const RouterConfig& config() const;
    const QUuid& uuid() const;

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

    // Manager methods.

signals:
    void sig_statusChanged(const QUuid& uuid, client::RouterConnection::Status status);
    void sig_relayListReceived(const proto::router::RelayList& list);
    void sig_hostListReceived(const proto::router::HostList& list);
    void sig_userListReceived(const proto::router::UserList& list);
    void sig_userResultReceived(const proto::router::UserResult& result);

private slots:
    void onTcpReady();
    void onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);

private:
    void setStatus(Status status);
    void sendMessage(quint8 channel_id, const QByteArray& data);

    RouterConfig config_;
    base::TcpChannel* tcp_channel_ = nullptr;
    Status status_ = Status::OFFLINE;

    Q_DISABLE_COPY_MOVE(RouterConnection)
};

} // namespace client

#endif // CLIENT_ROUTER_CONNECTION_H
