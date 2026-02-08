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

#include "base/session_id.h"

#include <limits>
#include <type_traits>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#endif // defined(Q_OS_WINDOWS)

namespace base {

#if defined(Q_OS_WINDOWS)

static_assert(std::is_same<SessionId, DWORD>());
static_assert(kInvalidSessionId == std::numeric_limits<DWORD>::max());

//--------------------------------------------------------------------------------------------------
SessionId activeConsoleSessionId()
{
    return WTSGetActiveConsoleSessionId();
}

#endif // defined(Q_OS_WINDOWS)

} // namespace base
