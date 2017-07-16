//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/process/process.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__PROCESS__PROCESS_H
#define _ASPIA_BASE__PROCESS__PROCESS_H

#include "base/macros.h"

#include <cstdint>

namespace aspia {

using ProcessHandle = HANDLE;
using ProcessId = DWORD;

static const ProcessHandle kNullProcessHandle = nullptr;

class Process
{
public:
    Process() = default;
    Process(Process&& other) noexcept;
    ~Process() = default;

    enum class Priority
    {
        UNKNOWN      = 0,
        IDLE         = IDLE_PRIORITY_CLASS,
        BELOW_NORMAL = BELOW_NORMAL_PRIORITY_CLASS,
        NORMAL       = NORMAL_PRIORITY_CLASS,
        ABOVE_NORMAL = ABOVE_NORMAL_PRIORITY_CLASS,
        HIGH         = HIGH_PRIORITY_CLASS,
        REALTIME     = REALTIME_PRIORITY_CLASS
    };

    // Returns an object for the current process.
    static Process Current();

    // Returns a Process for the given |pid|.
    static Process Open(ProcessId pid);

    // Returns a Process for the given |pid|. On Windows the handle is opened
    // with more access rights and must only be used by trusted code (can read the
    // address space and duplicate handles).
    static Process OpenWithExtraPrivileges(ProcessId pid);

    // Returns a Process for the given |pid|, using some |desired_access|.
    // See ::OpenProcess documentation for valid |desired_access|.
    static Process OpenWithAccess(ProcessId pid, DWORD desired_access);

    // Returns true if this objects represents a valid process.
    bool IsValid() const;

    // Returns true if this process is the current process.
    bool IsCurrent() const;

    Priority GetPriority() const;
    bool SetPriority(Priority priority);

    // Get the PID for this process.
    ProcessId Pid() const;

    // Returns a handle for this process. There is no guarantee about when that
    // handle becomes invalid because this object retains ownership.
    ProcessHandle Handle() const;

    // Terminates the process with extreme prejudice. The given |exit_code| will
    // be the exit code of the process. If |wait| is true, this method will wait
    // for up to one minute for the process to actually terminate.
    // Returns true if the process terminates within the allowed time.
    bool Terminate(uint32_t exit_code, bool wait);

    // Waits for the process to exit. Returns true on success.
    // NOTE: |exit_code| is optional, nullptr can be passed if the exit code is
    // not required.
    bool WaitForExit(int* exit_code) const;

    // Same as WaitForExit() but only waits for up to |timeout|.
    // NOTE: |exit_code| is optional, nullptr can be passed if the exit code
    // is not required.
    bool WaitForExitWithTimeout(uint32_t timeout_ms, int* exit_code) const;

    // Close the process handle. This will not terminate the process.
    void Close();

    Process& operator=(Process&& other) noexcept;

private:
    explicit Process(ProcessHandle process_handle);

    ProcessHandle process_handle_ = kNullProcessHandle;

    DISALLOW_COPY_AND_ASSIGN(Process);
};

} // namespace aspia

#endif // _ASPIA_BASE__PROCESS__PROCESS_H
