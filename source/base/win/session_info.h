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

#ifndef BASE_WIN_SESSION_INFO_H
#define BASE_WIN_SESSION_INFO_H

#include "base/session_id.h"
#include "base/win/scoped_wts_memory.h"

#include <QString>

namespace base {

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
    static const char* connectStateToString(ConnectState connect_state);

    QString winStationName() const;
    QString domain() const;
    QString userName() const;
    QString clientName() const;

    qint64 connectTime() const;
    qint64 disconnectTime() const;
    qint64 lastInputTime() const;
    qint64 logonTime() const;
    qint64 currentTime() const;

    bool isUserLocked() const;

private:
    ScopedWtsMemory<WTSINFOEXW> info_;

    DISALLOW_COPY_AND_ASSIGN(SessionInfo);
};

} // namespace base

#endif // BASE_WIN_SESSION_INFO_H
