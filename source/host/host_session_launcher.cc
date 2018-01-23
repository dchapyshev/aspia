//
// PROJECT:         Aspia
// FILE:            host/host_session_launcher.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_launcher.h"
#include "host/host_session_launcher_service.h"
#include "base/process/process_helpers.h"
#include "base/command_line.h"
#include "base/scoped_native_library.h"
#include "base/scoped_object.h"
#include "base/scoped_impersonator.h"
#include "base/files/base_paths.h"
#include "base/logging.h"
#include "command_line_switches.h"

#include <userenv.h>
#include <string>

namespace aspia {

namespace {

// Name of the default session desktop.
constexpr WCHAR kDefaultDesktopName[] = L"winsta0\\default";

bool CopyProcessToken(DWORD desired_access, ScopedHandle& token_out)
{
    ScopedHandle process_token;

    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_DUPLICATE | desired_access,
                          process_token.Recieve()))
    {
        PLOG(LS_ERROR) << "OpenProcessToken() failed";
        return false;
    }

    if (!DuplicateTokenEx(process_token,
                          desired_access,
                          nullptr,
                          SecurityImpersonation,
                          TokenPrimary,
                          token_out.Recieve()))
    {
        PLOG(LS_ERROR) << "DuplicateTokenEx() failed";
        return false;
    }

    return true;
}

// Creates a copy of the current process with SE_TCB_NAME privilege enabled.
bool CreatePrivilegedToken(ScopedHandle& token_out)
{
    ScopedHandle privileged_token;
    const DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
        TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!CopyProcessToken(desired_access, privileged_token))
        return false;

    // Get the LUID for the SE_TCB_NAME privilege.
    TOKEN_PRIVILEGES state;
    state.PrivilegeCount = 1;
    state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_TCB_NAME, &state.Privileges[0].Luid))
    {
        PLOG(LS_ERROR) << "LookupPrivilegeValueW() failed";
        return false;
    }

    // Enable the SE_TCB_NAME privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0,
                               nullptr, nullptr))
    {
        PLOG(LS_ERROR) << "AdjustTokenPrivileges() failed";
        return false;
    }

    token_out.Reset(privileged_token.Release());
    return true;
}

// Creates a copy of the current process token for the given |session_id| so
// it can be used to launch a process in that session.
bool CreateSessionToken(uint32_t session_id, ScopedHandle& token_out)
{
    ScopedHandle session_token;
    const DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID |
        TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!CopyProcessToken(desired_access, session_token))
        return false;

    ScopedHandle privileged_token;

    if (!CreatePrivilegedToken(privileged_token))
        return false;

    {
        ScopedImpersonator impersonator;

        if (!impersonator.ImpersonateLoggedOnUser(privileged_token))
            return false;

        // Change the session ID of the token.
        DWORD new_session_id = session_id;
        if (!SetTokenInformation(session_token,
                                 TokenSessionId,
                                 &new_session_id,
                                 sizeof(new_session_id)))
        {
            PLOG(LS_ERROR) << "SetTokenInformation() failed";
            return false;
        }
    }

    token_out.Reset(session_token.Release());
    return true;
}

bool CreateProcessWithToken(HANDLE user_token, const CommandLine& command_line)
{
    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = const_cast<WCHAR*>(kDefaultDesktopName);

    PVOID environment = nullptr;

    if (!CreateEnvironmentBlock(&environment, user_token, FALSE))
    {
        PLOG(LS_ERROR) << "CreateEnvironmentBlock() failed";
        return false;
    }

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    if (!CreateProcessAsUserW(user_token,
                              nullptr,
                              const_cast<LPWSTR>(command_line.GetCommandLineString().c_str()),
                              nullptr,
                              nullptr,
                              FALSE,
                              CREATE_UNICODE_ENVIRONMENT,
                              environment,
                              nullptr,
                              &startup_info,
                              &process_info))
    {
        PLOG(LS_ERROR) << "CreateProcessAsUserW() failed";
        DestroyEnvironmentBlock(environment);
        return false;
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);

    DestroyEnvironmentBlock(environment);

    return true;
}

bool LaunchProcessWithCurrentRights(const std::wstring& run_mode, const std::wstring& channel_id)
{
    std::experimental::filesystem::path program_path;

    if (!GetBasePath(BasePathKey::FILE_EXE, program_path))
        return false;

    CommandLine command_line(program_path);

    command_line.AppendSwitch(kRunModeSwitch, run_mode);
    command_line.AppendSwitch(kChannelIdSwitch, channel_id);

    ScopedHandle token;

    if (!CopyProcessToken(TOKEN_ALL_ACCESS, token))
        return false;

    return CreateProcessWithToken(token, command_line);
}

bool LaunchSessionProcess(const std::wstring& run_mode,
                          uint32_t session_id,
                          const std::wstring& channel_id)
{
    if (IsRunningAsService())
    {
        ScopedHandle privileged_token;

        if (!CreatePrivilegedToken(privileged_token))
            return false;

        ScopedNativeLibrary wtsapi32_library(L"wtsapi32.dll");

        typedef BOOL(WINAPI * WTSQueryUserTokenFunc)(ULONG, PHANDLE);

        WTSQueryUserTokenFunc query_user_token_func =
            reinterpret_cast<WTSQueryUserTokenFunc>(
                wtsapi32_library.GetFunctionPointer("WTSQueryUserToken"));

        if (!query_user_token_func)
        {
            LOG(LS_ERROR) << "WTSQueryUserToken function not found in wtsapi32.dll";
            return false;
        }

        ScopedHandle session_token;

        {
            ScopedImpersonator impersonator;

            if (!impersonator.ImpersonateLoggedOnUser(privileged_token))
                return false;

            if (!query_user_token_func(session_id, session_token.Recieve()))
            {
                PLOG(LS_ERROR) << "WTSQueryUserToken() failed";
                return false;
            }
        }

        std::experimental::filesystem::path program_path;

        if (!GetBasePath(BasePathKey::FILE_EXE, program_path))
            return false;

        CommandLine command_line(program_path);

        command_line.AppendSwitch(kRunModeSwitch, run_mode);
        command_line.AppendSwitch(kChannelIdSwitch, channel_id);

        return CreateProcessWithToken(session_token, command_line);
    }

    return LaunchProcessWithCurrentRights(run_mode, channel_id);
}

} // namespace

bool LaunchSessionProcessFromService(const std::wstring& run_mode,
                                     uint32_t session_id,
                                     const std::wstring& channel_id)
{
    std::experimental::filesystem::path program_path;

    if (!GetBasePath(BasePathKey::FILE_EXE, program_path))
        return false;

    CommandLine command_line(program_path);

    command_line.AppendSwitch(kRunModeSwitch, run_mode);
    command_line.AppendSwitch(kChannelIdSwitch, channel_id);

    ScopedHandle session_token;

    if (!CreateSessionToken(session_id, session_token))
        return false;

    return CreateProcessWithToken(session_token, command_line);
}

bool LaunchSessionProcess(proto::auth::SessionType session_type,
                          uint32_t session_id,
                          const std::wstring& channel_id)
{
    switch (session_type)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
        case proto::auth::SESSION_TYPE_SYSTEM_INFO:
        {
            const WCHAR* launcher_mode = kRunModeDesktopSession;

            if (session_type == proto::auth::SESSION_TYPE_SYSTEM_INFO)
                launcher_mode = kRunModeSystemInfoSession;

            if (!IsCallerHasAdminRights())
            {
                return LaunchProcessWithCurrentRights(launcher_mode, channel_id);
            }

            if (!IsRunningAsService())
            {
                return HostSessionLauncherService::CreateStarted(launcher_mode,
                                                                 session_id,
                                                                 channel_id);
            }

            // The code is executed from the service.
            // Start the process directly.
            return LaunchSessionProcessFromService(launcher_mode, session_id, channel_id);
        }

        case proto::auth::SESSION_TYPE_FILE_TRANSFER:
        {
            return LaunchSessionProcess(kRunModeFileTransferSession, session_id, channel_id);
        }

        case proto::auth::SESSION_TYPE_POWER_MANAGE:
        {
            return LaunchSessionProcess(kRunModePowerManageSession, session_id, channel_id);
        }

        default:
        {
            DLOG(LS_ERROR) << "Unknown session type: " << session_type;
            return false;
        }
    }
}

bool LaunchSystemInfoProcess()
{
    std::experimental::filesystem::path program_path;

    if (!GetBasePath(BasePathKey::FILE_EXE, program_path))
        return false;

    CommandLine command_line(program_path);
    command_line.AppendSwitch(kRunModeSwitch, kRunModeSystemInfo);

    ScopedHandle token;

    if (!CopyProcessToken(TOKEN_ALL_ACCESS, token))
        return false;

    return CreateProcessWithToken(token, command_line);
}

} // namespace aspia
