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

#include "host/user_session_process_proxy.h"

#include "base/logging.h"

namespace host {

UserSessionProcessProxy::UserSessionProcessProxy(
    std::shared_ptr<base::TaskRunner> io_task_runner, UserSessionProcess* process)
    : io_task_runner_(std::move(io_task_runner)),
      process_(process)
{
    DCHECK(io_task_runner_ && process_);
    DCHECK(io_task_runner_->belongsToCurrentThread());
}

UserSessionProcessProxy::~UserSessionProcessProxy() = default;

void UserSessionProcessProxy::dettach()
{
    DCHECK(io_task_runner_->belongsToCurrentThread());
    process_ = nullptr;
}

void UserSessionProcessProxy::updateCredentials(proto::CredentialsRequest::Type request_type)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(
            &UserSessionProcessProxy::updateCredentials, shared_from_this(), request_type));
        return;
    }

    if (process_)
        process_->updateCredentials(request_type);
}

void UserSessionProcessProxy::killClient(const std::string& uuid)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(
            &UserSessionProcessProxy::killClient, shared_from_this(), uuid));
        return;
    }

    if (process_)
        process_->killClient(uuid);
}

} // namespace host
