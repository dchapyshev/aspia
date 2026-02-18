//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/client_system_info.h"

#include "base/logging.h"
#include "base/serialization.h"

#if defined(Q_OS_WINDOWS)
#include "host/system_info.h"
#endif // defined(Q_OS_WINDOWS)

namespace host {

//--------------------------------------------------------------------------------------------------
ClientSystemInfo::ClientSystemInfo(base::TcpChannel* channel, QObject* parent)
    : Client(channel, parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientSystemInfo::~ClientSystemInfo()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::onStarted()
{
    LOG(INFO) << "Session started";
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::onReceived(const QByteArray& buffer)
{
#if defined(Q_OS_WINDOWS)
    proto::system_info::SystemInfoRequest request;

    if (!base::parse(buffer, &request))
    {
        LOG(ERROR) << "Unable to parse system info request";
        return;
    }

    proto::system_info::SystemInfo system_info;
    createSystemInfo(request, &system_info);

    sendMessage(base::serialize(system_info));
#endif // defined(Q_OS_WINDOWS)
}

} // namespace host
