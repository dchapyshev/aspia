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

#include "client/system_info_window_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "client/system_info_control_proxy.h"
#include "proto/system_info.pb.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SystemInfoWindowProxy::SystemInfoWindowProxy(std::shared_ptr<base::TaskRunner> ui_task_runner,
                                             SystemInfoWindow* system_info_window)
    : ui_task_runner_(std::move(ui_task_runner)),
      system_info_window_(system_info_window)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
SystemInfoWindowProxy::~SystemInfoWindowProxy()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(!system_info_window_);
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindowProxy::dettach()
{
    LOG(LS_INFO) << "Dettach system info window";
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    system_info_window_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindowProxy::start(std::shared_ptr<SystemInfoControlProxy> system_info_control_proxy)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&SystemInfoWindowProxy::start, shared_from_this(), system_info_control_proxy));
        return;
    }

    if (system_info_window_)
        system_info_window_->start(system_info_control_proxy);
}

//--------------------------------------------------------------------------------------------------
void SystemInfoWindowProxy::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&SystemInfoWindowProxy::setSystemInfo, shared_from_this(), system_info));
        return;
    }

    if (system_info_window_)
        system_info_window_->setSystemInfo(system_info);
}

} // namespace client
