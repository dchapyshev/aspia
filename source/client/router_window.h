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

#ifndef CLIENT_ROUTER_WINDOW_H
#define CLIENT_ROUTER_WINDOW_H

#include "base/peer/authenticator.h"

namespace proto {
class SessionList;
class SessionResult;
class UserList;
class UserResult;
} // namespace proto

namespace client {

class RouterWindow
{
public:
    virtual ~RouterWindow() = default;

    virtual void onConnecting() = 0;
    virtual void onConnected(const base::Version& peer_version) = 0;
    virtual void onDisconnected(base::TcpChannel::ErrorCode error_code) = 0;
    virtual void onWaitForRouter() = 0;
    virtual void onWaitForRouterTimeout() = 0;
    virtual void onVersionMismatch(const base::Version& router, const base::Version& client) = 0;
    virtual void onAccessDenied(base::Authenticator::ErrorCode error_code) = 0;
    virtual void onSessionList(std::shared_ptr<proto::SessionList> session_list) = 0;
    virtual void onSessionResult(std::shared_ptr<proto::SessionResult> session_result) = 0;
    virtual void onUserList(std::shared_ptr<proto::UserList> user_list) = 0;
    virtual void onUserResult(std::shared_ptr<proto::UserResult> user_result) = 0;
};

} // namespace client

#endif // CLIENT_ROUTER_WINDOW_H
