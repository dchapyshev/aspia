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

#include "client/client_system_info.h"

#include "base/logging.h"
#include "base/serialization.h"

namespace client {

//--------------------------------------------------------------------------------------------------
ClientSystemInfo::ClientSystemInfo(QObject* parent)
    : Client(parent)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientSystemInfo::~ClientSystemInfo()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request)
{
    sendMessage(proto::HOST_CHANNEL_ID_SESSION, request);
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::onSessionStarted()
{
    LOG(LS_INFO) << "System info session started";
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::onSessionMessageReceived(const QByteArray& buffer)
{
    proto::system_info::SystemInfo system_info;

    if (!base::parse(buffer, &system_info))
    {
        LOG(LS_ERROR) << "Unable to parse system info";
        return;
    }

    emit sig_systemInfo(system_info);
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::onSessionMessageWritten(size_t /* pending */)
{
    // Nothing
}

} // namespace client
