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

#include "client/client_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "client/client.h"

namespace client {

ClientProxy::ClientProxy(
    std::shared_ptr<base::TaskRunner> io_task_runner, std::unique_ptr<Client> client)
    : io_task_runner_(std::move(io_task_runner)),
      client_(std::move(client))
{
    DCHECK(io_task_runner_ && client_);
}

ClientProxy::~ClientProxy() = default;

void ClientProxy::start(const Config& config)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&ClientProxy::start, shared_from_this(), config));
        return;
    }

    if (client_)
        client_->start(config);
}

void ClientProxy::stop()
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&ClientProxy::stop, shared_from_this()));
        return;
    }

    std::unique_lock lock(client_lock_);
    client_.reset();
}

Config ClientProxy::config() const
{
    Config config;

    std::shared_lock lock(client_lock_);
    if (client_)
        config = client_->config();

    return config;
}

} // namespace client
