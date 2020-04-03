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

#ifndef CLIENT__CLIENT_PROXY_H
#define CLIENT__CLIENT_PROXY_H

#include "base/macros_magic.h"
#include "client/client_config.h"

#include <memory>
#include <shared_mutex>

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class Client;

class ClientProxy : public std::enable_shared_from_this<ClientProxy>
{
public:
    ClientProxy(std::shared_ptr<base::TaskRunner> io_task_runner, std::unique_ptr<Client> client);
    ~ClientProxy();

    void start(const Config& config);
    void stop();

    Config config() const;

private:
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    mutable std::shared_mutex client_lock_;
    std::unique_ptr<Client> client_;

    DISALLOW_COPY_AND_ASSIGN(ClientProxy);
};

} // namespace client

#endif // CLIENT__CLIENT_PROXY_H
