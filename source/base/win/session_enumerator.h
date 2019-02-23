//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__WIN__SESSION_ENUMERATOR_H
#define BASE__WIN__SESSION_ENUMERATOR_H

#include <cstdint>
#include <string>

#include "base/macros_magic.h"
#include "base/win/scoped_wts_memory.h"
#include "base/win/session_id.h"

namespace base::win {

class SessionEnumerator
{
public:
    SessionEnumerator();
    ~SessionEnumerator();

    void advance();
    bool isAtEnd() const;

    enum class State
    {
        UNKNOWN       = 0, // Unknown state.
        ACTIVE        = 1, // A user is logged on to the WinStation.
        CONNECTED     = 2, // The WinStation is connected to the client.
        CONNECT_QUERY = 3, // The WinStation is in the process of connecting to the client.
        SHADOW        = 4, // The WinStation is shadowing another WinStation.
        DISCONNECTED  = 5, // The WinStation is active but the client is disconnected.
        IDLE          = 6, // The WinStation is waiting for a client to connect.
        LISTEN        = 7, // The WinStation is listening for a connection. A listener session
                           // waits for requests for new client connections. No user is logged on
                           // a listener session. A listener session cannot be reset, shadowed, or
                           // changed to a regular client session.
        RESET         = 8, // The WinStation is being reset.
        DOWN          = 9, // The WinStation is down due to an error.
        INIT          = 10 // The WinStation is initializing.
    };

    State state() const;
    SessionId sessionId() const;

    // A string that contains the name of this session. For example, "services", "console",
    // or "RDP-Tcp#0".
    std::string sessionName() const;

    // A string that contains the name of the computer that the session is running on.
    std::string hostName() const;

    // A string that contains the name of the user who is logged on to the session.
    // If no user is logged on to the session, the string is empty.
    std::string userName() const;

    // A string that contains the domain name of the user who is logged on to the session.
    // If no user is logged on to the session, the string is empty.
    std::string domainName() const;

    // A string that contains the name of the farm that the virtual machine is joined to.
    // If the session is not running on a virtual machine that is joined to a farm, the string
    // is empty.
    std::string farmName() const;

private:
    base::win::ScopedWtsMemory<PWTS_SESSION_INFO_1W> info_;
    DWORD count_ = 0;
    DWORD current_ = 0;

    DISALLOW_COPY_AND_ASSIGN(SessionEnumerator);
};

} // namespace base::win

#endif // BASE__WIN__SESSION_ENUMERATOR_H
