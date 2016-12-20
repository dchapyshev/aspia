/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/runas_service.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/runas_service.h"

#include <userenv.h>
#include <wtsapi32.h>
#include <string>

#include "base/service_control.h"
#include "base/scoped_handle.h"
#include "base/logging.h"

namespace aspia {

static const WCHAR kRunAsServiceName[] = L"aspia-runas-service";

RunAsService::RunAsService() :
    Service(kRunAsServiceName),
    kernel32_(new ScopedKernel32Library()),
    wtsapi32_(new ScopedWtsApi32Library())
{
    // Nothing
}

RunAsService::~RunAsService()
{
    // Nothing
}

DWORD RunAsService::GetWinlogonProcessId(DWORD session_id)
{
    DWORD process_id = static_cast<DWORD>(-1);

    PWTS_PROCESS_INFOW process_info;
    DWORD process_count;

    if (wtsapi32_->WTSEnumerateProcessesW(WTS_CURRENT_SERVER_HANDLE,
                                          0,
                                          1,
                                          &process_info,
                                          &process_count))
    {
        for (DWORD current = 0; current < process_count; ++current)
        {
            if (_wcsicmp(process_info[current].pProcessName, L"winlogon.exe") == 0 &&
                process_info[current].SessionId == session_id)
            {
                process_id = process_info[current].ProcessId;
                break;
            }
        }

        wtsapi32_->WTSFreeMemory(process_info);
    }

    return process_id;
}

HANDLE RunAsService::GetWinlogonUserToken()
{
    HANDLE user_token = nullptr;

    DWORD session_id = kernel32_->WTSGetActiveConsoleSessionId();
    if (session_id == 0xFFFFFFFF)
    {
        LOG(ERROR) << "WTSGetActiveConsoleSessionId() failed: " << GetLastError();
        return nullptr;
    }

    DWORD process_id = GetWinlogonProcessId(session_id);
    if (process_id == -1)
    {
        LOG(ERROR) << "Process 'winlogon.exe' not found";
        return nullptr;
    }

    ScopedHandle process(OpenProcess(MAXIMUM_ALLOWED, FALSE, process_id));

    if (process)
    {
        HANDLE handle;

        if (OpenProcessToken(process,
                             TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY | TOKEN_DUPLICATE |
                                 TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_SESSIONID |
                                 TOKEN_READ | TOKEN_WRITE,
                             &handle))
        {
            ScopedHandle process_token(handle);

            if (DuplicateTokenEx(process_token,
                                 MAXIMUM_ALLOWED,
                                 nullptr,
                                 SecurityImpersonation,
                                 TokenPrimary,
                                 &user_token))
            {
                if (!SetTokenInformation(user_token, TokenSessionId, &session_id, sizeof(session_id)))
                {
                    LOG(ERROR) << "SetTokenInformation() failed: " << GetLastError();

                    CloseHandle(user_token);
                    user_token = nullptr;
                }
            }
            else
            {
                LOG(ERROR) << "DuplicateTokenEx() failed: " << GetLastError();
            }
        }
        else
        {
            LOG(ERROR) << "OpenProcessToken() failed: " << GetLastError();
        }
    }
    else
    {
        LOG(ERROR) << "OpenProcess() failed: " << GetLastError();
    }

    return user_token;
}

void RunAsService::Worker()
{
    WCHAR module_path[MAX_PATH];

    if (GetModuleFileNameW(nullptr, module_path, ARRAYSIZE(module_path)))
    {
        ScopedHandle winlogon_user_token(GetWinlogonUserToken());

        if (winlogon_user_token)
        {
            PVOID environment;

            if (CreateEnvironmentBlock(&environment, winlogon_user_token, FALSE))
            {
                std::wstring command_line(module_path);

                command_line += L" --run_mode=system";

                PROCESS_INFORMATION pi = { 0 };
                STARTUPINFO si = { 0 };

                si.lpDesktop = L"Winsta0\\Default";
                si.cb = sizeof(STARTUPINFO);

                if (CreateProcessAsUserW(winlogon_user_token,
                                         nullptr,
                                         &command_line[0],
                                         nullptr,
                                         nullptr,
                                         FALSE,
                                         CREATE_UNICODE_ENVIRONMENT,
                                         environment,
                                         nullptr,
                                         &si,
                                         &pi))
                {
                    CloseHandle(pi.hThread);
                    CloseHandle(pi.hProcess);
                }
                else
                {
                    LOG(ERROR) << "CreateProcessAsUserW() failed: " << GetLastError();
                }

                DestroyEnvironmentBlock(environment);
            }
        }
    }
}

void RunAsService::OnStop()
{
    // Nothing
}

void RunAsService::DoService()
{
    // Запускаем службу для выполнения метода Worker().
    DoWork();

    // Удаляем службу.
    ServiceControl(kRunAsServiceName).Delete();
}

// static
bool RunAsService::InstallAndStartService()
{
    WCHAR module_path[MAX_PATH];

    // Получаем полный путь к исполняемому файлу.
    if (!GetModuleFileNameW(nullptr, module_path, ARRAYSIZE(module_path)))
    {
        LOG(ERROR) << "GetModuleFileNameW() failed: " << GetLastError();
        return false;
    }

    std::wstring command_line(module_path);

    // Добавляем флаг запуска в виде службы.
    command_line += L" --run_mode=runas";

    // Устанавливаем службу в системе.
    std::unique_ptr<ServiceControl> service_control =
        ServiceControl::Install(command_line.c_str(),
                                kRunAsServiceName,
                                kRunAsServiceName,
                                kRunAsServiceName,
                                true);

    // Если служба не была установлена.
    if (!service_control)
    {
        return false;
    }

    // Запускаем ее.
    return service_control->Start();
}

} // namespace aspia
