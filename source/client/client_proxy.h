//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_CLIENT_PROXY_H
#define CLIENT_CLIENT_PROXY_H

#include "base/macros_magic.h"
#include "client/client_config.h"

#include <memory>

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class Client;

class ClientProxy
{
public:
    ClientProxy(std::shared_ptr<base::TaskRunner> io_task_runner,
                std::unique_ptr<Client> client);
    ~ClientProxy();

    void start();
    void stop();

private:
    class Impl;
    std::shared_ptr<Impl> impl_;

    DISALLOW_COPY_AND_ASSIGN(ClientProxy);
};

} // namespace client

#endif // CLIENT_CLIENT_PROXY_H
