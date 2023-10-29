//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/win/process.h"
#include "base/logging.h"
#include "base/win/process_util.h"
#include "base/win/scoped_impersonator.h"

#include <Psapi.h>
#include <TlHelp32.h>

namespace base::win {

namespace {

//--------------------------------------------------------------------------------------------------
// Creates a copy of the current process with |privilege_name| privilege enabled.
bool createToken(const wchar_t* privilege_name, win::ScopedHandle* token_out)
{
    const DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
        TOKEN_DUPLICATE | TOKEN_QUERY;

    win::ScopedHandle privileged_token;
    if (!copyProcessToken(desired_access, &privileged_token))
    {
        LOG(LS_ERROR) << "copyProcessToken failed";
        return false;
    }

    // Get the LUID for the privilege.
    TOKEN_PRIVILEGES state;
    state.PrivilegeCount = 1;
    state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, privilege_name, &state.Privileges[0].Luid))
    {
        PLOG(LS_ERROR) << "LookupPrivilegeValueW failed";
        return false;
    }

    // Enable the privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0, nullptr, nullptr))
    {
        PLOG(LS_ERROR) << "AdjustTokenPrivileges failed";
        return false;
    }

    token_out->reset(privileged_token.release());
    return true;
}

//--------------------------------------------------------------------------------------------------
BOOL CALLBACK terminateEnumProc(HWND hwnd, LPARAM lparam)
{
    DWORD process_id = static_cast<DWORD>(lparam);

    DWORD current_process_id = 0;
    GetWindowThreadProcessId(hwnd, &current_process_id);

    if (process_id == current_process_id)
        PostMessageW(hwnd, WM_CLOSE, 0, 0);

    return TRUE;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Process::Process(std::shared_ptr<TaskRunner> task_runner, ProcessId process_id)
    : watcher_(std::move(task_runner))
{
    // We need SE_DEBUG_NAME privilege to open the process.
    ScopedHandle privileged_token;
    if (!createToken(SE_DEBUG_NAME, &privileged_token))
    {
        LOG(LS_ERROR) << "createToken failed";
        return;
    }

    ScopedImpersonator impersonator;
    if (!impersonator.loggedOnUser(privileged_token))
    {
        LOG(LS_ERROR) << "loggedOnUser failed";
        return;
    }

    ScopedHandle process(OpenProcess(PROCESS_ALL_ACCESS, TRUE, process_id));
    if (!process.isValid())
    {
        PLOG(LS_ERROR) << "OpenProcess failed";
        return;
    }

    process_.swap(process);
}

//--------------------------------------------------------------------------------------------------
Process::Process(std::shared_ptr<TaskRunner> task_runner, HANDLE process, HANDLE thread)
    : watcher_(std::move(task_runner)),
      process_(process),
      thread_(thread)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Process::~Process()
{
    stopWatching();
}

//--------------------------------------------------------------------------------------------------
void Process::startWatching(const ExitCallback& callback)
{
    DCHECK(callback);

    callback_ = callback;
    watcher_.startWatchingOnce(process_.get(), this);
}

//--------------------------------------------------------------------------------------------------
void Process::stopWatching()
{
    watcher_.stopWatching();
}

//--------------------------------------------------------------------------------------------------
bool Process::isValid() const
{
    return process_.isValid();
}

//--------------------------------------------------------------------------------------------------
std::filesystem::path Process::filePath() const
{
    wchar_t buffer[MAX_PATH] = { 0 };

    if (!GetModuleFileNameExW(process_.get(), nullptr, buffer, static_cast<DWORD>(std::size(buffer))))
    {
        PLOG(LS_ERROR) << "GetModuleFileNameExW failed";
        return std::filesystem::path();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
std::u16string Process::fileName() const
{
    wchar_t buffer[MAX_PATH] = { 0 };

    if (GetProcessImageFileNameW(process_.get(), buffer, static_cast<DWORD>(std::size(buffer))))
    {
        PLOG(LS_ERROR) << "GetProcessImageFileNameW failed";
        return std::u16string();
    }

    return reinterpret_cast<const char16_t*>(buffer);
}

//--------------------------------------------------------------------------------------------------
ProcessId Process::processId() const
{
    return GetProcessId(process_.get());
}

//--------------------------------------------------------------------------------------------------
SessionId Process::sessionId() const
{
    DWORD session_id;

    if (!ProcessIdToSessionId(processId(), &session_id))
    {
        PLOG(LS_ERROR) << "ProcessIdToSessionId failed";
        return kInvalidSessionId;
    }

    return session_id;
}

//--------------------------------------------------------------------------------------------------
int Process::exitCode() const
{
    DWORD exit_code;

    if (!GetExitCodeProcess(process_.get(), &exit_code))
    {
        PLOG(LS_ERROR) << "GetExitCodeProcess failed";
        return 0;
    }

    return static_cast<int>(exit_code);
}

//--------------------------------------------------------------------------------------------------
void Process::kill()
{
    if (!process_.isValid())
    {
        LOG(LS_ERROR) << "Invalid process handle";
        return;
    }

    if (!TerminateProcess(process_, 0))
    {
        LOG(LS_ERROR) << "TerminateProcess failed";
    }
}

//--------------------------------------------------------------------------------------------------
void Process::terminate()
{
    if (!process_.isValid())
    {
        LOG(LS_ERROR) << "Invalid process handle";
        return;
    }

    ProcessId process_id = processId();

    if (!EnumWindows(terminateEnumProc, static_cast<LPARAM>(process_id)))
    {
        PLOG(LS_ERROR) << "EnumWindows failed";
    }

    if (!thread_.isValid())
    {
        ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, process_id));
        if (!snapshot.isValid())
        {
            PLOG(LS_ERROR) << "CreateToolhelp32Snapshot failed";
            return;
        }

        THREADENTRY32 entry;
        entry.dwSize = sizeof(entry);

        for (BOOL ret = Thread32First(snapshot, &entry);
             ret && GetLastError() != ERROR_NO_MORE_FILES;
             ret = Thread32Next(snapshot, &entry))
        {
            if (entry.th32OwnerProcessID == process_id)
            {
                if (!PostThreadMessageW(entry.th32ThreadID, WM_CLOSE, 0, 0))
                {
                    PLOG(LS_ERROR) << "PostThreadMessageW failed";
                }
            }
        }
    }
    else
    {
        if (!PostThreadMessageW(GetThreadId(thread_), WM_CLOSE, 0, 0))
        {
            PLOG(LS_ERROR) << "PostThreadMessageW failed";
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Process::onObjectSignaled(HANDLE object)
{
    DCHECK_EQ(object, process_.get());
    callback_(exitCode());
}

} // namespace base::win
