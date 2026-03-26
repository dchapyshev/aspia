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

#include "client/client_system_info.h"

#include "base/logging.h"
#include "base/serialization.h"

namespace client {

//--------------------------------------------------------------------------------------------------
ClientSystemInfo::ClientSystemInfo(QObject* parent)
    : Client(parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientSystemInfo::~ClientSystemInfo()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request)
{
    sendSessionMessage(base::serialize(request));
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::onSessionStarted()
{
    CLOG(INFO) << "System info session started";
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::onSessionMessageReceived(const QByteArray& buffer)
{
    proto::system_info::SystemInfo system_info;

    if (!base::parse(buffer, &system_info))
    {
        CLOG(ERROR) << "Unable to parse system info";
        return;
    }

    emit sig_systemInfo(system_info);
}

//--------------------------------------------------------------------------------------------------
void ClientSystemInfo::onServiceMessageReceived(const QByteArray& /* buffer */)
{
    // Not used yet.
}

} // namespace client
