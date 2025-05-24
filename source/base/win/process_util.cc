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

#include "base/win/process_util.h"

#include "base/logging.h"

#include <qt_windows.h>
#include <TlHelp32.h>
#include <shellapi.h>

namespace base {

//--------------------------------------------------------------------------------------------------
bool isProcessElevated()
{
    ScopedHandle token;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.recieve()))
    {
        PLOG(LS_ERROR) << "OpenProcessToken failed";
        return false;
    }

    TOKEN_ELEVATION elevation;
    DWORD size;

    if (!GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size))
    {
        PLOG(LS_ERROR) << "GetTokenInformation failed";
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
        PLOG(LS_ERROR) << "ShellExecuteExW failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool copyProcessToken(DWORD desired_access, ScopedHandle* token_out)
{
    ScopedHandle process_token;

    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_DUPLICATE | desired_access,
                          process_token.recieve()))
    {
        PLOG(LS_ERROR) << "OpenProcessToken failed";
        return false;
    }

    if (!DuplicateTokenEx(process_token,
                          desired_access,
                          nullptr,
                          SecurityImpersonation,
                          TokenPrimary,
                          token_out->recieve()))
    {
        PLOG(LS_ERROR) << "DuplicateTokenEx failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool createPrivilegedToken(ScopedHandle* token_out)
{
    ScopedHandle privileged_token;
    const DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
        TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!copyProcessToken(desired_access, &privileged_token))
    {
        LOG(LS_ERROR) << "copyProcessToken failed";
        return false;
    }

    // Get the LUID for the SE_TCB_NAME privilege.
    TOKEN_PRIVILEGES state;
    state.PrivilegeCount = 1;
    state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_TCB_NAME, &state.Privileges[0].Luid))
    {
        PLOG(LS_ERROR) << "LookupPrivilegeValueW failed";
        return false;
    }

    // Enable the SE_TCB_NAME privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0, nullptr, nullptr))
    {
        PLOG(LS_ERROR) << "AdjustTokenPrivileges failed";
        return false;
    }

    token_out->reset(privileged_token.release());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool isProcessStartedFromService()
{
    ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snapshot.isValid())
    {
        PLOG(LS_ERROR) << "CreateToolhelp32Snapshot failed";
        return false;
    }

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);

    if (!Process32FirstW(snapshot, &entry))
    {
        PLOG(LS_ERROR) << "Process32FirstW failed";
        return false;
    }

    for (DWORD process_id = GetCurrentProcessId();;)
    {
        if (entry.th32ProcessID == process_id)
        {
            DWORD session_id;

            if (ProcessIdToSessionId(entry.th32ParentProcessID, &session_id))
            {
                // Services are started in session with id 0.
                return session_id == 0;
            }
            else
            {
                PLOG(LS_ERROR) << "ProcessIdToSessionId failed";
                break;
            }
        }

        if (!Process32NextW(snapshot, &entry))
        {
            PLOG(LS_ERROR) << "Process32NextW failed";
            break;
        }
    }

    return false;
}

} // namespace base
