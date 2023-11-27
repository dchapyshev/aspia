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

#include "client/client_system_info.h"

#include "base/logging.h"
#include "client/system_info_control_proxy.h"
#include "client/system_info_window_proxy.h"
#include "proto/system_info.pb.h"

namespace client {

//--------------------------------------------------------------------------------------------------
ClientSystemInfo::ClientSystemInfo(std::shared_ptr<base::TaskRunner> io_task_runner)
    : Client(io_task_runner),
      system_info_control_proxy_(std::make_shared<SystemInfoControlProxy>(io_task_runner, this))
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientSystemInfo::~ClientSystemInfo()
{
    LOG(LS_INFO) << "Dtor";
    system_info_control_proxy_->dettach();
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::setSystemInfoWindow(
    std::shared_ptr<SystemInfoWindowProxy> system_info_window_proxy)
{
    LOG(LS_INFO) << "System info window installed";
    system_info_window_proxy_ = std::move(system_info_window_proxy);
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request)
{
    sendMessage(proto::HOST_CHANNEL_ID_SESSION, request);
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::onSessionStarted(const base::Version& /* peer_version */)
{
    LOG(LS_INFO) << "System info session started";

    system_info_window_proxy_->start(system_info_control_proxy_);
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::onSessionMessageReceived(
    uint8_t /* channel_id */, const base::ByteArray& buffer)
{
    proto::system_info::SystemInfo system_info;

    if (!base::parse(buffer, &system_info))
    {
        LOG(LS_ERROR) << "Unable to parse system info";
        return;
    }

    system_info_window_proxy_->setSystemInfo(system_info);
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::onSessionMessageWritten(uint8_t /* channel_id */, size_t /* pending */)
{
    // Nothing
}

} // namespace client
