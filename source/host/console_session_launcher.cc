//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/console_session_launcher.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/console_session_launcher.h"

#include <userenv.h>
#include <string>

#include "base/process/process_helpers.h"
#include "base/service_manager.h"
#include "base/scoped_native_library.h"
#include "base/scoped_object.h"
#include "base/scoped_impersonator.h"
#include "base/files/base_paths.h"
#include "base/logging.h"

namespace aspia {

static const WCHAR kServiceShortName[] = L"aspia-desktop-session-launcher";
static const WCHAR kServiceFullName[] = L"Aspia Desktop Session Launcher";

// Name of the default session desktop.
static WCHAR kDefaultDesktopName[] = L"winsta0\\default";

ConsoleSessionLauncher::ConsoleSessionLauncher(const std::wstring& service_id)
    : Service(ServiceManager::CreateUniqueServiceName(
          kServiceShortName, service_id))
{
    // Nothing
}

static bool CopyProcessToken(DWORD desired_access, ScopedHandle& token_out)
{
    ScopedHandle process_token;

    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_DUPLICATE | desired_access,
                          process_token.Recieve()))
    {
        LOG(ERROR) << "OpenProcessToken() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    if (!DuplicateTokenEx(process_token,
                          desired_access,
                          nullptr,
                          SecurityImpersonation,
                          TokenPrimary,
                          token_out.Recieve()))
    {
        LOG(ERROR) << "DuplicateTokenEx() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    return true;
}

// Creates a copy of the current process with SE_TCB_NAME privilege enabled.
static bool CreatePrivilegedToken(ScopedHandle& token_out)
{
    ScopedHandle privileged_token;
    DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
        TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!CopyProcessToken(desired_access, privileged_token))
        return false;

    // Get the LUID for the SE_TCB_NAME privilege.
    TOKEN_PRIVILEGES state;
    state.PrivilegeCount = 1;
    state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_TCB_NAME,
                               &state.Privileges[0].Luid))
    {
        LOG(ERROR) << "LookupPrivilegeValueW() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    // Enable the SE_TCB_NAME privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0,
                               nullptr, nullptr))
    {
        LOG(ERROR) << "AdjustTokenPrivileges() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    token_out.Reset(privileged_token.Release());
    return true;
}

// Creates a copy of the current process token for the given |session_id| so
// it can be used to launch a process in that session.
static bool CreateSessionToken(uint32_t session_id, ScopedHandle& token_out)
{
    ScopedHandle session_token;
    DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID |
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
            LOG(ERROR) << "SetTokenInformation() failed: "
                       << GetLastSystemErrorString();
            return false;
        }
    }

    token_out.Reset(session_token.Release());
    return true;
}

static bool CreateProcessWithToken(HANDLE user_token,
                                   const std::wstring& command_line)
{
    PROCESS_INFORMATION process_info = { 0 };
    STARTUPINFOW startup_info = { 0 };

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = kDefaultDesktopName;

    PVOID environment;

    if (!CreateEnvironmentBlock(&environment, user_token, FALSE))
    {
        LOG(ERROR) << "CreateEnvironmentBlock() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    if (!CreateProcessAsUserW(user_token,
                              nullptr,
                              const_cast<LPWSTR>(command_line.c_str()),
                              nullptr,
                              nullptr,
                              FALSE,
                              CREATE_UNICODE_ENVIRONMENT,
                              environment,
                              nullptr,
                              &startup_info,
                              &process_info))
    {
        LOG(ERROR) << "CreateProcessAsUserW() failed: "
                   << GetLastSystemErrorString();
        DestroyEnvironmentBlock(environment);
        return false;
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);

    DestroyEnvironmentBlock(environment);

    return true;
}

static bool CreateCommandLine(const std::wstring& run_mode,
                              const std::wstring& channel_id,
                              std::wstring& command_line)
{
    FilePath path;

    if (!GetBasePath(BasePathKey::FILE_EXE, path))
        return false;

    command_line.assign(path);
    command_line.append(L" --run_mode=");
    command_line.append(run_mode);
    command_line.append(L" --channel_id=");
    command_line.append(channel_id);

    return true;
}

static bool LaunchProcessInSession(const std::wstring& run_mode,
                                   uint32_t session_id,
                                   const std::wstring& channel_id)
{
    std::wstring command_line;

    if (!CreateCommandLine(run_mode, channel_id, command_line))
        return false;

    ScopedHandle session_token;

    if (!CreateSessionToken(session_id, session_token))
        return false;

    return CreateProcessWithToken(session_token, command_line);
}

static bool LaunchProcessWithCurrentRights(const std::wstring& run_mode,
                                           const std::wstring& channel_id)
{
    std::wstring command_line;

    if (!CreateCommandLine(run_mode, channel_id, command_line))
        return false;

    ScopedHandle token;

    if (!CopyProcessToken(TOKEN_ALL_ACCESS, token))
        return false;

    return CreateProcessWithToken(token, command_line);
}

static bool LaunchProcessOverService(uint32_t session_id,
                                     const std::wstring& channel_id)
{
    std::wstring service_id =
        ServiceManager::GenerateUniqueServiceId();

    std::wstring unique_short_name =
        ServiceManager::CreateUniqueServiceName(kServiceShortName, service_id);

    std::wstring unique_full_name =
        ServiceManager::CreateUniqueServiceName(kServiceFullName, service_id);

    FilePath path;

    if (!GetBasePath(BasePathKey::FILE_EXE, path))
        return false;

    std::wstring command_line;

    command_line.assign(path);
    command_line.append(L" --channel_id=");
    command_line.append(channel_id);
    command_line.append(L" --run_mode=");
    command_line.append(kDesktopSessionLauncherSwitch);
    command_line.append(L" --session_id=");
    command_line.append(std::to_wstring(session_id));
    command_line.append(L" --service_id=");
    command_line.append(service_id);

    // Install the service in the system.
    std::unique_ptr<ServiceManager> manager =
        ServiceManager::Create(command_line,
                               unique_full_name,
                               unique_short_name);

    // If the service was not installed.
    if (!manager)
        return false;

    return manager->Start();
}

void ConsoleSessionLauncher::Worker()
{
    LaunchProcessInSession(kDesktopSessionSwitch,
                           session_id_,
                           channel_id_);
}

void ConsoleSessionLauncher::OnStop()
{
    // Nothing
}

void ConsoleSessionLauncher::ExecuteService(uint32_t session_id,
                                            const std::wstring& channel_id)
{
    session_id_ = session_id;
    channel_id_ = channel_id;

    Run();

    ServiceManager(ServiceName()).Remove();
}

bool LaunchDesktopSession(uint32_t session_id, const std::wstring& channel_id)
{
    if (!IsCallerHasAdminRights())
    {
        return LaunchProcessWithCurrentRights(kDesktopSessionSwitch,
                                              channel_id);
    }

    if (!IsRunningAsService())
    {
        return LaunchProcessOverService(session_id, channel_id);
    }

    // The code is executed from the service.
    // Start the process directly.
    return LaunchProcessInSession(kDesktopSessionSwitch,
                                  session_id,
                                  channel_id);
}

bool LaunchFileTransferSession(uint32_t session_id,
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
            LOG(ERROR) << "WTSQueryUserToken function not found in wtsapi32.dll";
            return false;
        }

        ScopedHandle session_token;

        {
            ScopedImpersonator impersonator;

            if (!impersonator.ImpersonateLoggedOnUser(privileged_token))
                return false;

            if (!query_user_token_func(session_id,
                                       session_token.Recieve()))
            {
                LOG(ERROR) << "WTSQueryUserToken() failed: "
                           << GetLastSystemErrorString();
                return false;
            }
        }

        std::wstring command_line;

        if (!CreateCommandLine(kFileTransferSessionSwitch,
                               channel_id, command_line))
            return false;

        return CreateProcessWithToken(session_token, command_line);
    }

    return LaunchProcessWithCurrentRights(kFileTransferSessionSwitch,
                                          channel_id);
}

} // namespace aspia
