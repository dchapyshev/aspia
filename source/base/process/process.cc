//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/process/process.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/process/process.h"

#include <stdlib.h>
#include <string>

#include "base/logging.h"

namespace aspia {

static const DWORD kBasicProcessAccess = PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | SYNCHRONIZE;

Process::Process(ProcessHandle process_handle) :
    process_handle_(process_handle)
{
    // Nothing
}

Process::Process(Process&& other) noexcept
{
    process_handle_ = other.process_handle_;
    other.process_handle_ = kNullProcessHandle;
}

// static
Process Process::Current()
{
    return Process(GetCurrentProcess());
}

// static
Process Process::Open(ProcessId pid)
{
    return Process(OpenProcess(kBasicProcessAccess, FALSE, pid));
}

// static
Process Process::OpenWithExtraPrivileges(ProcessId pid)
{
    DWORD access = kBasicProcessAccess | PROCESS_DUP_HANDLE | PROCESS_VM_READ;
    return Process(OpenProcess(access, FALSE, pid));
}

// static
Process Process::OpenWithAccess(ProcessId pid, DWORD desired_access)
{
    return Process(OpenProcess(desired_access, FALSE, pid));
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

Process::Priority Process::GetPriority() const
{
    return static_cast<Priority>(GetPriorityClass(Handle()));
}

bool Process::SetPriority(Priority priority)
{
    if (!SetPriorityClass(Handle(), static_cast<DWORD>(priority)))
    {
        LOG(ERROR) << "SetPriorityClass() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    return true;
}

ProcessId Process::Pid() const
{
    return GetProcessId(Handle());
}

ProcessHandle Process::Handle() const
{
    return process_handle_;
}

bool Process::Terminate(uint32_t exit_code, bool wait)
{
    bool result = TerminateProcess(Handle(), exit_code) != FALSE;

    if (result && wait)
    {
        if (WaitForSingleObject(Handle(), 60 * 1000) != WAIT_OBJECT_0)
        {
            LOG(ERROR) << "Error waiting for process exit";
        }
    }
    else if (!result)
    {
        LOG(ERROR) << "Unable to terminate process";
    }

    return result;
}

bool Process::WaitForExit(int* exit_code) const
{
    return WaitForExitWithTimeout(INFINITE, exit_code);
}

bool Process::WaitForExitWithTimeout(uint32_t timeout_ms, int* exit_code) const
{
    if (WaitForSingleObject(Handle(), timeout_ms) != WAIT_OBJECT_0)
        return false;

    DWORD temp_code; // Don't clobber out-parameters in case of failure.
    if (!GetExitCodeProcess(Handle(), &temp_code))
        return false;

    if (exit_code)
        *exit_code = temp_code;

    return true;
}

Process& Process::operator=(Process&& other) noexcept
{
    Close();

    process_handle_ = other.process_handle_;
    other.process_handle_ = kNullProcessHandle;

    return *this;
}

} // namespace aspia
