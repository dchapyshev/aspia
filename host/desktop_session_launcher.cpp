//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/desktop_session_launcher.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/desktop_session_launcher.h"

#include <userenv.h>
#include <wtsapi32.h>
#include <string>

#include "base/service_manager.h"
#include "base/scoped_handle.h"
#include "base/scoped_wts_memory.h"
#include "base/scoped_impersonator.h"
#include "base/object_watcher.h"
#include "base/unicode.h"
#include "base/path.h"
#include "base/process.h"
#include "base/logging.h"

namespace aspia {

static const WCHAR kServiceShortName[] = L"aspia-desktop-session-launcher";
static const WCHAR kServiceFullName[] = L"Aspia Desktop Session Launcher";

// Name of the default session desktop.
static WCHAR kDefaultDesktopName[] = L"winsta0\\default";

static const uint32_t kInvalidSessionId = 0xFFFFFFFF;

DesktopSessionLauncher::DesktopSessionLauncher(const std::wstring& service_id) :
    Service(ServiceManager::CreateUniqueServiceName(kServiceShortName, service_id)),
    session_id_(kInvalidSessionId)
{
    // Nothing
}

static bool CopyProcessToken(DWORD desired_access, ScopedHandle* token_out)
{
    ScopedHandle process_token;

    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_DUPLICATE | desired_access,
                          process_token.Recieve()))
    {
        LOG(ERROR) << "OpenProcessToken() failed: " << GetLastError();
        return false;
    }

    if (!DuplicateTokenEx(process_token,
                          desired_access,
                          nullptr,
                          SecurityImpersonation,
                          TokenPrimary,
                          token_out->Recieve()))
    {
        LOG(ERROR) << "DuplicateTokenEx() failed: " << GetLastError();
        return false;
    }

    return true;
}

// Creates a copy of the current process with SE_TCB_NAME privilege enabled.
static bool CreatePrivilegedToken(ScopedHandle* token_out)
{
    ScopedHandle privileged_token;
    DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
        TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!CopyProcessToken(desired_access, &privileged_token))
        return false;

    // Get the LUID for the SE_TCB_NAME privilege.
    TOKEN_PRIVILEGES state;
    state.PrivilegeCount = 1;
    state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_TCB_NAME, &state.Privileges[0].Luid))
    {
        LOG(ERROR) << "LookupPrivilegeValueW() failed: " << GetLastError();
        return false;
    }

    // Enable the SE_TCB_NAME privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0, nullptr, 0))
    {
        LOG(ERROR) << "AdjustTokenPrivileges() failed: " << GetLastError();
        return false;
    }

    token_out->Set(privileged_token.Release());
    return true;
}

//
// Creates a copy of the current process token for the given |session_id| so
// it can be used to launch a process in that session.
//
static bool CreateSessionToken(uint32_t session_id, ScopedHandle* token_out)
{
    ScopedHandle session_token;
    DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID |
        TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!CopyProcessToken(desired_access, &session_token))
        return false;

    ScopedHandle privileged_token;

    if (!CreatePrivilegedToken(&privileged_token))
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
            LOG(ERROR) << "SetTokenInformation() failed: " << GetLastError();
            return false;
        }
    }

    token_out->Set(session_token.Release());
    return true;
}

static bool CreateProcessWithToken(HANDLE user_token, const std::wstring& command_line)
{
    PROCESS_INFORMATION process_info = { 0 };
    STARTUPINFOW startup_info = { 0 };

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = kDefaultDesktopName;

    if (!CreateProcessAsUserW(user_token,
                              nullptr,
                              const_cast<LPWSTR>(command_line.c_str()),
                              nullptr,
                              nullptr,
                              FALSE,
                              0,
                              nullptr,
                              nullptr,
                              &startup_info,
                              &process_info))
    {
        LOG(ERROR) << "CreateProcessAsUserW() failed: " << GetLastError();
        return false;
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);

    return true;
}

static bool LaunchProcessInSession(uint32_t session_id,
                                   const std::wstring& input_channel_id,
                                   const std::wstring& output_channel_id)
{
    std::wstring command_line;

    // Получаем полный путь к исполняемому файлу.
    if (!GetPath(PathKey::FILE_EXE, &command_line))
        return false;

    command_line.append(L" --run_mode=");

    command_line.append(kDesktopSessionSwitch);

    command_line.append(L" --input_channel_id=");
    command_line.append(input_channel_id);
    command_line.append(L" --output_channel_id=");
    command_line.append(output_channel_id);

    ScopedHandle session_token;

    if (!CreateSessionToken(session_id, &session_token))
        return false;

    return CreateProcessWithToken(session_token, command_line);
}

void DesktopSessionLauncher::Worker()
{
    LaunchProcessInSession(session_id_, input_channel_id_, output_channel_id_);
}

void DesktopSessionLauncher::OnStop()
{
    // Nothing
}

void DesktopSessionLauncher::ExecuteService(uint32_t session_id,
                                            const std::wstring& input_channel_id,
                                            const std::wstring& output_channel_id)
{
    session_id_ = session_id;
    input_channel_id_ = input_channel_id;
    output_channel_id_ = output_channel_id;

    // Запускаем службу для выполнения метода Worker().
    Run();

    // Удаляем службу.
    ServiceManager(ServiceName()).Remove();
}

// static
bool DesktopSessionLauncher::LaunchSession(uint32_t session_id,
                                           const std::wstring& input_channel_id,
                                           const std::wstring& output_channel_id)
{
    Process current_process(Process::Current());

    bool is_process_elevated = current_process.IsElevated();

    std::wstring command_line;

    // Получаем полный путь к исполняемому файлу.
    if (!GetPath(PathKey::FILE_EXE, &command_line))
        return false;

    command_line.append(L" --input_channel_id=");
    command_line.append(input_channel_id);
    command_line.append(L" --output_channel_id=");
    command_line.append(output_channel_id);

    if (!is_process_elevated)
    {
        command_line.append(L" --run_mode=");
        command_line.append(kDesktopSessionSwitch);

        ScopedHandle token;

        if (!CopyProcessToken(TOKEN_ALL_ACCESS, &token))
            return false;

        return CreateProcessWithToken(token, command_line);
    }
    else
    {
        DWORD current_process_session_id;

        if (!ProcessIdToSessionId(GetCurrentProcessId(), &current_process_session_id))
        {
            LOG(WARNING) << "ProcessIdToSessionId() failed: " << GetLastError();
            return false;
        }
        //
        // Если текущий процесс запущен не в нулевой сессии (не как служба), то создаем службу
        // для запуска процесса в требуемой сессии.
        //
        if (current_process_session_id)
        {
            std::wstring service_id =
                ServiceManager::GenerateUniqueServiceId();

            std::wstring unique_name =
                ServiceManager::CreateUniqueServiceName(kServiceShortName, service_id);

            command_line.append(L" --run_mode=");
            command_line.append(kDesktopSessionLauncherSwitch);
            command_line.append(L" --session_id=");
            command_line.append(std::to_wstring(session_id));

            command_line.append(L" --service_id=");
            command_line.append(service_id);

            // Устанавливаем службу в системе.
            ServiceManager manager(ServiceManager::Create(command_line,
                                                          kServiceFullName,
                                                          unique_name));

            // Если служба не была установлена.
            if (!manager.IsValid())
                return false;

            // Запускаем ее.
            return manager.Start();
        }
        // Код выполняется из службы.
        else
        {
            // Запускаем процесс напрямую.
            return LaunchProcessInSession(session_id, input_channel_id, output_channel_id);
        }
    }
}

} // namespace aspia
