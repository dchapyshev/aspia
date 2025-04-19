//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/port_forwarding_window_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"

namespace client {

//--------------------------------------------------------------------------------------------------
PortForwardingWindowProxy::PortForwardingWindowProxy(
    std::shared_ptr<base::TaskRunner> ui_task_runner,
    PortForwardingWindow* port_forwarding_window)
    : ui_task_runner_(std::move(ui_task_runner)),
      port_forwarding_window_(port_forwarding_window)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
PortForwardingWindowProxy::~PortForwardingWindowProxy()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(!port_forwarding_window_);
}

//--------------------------------------------------------------------------------------------------
void PortForwardingWindowProxy::dettach()
{
    LOG(LS_INFO) << "Dettach port forwarding window";
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    port_forwarding_window_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void PortForwardingWindowProxy::start()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&PortForwardingWindowProxy::start, shared_from_this()));
        return;
    }

    if (port_forwarding_window_)
        port_forwarding_window_->start();
}

//--------------------------------------------------------------------------------------------------
void PortForwardingWindowProxy::setStatistics(const PortForwardingWindow::Statistics& statistics)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&PortForwardingWindowProxy::setStatistics, shared_from_this(), statistics));
        return;
    }

    if (port_forwarding_window_)
        port_forwarding_window_->setStatistics(statistics);
}

} // namespace client
