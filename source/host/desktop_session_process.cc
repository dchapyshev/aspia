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

#include "host/desktop_session_process.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/win/scoped_impersonator.h"

#include <userenv.h>

namespace host {

namespace {

// Name of the default session desktop.
const char16_t kDefaultDesktopName[] = u"winsta0\\default";
const char16_t kDesktopAgentFile[] = u"aspia_desktop_agent.exe";

bool copyProcessToken(DWORD desired_access, base::win::ScopedHandle* token_out)
{
    base::win::ScopedHandle process_token;

    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_DUPLICATE | desired_access,
                          process_token.recieve()))
    {
        PLOG(LS_WARNING) << "OpenProcessToken failed";
        return false;
    }

    if (!DuplicateTokenEx(process_token,
                          desired_access,
                          nullptr,
                          SecurityImpersonation,
                          TokenPrimary,
                          token_out->recieve()))
    {
        PLOG(LS_WARNING) << "DuplicateTokenEx failed";
        return false;
    }

    return true;
}

bool createPrivilegedToken(base::win::ScopedHandle* token_out)
{
    base::win::ScopedHandle privileged_token;
    const DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
        TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!copyProcessToken(desired_access, &privileged_token))
        return false;

    // Get the LUID for the SE_TCB_NAME privilege.
    TOKEN_PRIVILEGES state;
    state.PrivilegeCount = 1;
    state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_TCB_NAME, &state.Privileges[0].Luid))
    {
        PLOG(LS_WARNING) << "LookupPrivilegeValueW failed";
        return false;
    }

    // Enable the SE_TCB_NAME privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0, nullptr, nullptr))
    {
        PLOG(LS_WARNING) << "AdjustTokenPrivileges failed";
        return false;
    }

    token_out->reset(privileged_token.release());
    return true;
}

// Creates a copy of the current process token for the given |session_id| so
// it can be used to launch a process in that session.
bool createSessionToken(DWORD session_id, base::win::ScopedHandle* token_out)
{
    base::win::ScopedHandle session_token;
    const DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID |
        TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!copyProcessToken(desired_access, &session_token))
        return false;

    base::win::ScopedHandle privileged_token;

    if (!createPrivilegedToken(&privileged_token))
        return false;

    base::win::ScopedImpersonator impersonator;

    if (!impersonator.loggedOnUser(privileged_token))
        return false;

    // Change the session ID of the token.
    if (!SetTokenInformation(session_token, TokenSessionId, &session_id, sizeof(session_id)))
    {
        PLOG(LS_WARNING) << "SetTokenInformation failed";
        return false;
    }

    DWORD ui_access = 1;
    if (!SetTokenInformation(session_token, TokenUIAccess, &ui_access, sizeof(ui_access)))
    {
        PLOG(LS_WARNING) << "SetTokenInformation failed";
        return false;
    }

    token_out->reset(session_token.release());
    return true;
}

bool startProcessWithToken(HANDLE token,
                           const base::CommandLine& command_line,
                           base::win::ScopedHandle* process,
                           base::win::ScopedHandle* thread)
{
    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = const_cast<wchar_t*>(
        reinterpret_cast<const wchar_t*>(kDefaultDesktopName));

    void* environment = nullptr;

    if (!CreateEnvironmentBlock(&environment, token, FALSE))
    {
        PLOG(LS_WARNING) << "CreateEnvironmentBlock failed";
        return false;
    }

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    if (!CreateProcessAsUserW(token,
                              nullptr,
                              const_cast<wchar_t*>(reinterpret_cast<const wchar_t*>(
                                  command_line.commandLineString().c_str())),
                              nullptr,
                              nullptr,
                              FALSE,
                              CREATE_UNICODE_ENVIRONMENT | HIGH_PRIORITY_CLASS,
                              environment,
                              nullptr,
                              &startup_info,
                              &process_info))
    {
        PLOG(LS_WARNING) << "CreateProcessAsUserW failed";
        DestroyEnvironmentBlock(environment);
        return false;
    }

    thread->reset(process_info.hThread);
    process->reset(process_info.hProcess);

    DestroyEnvironmentBlock(environment);
    return true;
}

} // namespace

DesktopSessionProcess::DesktopSessionProcess(
    base::win::ScopedHandle&& process, base::win::ScopedHandle&& thread)
    : process_(std::move(process)),
      thread_(std::move(thread))
{
    // Nothing
}

DesktopSessionProcess::~DesktopSessionProcess() = default;

// static
std::unique_ptr<DesktopSessionProcess> DesktopSessionProcess::create(
    base::SessionId session_id, std::u16string_view channel_id)
{
    base::CommandLine command_line(filePath());
    command_line.appendSwitch(u"channel_id", channel_id);

    base::win::ScopedHandle session_token;
    if (!createSessionToken(session_id, &session_token))
        return nullptr;

    base::win::ScopedHandle process_handle;
    base::win::ScopedHandle thread_handle;

    if (!startProcessWithToken(session_token, command_line, &process_handle, &thread_handle))
        return nullptr;

    return std::unique_ptr<DesktopSessionProcess>(
        new DesktopSessionProcess(std::move(process_handle), std::move(thread_handle)));
}

// static
std::filesystem::path DesktopSessionProcess::filePath()
{
    std::filesystem::path file_path;
    if (!base::BasePaths::currentExecDir(&file_path))
        return std::filesystem::path();

    file_path.append(kDesktopAgentFile);
    return file_path;
}

void DesktopSessionProcess::kill()
{
    if (!process_.isValid())
    {
        LOG(LS_WARNING) << "Invalid process handle";
        return;
    }

    TerminateProcess(process_, 0);
}

} // namespace host
