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

#ifndef BASE_PROCESS_HANDLE_H
#define BASE_PROCESS_HANDLE_H

#include <QtGlobal>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#elif defined(Q_OS_UNIX)
#include <unistd.h>
#endif // defined(Q_OS_*)

namespace base {

#if defined(Q_OS_WINDOWS)
using ProcessHandle = HANDLE;
using ProcessId = DWORD;

const ProcessHandle kNullProcessHandle = nullptr;
const ProcessId kNullProcessId = 0;
#elif defined(Q_OS_UNIX)
using ProcessHandle = pid_t;
using ProcessId = pid_t;

const ProcessHandle kNullProcessHandle = 0;
const ProcessId kNullProcessId = 0;
#else
#error Platform support not implemented
#endif

#if defined(Q_OS_WINDOWS)
ProcessHandle currentProcessHandle();
#endif
ProcessId currentProcessId();

} // namespace base

#endif // BASE_PROCESS_HANDLE_H
