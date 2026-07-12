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

#ifndef BASE_SESSION_ID_H
#define BASE_SESSION_ID_H

#include <QtSystemDetection>
#include <QtTypes>

#if defined(Q_OS_UNIX)
#include <unistd.h>
#endif

#if defined(Q_OS_WINDOWS)
using SessionId = unsigned long;
#elif defined(Q_OS_UNIX)
using SessionId = pid_t;
#else // defined(Q_OS_*)
#error Not implemented
#endif

extern const SessionId kInvalidSessionId;
extern const SessionId kServiceSessionId;

enum class ConsoleUserState
{
    UNKNOWN,
    NO_USER,
    ACTIVE
};

SessionId activeConsoleSessionId();
SessionId currentProcessSessionId();
ConsoleUserState consoleUserState();
const char* sessionStatusToString(quint32 status);

#endif // BASE_SESSION_ID_H
