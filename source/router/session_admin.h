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

#include "proto/router_admin.h"
#include "router/session_manager.h"

namespace router {

class SessionAdmin final : public SessionManager
{
    Q_OBJECT

public:
    explicit SessionAdmin(base::TcpChannel* channel, QObject* parent = nullptr);
    ~SessionAdmin() final;

protected:
    // Session implementation.
    void onSessionMessage(quint8 channel_id, const QByteArray& buffer) final;

private:
    void doRelayListRequest();
    void doHostListRequest();
    void doUserListRequest();
    void doUserRequest(const proto::router::UserRequest& request);
    void doHostRequest(const proto::router::HostRequest& request);
    void doRelayRequest(const proto::router::RelayRequest& request);
    void doPeerRequest(const proto::router::PeerRequest& request);

    std::string addUser(const proto::router::User& user);
    std::string modifyUser(const proto::router::User& user);
    std::string deleteUser(const proto::router::User& user);

    Q_DISABLE_COPY_MOVE(SessionAdmin)
};

} // namespace router

#endif // ROUTER_SESSION_ADMIN_H
