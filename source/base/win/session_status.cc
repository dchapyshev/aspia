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

#include "base/win/session_status.h"

#include <qt_windows.h>

namespace base {

//--------------------------------------------------------------------------------------------------
const char* sessionStatusToString(quint32 status)
{
    switch (status)
    {
        case WTS_CONSOLE_CONNECT:
            return "WTS_CONSOLE_CONNECT";

        case WTS_CONSOLE_DISCONNECT:
            return "WTS_CONSOLE_DISCONNECT";

        case WTS_REMOTE_CONNECT:
            return "WTS_REMOTE_CONNECT";

        case WTS_REMOTE_DISCONNECT:
            return "WTS_REMOTE_DISCONNECT";

        case WTS_SESSION_LOGON:
            return "WTS_SESSION_LOGON";

        case WTS_SESSION_LOGOFF:
            return "WTS_SESSION_LOGOFF";

        case WTS_SESSION_LOCK:
            return "WTS_SESSION_LOCK";

        case WTS_SESSION_UNLOCK:
            return "WTS_SESSION_UNLOCK";

        case WTS_SESSION_REMOTE_CONTROL:
            return "WTS_SESSION_REMOTE_CONTROL";

        case WTS_SESSION_CREATE:
            return "WTS_SESSION_CREATE";

        case WTS_SESSION_TERMINATE:
            return "WTS_SESSION_TERMINATE";

        default:
            return "UNKNOWN";
    }
}

} // namespace base
