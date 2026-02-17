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

#ifndef BASE_WIN_PROCESS_UTIL_H
#define BASE_WIN_PROCESS_UTIL_H

#include <QString>

#include "base/win/scoped_object.h"

namespace base {

bool isProcessElevated();

enum class ProcessExecuteMode
{
    NORMAL, ELEVATE
};

bool createProcess(
    const QString& program, const QString& arguments,
    ProcessExecuteMode mode = ProcessExecuteMode::NORMAL);

bool copyProcessToken(DWORD desired_access, ScopedHandle* token_out);

// Creates a copy of the current process with SE_TCB_NAME privilege enabled.
bool createPrivilegedToken(ScopedHandle* token_out);

bool isProcessStartedFromService();

} // namespace base

#endif // BASE_WIN_PROCESS_UTIL_H
