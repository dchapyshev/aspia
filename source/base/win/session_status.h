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

#ifndef BASE_WIN_SESSION_STATUS_H
#define BASE_WIN_SESSION_STATUS_H

namespace base {

enum class SessionStatus
{
    UNKNOWN                = 0x0,
    CONSOLE_CONNECT        = 0x1,
    CONSOLE_DISCONNECT     = 0x2,
    REMOTE_CONNECT         = 0x3,
    REMOTE_DISCONNECT      = 0x4,
    SESSION_LOGON          = 0x5,
    SESSION_LOGOFF         = 0x6,
    SESSION_LOCK           = 0x7,
    SESSION_UNLOCK         = 0x8,
    SESSION_REMOTE_CONTROL = 0x9,
    SESSION_CREATE         = 0xA,
    SESSION_TERMINATE      = 0xB
};

const char* sessionStatusToString(SessionStatus status);

} // namespace base

#endif // BASE_WIN_SESSION_STATUS_H
