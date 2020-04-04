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

UserSessionAgentProxy::UserSessionAgentProxy(
    std::shared_ptr<base::TaskRunner> io_task_runner, std::unique_ptr<UserSessionAgent> agent)
    : io_task_runner_(std::move(io_task_runner)),
      agent_(std::move(agent))
{
    DCHECK(io_task_runner_ && agent_);
    DCHECK(io_task_runner_->belongsToCurrentThread());
}

UserSessionAgentProxy::~UserSessionAgentProxy()
{
    stop();
}

void UserSessionAgentProxy::start()
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&UserSessionAgentProxy::start, shared_from_this()));
        return;
    }

    if (agent_)
        agent_->start();
}

void UserSessionAgentProxy::stop()
{
    std::unique_lock lock(agent_lock_);
    io_task_runner_->deleteSoon(std::move(agent_));
}

void UserSessionAgentProxy::updateCredentials(
    proto::internal::CredentialsRequest::Type request_type)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(
            &UserSessionAgentProxy::updateCredentials, shared_from_this(), request_type));
        return;
    }

    std::shared_lock lock(agent_lock_);
    if (agent_)
        agent_->updateCredentials(request_type);
}

void UserSessionAgentProxy::killClient(const std::string& uuid)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(
            &UserSessionAgentProxy::killClient, shared_from_this(), uuid));
        return;
    }

    std::shared_lock lock(agent_lock_);
    if (agent_)
        agent_->killClient(uuid);
}

} // namespace host
