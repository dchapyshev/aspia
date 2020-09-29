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

#ifndef CLIENT__ROUTER_PROXY_H
#define CLIENT__ROUTER_PROXY_H

#include "base/macros_magic.h"
#include "base/memory/byte_array.h"
#include "base/peer/host_id.h"

namespace base {
class TaskRunner;
} // namespace base

namespace proto {
class User;
} // namespace proto

namespace client {

class Router;

class RouterProxy
{
public:
    RouterProxy(std::shared_ptr<base::TaskRunner> io_task_runner, std::unique_ptr<Router> router);
    ~RouterProxy();

    void connectToRouter(const std::u16string& address, uint16_t port);
    void disconnectFromRouter();
    void refreshHostList();
    void disconnectHost(base::HostId host_id);
    void refreshRelayList();
    void refreshUserList();
    void addUser(const proto::User& user);
    void modifyUser(const proto::User& user);
    void deleteUser(int64_t entry_id);

private:
    class Impl;
    std::shared_ptr<Impl> impl_;

    DISALLOW_COPY_AND_ASSIGN(RouterProxy);
};

} // namespace client

#endif // CLIENT__ROUTER_PROXY_H
