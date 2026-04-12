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

#include "router/session_manager.h"

#include "base/logging.h"
#include "base/serialization.h"

namespace router {

//--------------------------------------------------------------------------------------------------
SessionManager::SessionManager(base::TcpChannel* channel, QObject* parent)
    : SessionClient(channel, parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
SessionManager::~SessionManager()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SessionManager::onSessionMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == CHANNEL_ID_CLIENT)
    {
        SessionClient::onSessionMessage(channel_id, buffer);
        return;
    }

    if (channel_id != CHANNEL_ID_MANAGER)
        return;

    // TODO
}

} // namespace router
