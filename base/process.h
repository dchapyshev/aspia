//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/process.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__PROCESS_H
#define _ASPIA_BASE__PROCESS_H

#include "aspia_config.h"

#include <stdint.h>

#include "base/scoped_handle.h"
#include "base/macros.h"

namespace aspia {

typedef HANDLE ProcessHandle;

static const ProcessHandle kNullProcessHandle = nullptr;

class Process
{
public:
    Process();
    Process(ProcessHandle process_handle);
    Process(Process&& other);
    ~Process() = default;

    enum class Priority { Unknown, Idle, BelowNormal, Normal, AboveNormal, High, RealTime };

    static Process Current();

    bool IsValid() const;
    bool IsCurrent() const;

    Priority GetPriority();
    bool SetPriority(Priority priority);
    uint32_t Pid() const;
    ProcessHandle Handle() const;
    void Terminate(uint32_t exit_code);

    bool HasAdminRights();

    static bool ElevateProcess();

    bool IsElevated();

    void Close();

    Process& operator=(Process&& other);

private:
    ProcessHandle process_handle_;

    DISALLOW_COPY_AND_ASSIGN(Process);
};

} // namespace aspia

#endif // _ASPIA_BASE__PROCESS_H
