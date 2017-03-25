//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/process.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/process.h"

#include <shellapi.h>
#include <stdlib.h>
#include <string>

#include "base/scoped_impersonator.h"
#include "base/scoped_sid.h"
#include "base/logging.h"
#include "base/path.h"

namespace aspia {

Process::Process() :
    process_handle_(kNullProcessHandle)
{
    // Nothing
}

Process::Process(ProcessHandle process_handle) :
    process_handle_(process_handle)
{
    // Nothing
}

Process::Process(Process&& other)
{
    process_handle_ = other.process_handle_;
    other.process_handle_ = kNullProcessHandle;
}

// static
Process Process::Current()
{
    return Process(GetCurrentProcess());
}

void Process::Close()
{
    if (!process_handle_)
        return;

    // Don't call CloseHandle on a pseudo-handle.
    if (process_handle_ != GetCurrentProcess())
        CloseHandle(process_handle_);

    process_handle_ = kNullProcessHandle;
}

bool Process::IsValid() const
{
    return process_handle_ != kNullProcessHandle;
}

bool Process::IsCurrent() const
{
    return process_handle_ == GetCurrentProcess();
}

Process::Priority Process::GetPriority()
{
    DWORD value = GetPriorityClass(process_handle_);

    if (!value)
    {
        LOG(ERROR) << "GetPriorityClass() failed: " << GetLastError();
        return Priority::Unknown;
    }

    switch (value)
    {
        case IDLE_PRIORITY_CLASS:
            return Priority::Idle;

        case BELOW_NORMAL_PRIORITY_CLASS:
            return Priority::BelowNormal;

        case NORMAL_PRIORITY_CLASS:
            return Priority::Normal;

        case ABOVE_NORMAL_PRIORITY_CLASS:
            return Priority::AboveNormal;

        case HIGH_PRIORITY_CLASS:
            return Priority::High;

        case REALTIME_PRIORITY_CLASS:
            return Priority::RealTime;

        default:
            LOG(ERROR) << "Unknown process priority: " << value;
            return Priority::Unknown;
    }
}

bool Process::SetPriority(Priority priority)
{
    DWORD value = NORMAL_PRIORITY_CLASS;

    switch (priority)
    {
        case Priority::Idle:
            value = IDLE_PRIORITY_CLASS;
            break;

        case Priority::BelowNormal:
            value = BELOW_NORMAL_PRIORITY_CLASS;
            break;

        case Priority::Normal:
            value = NORMAL_PRIORITY_CLASS;
            break;

        case Priority::AboveNormal:
            value = ABOVE_NORMAL_PRIORITY_CLASS;
            break;

        case Priority::High:
            value = HIGH_PRIORITY_CLASS;
            break;

        case Priority::RealTime:
            value = REALTIME_PRIORITY_CLASS;
            break;

        default:
            LOG(ERROR) << "Unknown process priority.";
            return false;
    }

    if (!SetPriorityClass(process_handle_, value))
    {
        LOG(ERROR) << "SetPriorityClass() failed: " << GetLastError();
        return false;
    }

    return true;
}

uint32_t Process::Pid() const
{
    if (!process_handle_)
        return 0;

    return GetProcessId(process_handle_);
}

ProcessHandle Process::Handle() const
{
    return process_handle_;
}

void Process::Terminate(uint32_t exit_code)
{
    if (!process_handle_)
        return;

    //
    // Call NtTerminateProcess directly, without going through the import table,
    // which might have been hooked with a buggy replacement by third party
    // software. http://crbug.com/81449.
    //
    HMODULE module = GetModuleHandleW(L"ntdll.dll");

    typedef UINT(WINAPI *TerminateProcessPtr)(HANDLE handle, UINT code);

    TerminateProcessPtr terminate_process =
        reinterpret_cast<TerminateProcessPtr>(GetProcAddress(module, "NtTerminateProcess"));
    CHECK(terminate_process);

    terminate_process(process_handle_, exit_code);
}

Process& Process::operator=(Process&& other)
{
    Close();

    process_handle_ = other.process_handle_;
    other.process_handle_ = kNullProcessHandle;

    return *this;
}

bool Process::HasAdminRights()
{
    DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
                           TOKEN_DUPLICATE | TOKEN_QUERY;

    ScopedHandle process_token;

    if (!OpenProcessToken(process_handle_,
                          desired_access,
                          process_token.Recieve()))
    {
        LOG(ERROR) << "OpenProcessToken() failed: " << GetLastError();
        return false;
    }

    ScopedHandle duplicate_token;

    if (!DuplicateTokenEx(process_token,
                          desired_access,
                          nullptr,
                          SecurityImpersonation,
                          TokenPrimary,
                          duplicate_token.Recieve()))
    {
        LOG(ERROR) << "DuplicateTokenEx() failed: " << GetLastError();
        return false;
    }

    ScopedImpersonator impersonator;

    if (!impersonator.ImpersonateLoggedOnUser(duplicate_token))
        return false;

    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    ScopedSid admin_group;

    if (!AllocateAndInitializeSid(&nt_authority,
                                  2,
                                  SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0,
                                  admin_group.Recieve()))
    {
        LOG(ERROR) << "AllocateAndInitializeSid() failed: " << GetLastError();
        return false;
    }

    BOOL is_admin = FALSE;

    if (!CheckTokenMembership(nullptr, admin_group, &is_admin))
    {
        LOG(ERROR) << "CheckTokenMembership() failed: " << GetLastError();
        return false;
    }

    return !!is_admin;
}

// static
bool Process::ElevateProcess()
{
    std::wstring path;

    if (!GetPath(PathKey::FILE_EXE, &path))
        return false;

    SHELLEXECUTEINFOW sei = { 0 };

    sei.cbSize       = sizeof(SHELLEXECUTEINFOW);
    sei.lpVerb       = L"runas";
    sei.lpFile       = path.c_str();
    sei.hwnd         = nullptr;
    sei.nShow        = SW_SHOW;
    sei.lpParameters = nullptr;

    if (!ShellExecuteExW(&sei))
    {
        DLOG(WARNING) << "ShellExecuteExW() failed: " << GetLastError();
        return false;
    }

    return true;
}

bool Process::IsElevated()
{
    ScopedHandle token;

    if (!OpenProcessToken(process_handle_, TOKEN_QUERY, token.Recieve()))
    {
        LOG(ERROR) << "OpenProcessToken() failed: " << GetLastError();
        return false;
    }

    TOKEN_ELEVATION elevation;
    DWORD size;

    if (!GetTokenInformation(token, TokenElevation, &elevation,
                             sizeof(elevation), &size))
    {
        LOG(ERROR) << "GetTokenInformation() failed: " << GetLastError();
        return false;
    }

    return elevation.TokenIsElevated != 0;
}

} // namespace aspia
