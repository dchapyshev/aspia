//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/client_session_system_info.h"

#include "base/logging.h"
#include "base/net/network_channel_proxy.h"
#include "host/system_info.h"

namespace host {

ClientSessionSystemInfo::ClientSessionSystemInfo(std::unique_ptr<base::NetworkChannel> channel)
    : ClientSession(proto::SESSION_TYPE_SYSTEM_INFO, std::move(channel))
{
    LOG(LS_INFO) << "Ctor";
}

ClientSessionSystemInfo::~ClientSessionSystemInfo()
{
    LOG(LS_INFO) << "Dtor";
}

void ClientSessionSystemInfo::onMessageReceived(const base::ByteArray& buffer)
{
    proto::system_info::SystemInfoRequest request;

    if (!base::parse(buffer, &request))
    {
        LOG(LS_WARNING) << "Unable to parse system info request";
        return;
    }

    proto::system_info::SystemInfo system_info;
    createSystemInfo(request, &system_info);

    sendMessage(base::serialize(system_info));
}

void ClientSessionSystemInfo::onMessageWritten(size_t /* pending */)
{
    // Nothing
}

void ClientSessionSystemInfo::onStarted()
{
    // Nothing
}

} // namespace host
