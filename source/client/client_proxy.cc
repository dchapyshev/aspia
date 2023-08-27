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

#include "client/client_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "client/client.h"

namespace client {

class ClientProxy::Impl : public std::enable_shared_from_this<Impl>
{
public:
    Impl(std::shared_ptr<base::TaskRunner> io_task_runner, std::unique_ptr<Client> client);
    ~Impl();

    void start(const Config& config);
    void stop();

private:
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    std::unique_ptr<Client> client_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

//--------------------------------------------------------------------------------------------------
ClientProxy::Impl::Impl(std::shared_ptr<base::TaskRunner> io_task_runner,
                        std::unique_ptr<Client> client)
    : io_task_runner_(std::move(io_task_runner)),
      client_(std::move(client))
{
    DCHECK(io_task_runner_ && client_);
}

//--------------------------------------------------------------------------------------------------
ClientProxy::Impl::~Impl()
{
    DCHECK(!client_);
}

//--------------------------------------------------------------------------------------------------
void ClientProxy::Impl::start(const Config& config)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::start, shared_from_this(), config));
        return;
    }

    if (client_)
        client_->start(config);
}

//--------------------------------------------------------------------------------------------------
void ClientProxy::Impl::stop()
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::stop, shared_from_this()));
        return;
    }

    if (client_)
    {
        client_->stop();
        client_.reset();
    }
}

//--------------------------------------------------------------------------------------------------
ClientProxy::ClientProxy(std::shared_ptr<base::TaskRunner> io_task_runner,
                         std::unique_ptr<Client> client,
                         const Config& config)
    : impl_(std::make_shared<Impl>(std::move(io_task_runner), std::move(client))),
      config_(config)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ClientProxy::~ClientProxy()
{
    impl_->stop();
}

//--------------------------------------------------------------------------------------------------
void ClientProxy::start()
{
    impl_->start(config_);
}

//--------------------------------------------------------------------------------------------------
void ClientProxy::stop()
{
    impl_->stop();
}

} // namespace client
