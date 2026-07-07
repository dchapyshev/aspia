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

#ifndef ROUTER_CLIENT_ADMIN_H
#define ROUTER_CLIENT_ADMIN_H

#include "router/client_manager.h"

namespace proto::router {
class ClientListRequest;
class ClientRequest;
class HostRequest;
class PeerRequest;
class RelayListRequest;
class RelayRequest;
class User;
class UserListRequest;
class UserRequest;
class WorkspaceRequest;
} // namespace proto::router

class ClientAdmin final : public ClientManager
{
    Q_OBJECT

public:
    explicit ClientAdmin(TcpChannel* channel, QObject* parent = nullptr);
    ~ClientAdmin() final;

protected:
    // Client implementation.
    void onSessionMessage(quint8 channel_id, const QByteArray& buffer) final;

private:
    void doRelayListRequest(const proto::router::RelayListRequest& request);
    void doClientListRequest(const proto::router::ClientListRequest& request);
    void doUserListRequest(const proto::router::UserListRequest& request);
    void doUserRequest(const proto::router::UserRequest& request);
    void doHostRequest(const proto::router::HostRequest& request);
    void doRelayRequest(const proto::router::RelayRequest& request);
    void doClientRequest(const proto::router::ClientRequest& request);
    void doPeerRequest(const proto::router::PeerRequest& request);
    void doWorkspaceRequest(const proto::router::WorkspaceRequest& request);

    std::string addUser(const proto::router::User& user);
    std::string modifyUser(const proto::router::User& user, qint64* password_changed_user_id);
    std::string deleteUser(const proto::router::User& user);

    Q_DISABLE_COPY_MOVE(ClientAdmin)
};

#endif // ROUTER_CLIENT_ADMIN_H
