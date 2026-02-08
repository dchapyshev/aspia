//
// SmartCafe Project
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

#include "base/win/session_status.h"

#include <qt_windows.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
bool constexpr isEqual(SessionStatus status1, DWORD status2)
{
    return static_cast<DWORD>(status1) == status2;
}

static_assert(isEqual(SessionStatus::CONSOLE_CONNECT, WTS_CONSOLE_CONNECT));
static_assert(isEqual(SessionStatus::CONSOLE_DISCONNECT, WTS_CONSOLE_DISCONNECT));
static_assert(isEqual(SessionStatus::REMOTE_CONNECT, WTS_REMOTE_CONNECT));
static_assert(isEqual(SessionStatus::REMOTE_DISCONNECT, WTS_REMOTE_DISCONNECT));
static_assert(isEqual(SessionStatus::SESSION_LOGON, WTS_SESSION_LOGON));
static_assert(isEqual(SessionStatus::SESSION_LOGOFF, WTS_SESSION_LOGOFF));
static_assert(isEqual(SessionStatus::SESSION_LOCK, WTS_SESSION_LOCK));
static_assert(isEqual(SessionStatus::SESSION_UNLOCK, WTS_SESSION_UNLOCK));
static_assert(isEqual(SessionStatus::SESSION_REMOTE_CONTROL, WTS_SESSION_REMOTE_CONTROL));
static_assert(isEqual(SessionStatus::SESSION_CREATE, WTS_SESSION_CREATE));
static_assert(isEqual(SessionStatus::SESSION_TERMINATE, WTS_SESSION_TERMINATE));

} // namespace

//--------------------------------------------------------------------------------------------------
const char* sessionStatusToString(SessionStatus status)
{
    switch (status)
    {
        case SessionStatus::CONSOLE_CONNECT:
            return "CONSOLE_CONNECT";

        case SessionStatus::CONSOLE_DISCONNECT:
            return "CONSOLE_DISCONNECT";

        case SessionStatus::REMOTE_CONNECT:
            return "REMOTE_CONNECT";

        case SessionStatus::REMOTE_DISCONNECT:
            return "REMOTE_DISCONNECT";

        case SessionStatus::SESSION_LOGON:
            return "SESSION_LOGON";

        case SessionStatus::SESSION_LOGOFF:
            return "SESSION_LOGOFF";

        case SessionStatus::SESSION_LOCK:
            return "SESSION_LOCK";

        case SessionStatus::SESSION_UNLOCK:
            return "SESSION_UNLOCK";

        case SessionStatus::SESSION_REMOTE_CONTROL:
            return "SESSION_REMOTE_CONTROL";

        case SessionStatus::SESSION_CREATE:
            return "SESSION_CREATE";

        case SessionStatus::SESSION_TERMINATE:
            return "SESSION_TERMINATE";

        default:
            return "UNKNOWN";
    }
}

} // namespace base
