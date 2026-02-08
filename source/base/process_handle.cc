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

#include "base/process_handle.h"

namespace base {

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
ProcessHandle currentProcessHandle()
{
    return GetCurrentProcess();
}
#endif

//--------------------------------------------------------------------------------------------------
ProcessId currentProcessId()
{
#if defined(Q_OS_WINDOWS)
    return GetCurrentProcessId();
#elif defined(Q_OS_UNIX)
    return getpid();
#else
#error Platfrom support not implemented
#endif
}

} // namespace base
