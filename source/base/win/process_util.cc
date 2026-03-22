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

#include "base/win/process_util.h"

#include "base/logging.h"
#include "base/win/scoped_object.h"

#include <qt_windows.h>
#include <shellapi.h>

namespace base {

//--------------------------------------------------------------------------------------------------
bool isProcessElevated()
{
    ScopedHandle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.recieve()))
    {
        PLOG(ERROR) << "OpenProcessToken failed";
        return false;
    }

    TOKEN_ELEVATION elevation;
    DWORD size;

    if (!GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size))
    {
        PLOG(ERROR) << "GetTokenInformation failed";
        return false;
    }

    return elevation.TokenIsElevated != 0;
}

//--------------------------------------------------------------------------------------------------
bool createProcess(const QString& program, const QString& arguments, ProcessExecuteMode mode)
{
    SHELLEXECUTEINFOW sei;
    memset(&sei, 0, sizeof(sei));

    sei.cbSize = sizeof(sei);
    sei.lpVerb = ((mode == ProcessExecuteMode::ELEVATE) ? L"runas" : L"open");
    sei.lpFile = qUtf16Printable(program);
    sei.hwnd = nullptr;
    sei.nShow = SW_SHOW;
    sei.lpParameters = qUtf16Printable(arguments);

    if (!ShellExecuteExW(&sei))
    {
        PLOG(ERROR) << "ShellExecuteExW failed";
        return false;
    }

    return true;
}

} // namespace base
