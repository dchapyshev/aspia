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

#include "host/user_session_agent_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"

namespace host {

class UserSessionAgentProxy::Impl : public std::enable_shared_from_this<Impl>
{
public:
    Impl(std::shared_ptr<base::TaskRunner> io_task_runner,
         std::unique_ptr<UserSessionAgent> agent);
    ~Impl();

    void start();
    void stop();
    void updateCredentials(proto::internal::CredentialsRequest::Type request_type);
    void killClient(uint32_t id);

private:
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    std::unique_ptr<UserSessionAgent> agent_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

UserSessionAgentProxy::Impl::Impl(std::shared_ptr<base::TaskRunner> io_task_runner,
                                  std::unique_ptr<UserSessionAgent> agent)
    : io_task_runner_(std::move(io_task_runner)),
      agent_(std::move(agent))
{
    DCHECK(io_task_runner_ && agent_);
}

UserSessionAgentProxy::Impl::~Impl()
{
    DCHECK(!agent_);
}

void UserSessionAgentProxy::Impl::start()
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::start, shared_from_this()));
        return;
    }

    if (agent_)
        agent_->start();
}

void UserSessionAgentProxy::Impl::stop()
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::stop, shared_from_this()));
        return;
    }

    agent_.reset();
}

void UserSessionAgentProxy::Impl::updateCredentials(
    proto::internal::CredentialsRequest::Type request_type)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&Impl::updateCredentials, shared_from_this(), request_type));
        return;
    }

    if (agent_)
        agent_->updateCredentials(request_type);
}

void UserSessionAgentProxy::Impl::killClient(uint32_t id)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&Impl::killClient, shared_from_this(), id));
        return;
    }

    if (agent_)
        agent_->killClient(id);
}

UserSessionAgentProxy::UserSessionAgentProxy(std::shared_ptr<base::TaskRunner> io_task_runner,
                                             std::unique_ptr<UserSessionAgent> agent)
    : impl_(std::make_shared<Impl>(std::move(io_task_runner), std::move(agent)))
{
    // Nothing
}

UserSessionAgentProxy::~UserSessionAgentProxy()
{
    impl_->stop();
}

void UserSessionAgentProxy::start()
{
    impl_->start();
}

void UserSessionAgentProxy::stop()
{
    impl_->stop();
}

void UserSessionAgentProxy::updateCredentials(
    proto::internal::CredentialsRequest::Type request_type)
{
    impl_->updateCredentials(request_type);
}

void UserSessionAgentProxy::killClient(uint32_t id)
{
    impl_->killClient(id);
}

} // namespace host
