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

#ifndef BASE__WIN__SESSION_INFO_H
#define BASE__WIN__SESSION_INFO_H

#include "base/session_id.h"
#include "base/win/scoped_wts_memory.h"

#include <string>

namespace base::win {

class SessionInfo
{
public:
    explicit SessionInfo(SessionId session_id);
    ~SessionInfo();

    bool isValid() const;

    SessionId sessionId() const;

    enum class ConnectState
    {
        ACTIVE        = WTSActive,       // User logged on to WinStation.
        CONNECTED     = WTSConnected,    // WinStation connected to client.
        CONNECT_QUERY = WTSConnectQuery, // In the process of connecting to client.
        SHADOW        = WTSShadow,       // Shadowing another WinStation.
        DISCONNECTED  = WTSDisconnected, // WinStation logged on without client.
        IDLE          = WTSIdle,         // Waiting for client to connect.
        LISTEN        = WTSListen,       // WinStation is listening for connection.
        RESET         = WTSReset,        // WinStation is being reset.
        DOWN          = WTSDown,         // WinStation is down due to error.
        INIT          = WTSInit,         // WinStation in initialization.
    };

    ConnectState connectState() const;

    std::string winStationName() const;
    std::string domain() const;
    std::string userName() const;

    int64_t connectTime() const;
    int64_t disconnectTime() const;
    int64_t lastInputTime() const;
    int64_t logonTime() const;
    int64_t currentTime() const;

private:
    ScopedWtsMemory<WTSINFOW> info_;

    DISALLOW_COPY_AND_ASSIGN(SessionInfo);
};

} // namespace base::win

#endif // BASE__WIN__SESSION_INFO_H
