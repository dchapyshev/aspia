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

#ifndef BASE__PROCESS_HANDLE_H
#define BASE__PROCESS_HANDLE_H

#include "build/build_config.h"

#if defined(OS_WIN)
#include <Windows.h>
#endif // defined(OS_WIN)

namespace base {

#if defined(OS_WIN)
using ProcessHandle = HANDLE;
using ProcessId = DWORD;

const ProcessHandle kNullProcessHandle = nullptr;
const ProcessId kNullProcessId = 0;
#else
#error Platform support not implemented
#endif

ProcessHandle currentProcessHandle();
ProcessId currentProcessId();

} // namespace base

#endif // BASE__PROCESS_HANDLE_H
