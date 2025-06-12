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

#ifndef ROUTER_SESSION_ADMIN_H
#define ROUTER_SESSION_ADMIN_H

#include "proto/router_admin.h"
#include "router/session.h"

namespace router {

class ServerProxy;

class SessionAdmin final : public Session
{
public:
    explicit SessionAdmin(QObject* parent = nullptr);
    ~SessionAdmin() final;

protected:
    // Session implementation.
    void onSessionReady() final;
    void onSessionMessageReceived(quint8 channel_id, const QByteArray& buffer) final;
    void onSessionMessageWritten(quint8 channel_id, size_t pending) final;

private:
    void doUserListRequest();
    void doUserRequest(const proto::router::UserRequest& request);
    void doSessionListRequest(const proto::router::SessionListRequest& request);
    void doSessionRequest(const proto::router::SessionRequest& request);
    void doPeerConnectionRequest(const proto::router::PeerConnectionRequest& request);

    proto::router::UserResult::ErrorCode addUser(const proto::router::User& user);
    proto::router::UserResult::ErrorCode modifyUser(const proto::router::User& user);
    proto::router::UserResult::ErrorCode deleteUser(const proto::router::User& user);

    DISALLOW_COPY_AND_ASSIGN(SessionAdmin);
};

} // namespace router

#endif // ROUTER_SESSION_ADMIN_H
