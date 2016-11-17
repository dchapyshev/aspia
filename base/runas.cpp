/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/runas.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/runas.h"

#include <userenv.h>
#include <psapi.h>
#include <wtsapi32.h>
#include <tlhelp32.h>
#include <string>

#include "base/logging.h"

typedef BOOL(WINAPI *WTSENUMERATEPROCESSESW)(HANDLE, DWORD, DWORD, PWTS_PROCESS_INFOW*, DWORD*);
typedef VOID(WINAPI *WTSFREEMEMORY)(PVOID);
typedef DWORD(WINAPI *WTSGETACTIVECONSOLESESSIONID)(VOID);

RunAs::RunAs(const WCHAR *service_name) :
    Service(service_name)
{
    kernel32_.reset(new ScopedNativeLibrary("kernel32.dll"));
    wtsapi32_.reset(new ScopedNativeLibrary("wtsapi32.dll"));
}

RunAs::~RunAs()
{

}

DWORD RunAs::GetWinlogonProcessId(DWORD session_id)
{
    DWORD process_id = static_cast<DWORD>(-1);

    WTSENUMERATEPROCESSESW enumerate_processes =
        reinterpret_cast<WTSENUMERATEPROCESSESW>(wtsapi32_->GetFunctionPointer("WTSEnumerateProcessesW"));

    WTSFREEMEMORY free_memory =
        reinterpret_cast<WTSFREEMEMORY>(wtsapi32_->GetFunctionPointer("WTSFreeMemory"));

    if (enumerate_processes && free_memory)
    {
        PWTS_PROCESS_INFOW process_info;
        DWORD process_count;

        if (enumerate_processes(WTS_CURRENT_SERVER_HANDLE,
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

            free_memory(process_info);
        }
    }

    return process_id;
}

HANDLE RunAs::GetWinlogonUserToken()
{
    WTSGETACTIVECONSOLESESSIONID get_active_console_session_id =
        reinterpret_cast<WTSGETACTIVECONSOLESESSIONID>(kernel32_->GetFunctionPointer("WTSGetActiveConsoleSessionId"));

    if (!get_active_console_session_id)
        return nullptr;

    HANDLE user_token = nullptr;

    DWORD session_id = get_active_console_session_id();
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

    HANDLE process = OpenProcess(MAXIMUM_ALLOWED, FALSE, process_id);
    if (process)
    {
        HANDLE process_token;

        if (OpenProcessToken(process,
                             TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY | TOKEN_DUPLICATE |
                                 TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_SESSIONID |
                                 TOKEN_READ | TOKEN_WRITE,
                             &process_token))
        {
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

            CloseHandle(process_token);
        }
        else
        {
            LOG(ERROR) << "OpenProcessToken() failed: " << GetLastError();
        }

        CloseHandle(process);
    }
    else
    {
        LOG(ERROR) << "OpenProcess() failed: " << GetLastError();
    }

    return user_token;
}

void RunAs::Worker()
{
    WCHAR module_path[MAX_PATH];

    if (GetModuleFileNameW(nullptr, module_path, _countof(module_path)))
    {
        HANDLE winlogon_user_token = GetWinlogonUserToken();

        if (winlogon_user_token)
        {
            PVOID environment;

            if (CreateEnvironmentBlock(&environment, winlogon_user_token, FALSE))
            {
                std::wstring command_line(module_path);

                command_line += L" --system";

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
                                         CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                                         environment,
                                         nullptr,
                                         &si,
                                         &pi))
                {
                    CloseHandle(pi.hThread);
                    CloseHandle(pi.hProcess);

                    LOG(INFO) << "Process started: " << command_line.c_str();
                }
                else
                {
                    LOG(ERROR) << "CreateProcessAsUserW() failed: " << GetLastError();
                }

                DestroyEnvironmentBlock(environment);
            }

            CloseHandle(winlogon_user_token);
        }
    }
}

void RunAs::OnStart()
{

}

void RunAs::OnStop()
{

}
