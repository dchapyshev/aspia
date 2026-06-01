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

#ifndef ROUTER_SESSION_ADMIN_H
#define ROUTER_SESSION_ADMIN_H

#include <QList>

#include "router/session_manager.h"

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

class SessionAdmin final : public SessionManager
{
    Q_OBJECT

public:
    explicit SessionAdmin(TcpChannel* channel, QObject* parent = nullptr);
    ~SessionAdmin() final;

protected:
    // Session implementation.
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
    std::string modifyUser(const proto::router::User& user);
    std::string deleteUser(const proto::router::User& user);

    // Tears down live sessions of |user_id| that authenticated with a revoked token. An empty
    // |token_ids| drops every session of the user; otherwise only those matching a listed token.
    void disconnectRevokedSessions(qint64 user_id, const QList<qint64>& token_ids);

    Q_DISABLE_COPY_MOVE(SessionAdmin)
};

#endif // ROUTER_SESSION_ADMIN_H
