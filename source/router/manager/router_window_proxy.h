//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ROUTER__MANAGER__ROUTER_WINDOW_PROXY_H
#define ROUTER__MANAGER__ROUTER_WINDOW_PROXY_H

#include "router/manager/router_window.h"

namespace base {
class TaskRunner;
} // namespace base

namespace router {

class RouterWindowProxy : public std::enable_shared_from_this<RouterWindowProxy>
{
public:
    RouterWindowProxy(std::shared_ptr<base::TaskRunner> ui_task_runner,
                      RouterWindow* router_window);
    ~RouterWindowProxy();

    void dettach();

    void onConnected(const base::Version& peer_version);
    void onDisconnected(base::NetworkChannel::ErrorCode error_code);
    void onAccessDenied(peer::ClientAuthenticator::ErrorCode error_code);
    void onHostList(std::shared_ptr<proto::HostList> host_list);
    void onHostResult(std::shared_ptr<proto::HostResult> host_result);
    void onRelayList(std::shared_ptr<proto::RelayList> relay_list);
    void onUserList(std::shared_ptr<proto::UserList> user_list);
    void onUserResult(std::shared_ptr<proto::UserResult> user_result);

private:
    std::shared_ptr<base::TaskRunner> ui_task_runner_;
    RouterWindow* router_window_;

    DISALLOW_COPY_AND_ASSIGN(RouterWindowProxy);
};

} // namespace router

#endif // ROUTER__MANAGER__ROUTER_WINDOW_PROXY_H
