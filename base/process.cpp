/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/process.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/process.h"

#include <shellapi.h>
#include <stdlib.h>
#include <string>

#include "base/logging.h"

Process::Process(HANDLE process) :
    process_(process)
{

}

Process::~Process()
{

}

Process Process::Current()
{
    return Process(GetCurrentProcess());
}

Process::Priority Process::GetPriority()
{
    DWORD value = GetPriorityClass(process_);

    if (!value)
    {
        LOG(ERROR) << "GetPriorityClass() failed: " << GetLastError();
        return Priority::Unknown;
    }

    switch (value)
    {
        case IDLE_PRIORITY_CLASS:         return Priority::Idle;
        case BELOW_NORMAL_PRIORITY_CLASS: return Priority::BelowNormal;
        case NORMAL_PRIORITY_CLASS:       return Priority::Normal;
        case ABOVE_NORMAL_PRIORITY_CLASS: return Priority::AboveNormal;
        case HIGH_PRIORITY_CLASS:         return Priority::High;
        case REALTIME_PRIORITY_CLASS:     return Priority::RealTime;
        default:
        {
            LOG(ERROR) << "Unknown process priority: " << value;
            break;
        }
    }

    return Priority::Unknown;
}

bool Process::SetPriority(Priority priority)
{
    DWORD value = NORMAL_PRIORITY_CLASS;

    switch (priority)
    {
        case Priority::Idle:        value = IDLE_PRIORITY_CLASS;         break;
        case Priority::BelowNormal: value = BELOW_NORMAL_PRIORITY_CLASS; break;
        case Priority::Normal:      value = NORMAL_PRIORITY_CLASS;       break;
        case Priority::AboveNormal: value = ABOVE_NORMAL_PRIORITY_CLASS; break;
        case Priority::High:        value = HIGH_PRIORITY_CLASS;         break;
        case Priority::RealTime:    value = REALTIME_PRIORITY_CLASS;     break;
        default:
        {
            LOG(ERROR) << "Unknown process priority.";
            return false;
        }
    }

    if (!SetPriorityClass(process_, value))
    {
        LOG(ERROR) << "SetPriorityClass() failed: " << GetLastError();
        return false;
    }

    return true;
}

uint32_t Process::GetId() const
{
    return ::GetProcessId(process_);
}

// static
uint32_t Process::GetCurrentId()
{
    return ::GetCurrentProcessId();
}

// static
bool Process::IsRunAsAdmin()
{
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    PSID admin_group = nullptr;
    BOOL is_admin = FALSE;

    if (AllocateAndInitializeSid(&nt_authority,
                                 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0,
                                 &admin_group))
    {
        CheckTokenMembership(nullptr, admin_group, &is_admin);
        FreeSid(admin_group);
    }

    return (is_admin != FALSE);
}

// static
bool Process::IsElevated(const WCHAR *command_line)
{
    return (std::wstring(command_line).find(L"--elevated") != std::wstring::npos);
}

// static
bool Process::Elevate(const WCHAR *command_line)
{
    WCHAR module_path[MAX_PATH];

    if (!GetModuleFileNameW(nullptr, module_path, _countof(module_path)))
    {
        LOG(ERROR) << "GetModuleFileNameW() failed: " << GetLastError();
        return false;
    }

    SHELLEXECUTEINFOW sei = { 0 };

    std::wstring cmd(command_line);

    cmd += L" --elevated";

    sei.cbSize       = sizeof(SHELLEXECUTEINFOW);
    sei.lpVerb       = L"runas";
    sei.lpFile       = module_path;
    sei.hwnd         = nullptr;
    sei.nShow        = SW_SHOW;
    sei.lpParameters = cmd.c_str();

    if (!ShellExecuteExW(&sei))
    {
        LOG(ERROR) << "ShellExecuteExW() failed: " << GetLastError();
        return false;
    }

    return true;
}
