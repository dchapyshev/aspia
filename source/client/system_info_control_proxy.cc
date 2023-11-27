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

#include "client/system_info_control_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "proto/system_info.pb.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SystemInfoControlProxy::SystemInfoControlProxy(std::shared_ptr<base::TaskRunner> io_task_runner,
                                               SystemInfoControl* system_info_control)
    : io_task_runner_(std::move(io_task_runner)),
      system_info_control_(system_info_control)
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(io_task_runner_);
    DCHECK(system_info_control_);
}

//--------------------------------------------------------------------------------------------------
SystemInfoControlProxy::~SystemInfoControlProxy()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(!system_info_control_);
}

//--------------------------------------------------------------------------------------------------
void SystemInfoControlProxy::dettach()
{
    LOG(LS_INFO) << "Dettach system info control";
    DCHECK(io_task_runner_->belongsToCurrentThread());
    system_info_control_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void SystemInfoControlProxy::onSystemInfoRequest(
    const proto::system_info::SystemInfoRequest& request)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&SystemInfoControlProxy::onSystemInfoRequest, shared_from_this(), request));
        return;
    }

    if (system_info_control_)
        system_info_control_->onSystemInfoRequest(request);
}

} // namespace client
