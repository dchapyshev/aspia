/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/process.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__PROCESS_H
#define _ASPIA_BASE__PROCESS_H

#include "aspia_config.h"

#include <stdint.h>

class Process
{
public:
    ~Process();

    enum class Priority
    {
        Unknown     = 0,
        Idle        = 1,
        BelowNormal = 2,
        Normal      = 3,
        AboveNormal = 4,
        High        = 5,
        RealTime    = 6
    };

    static Process Current();

    Priority GetPriority();
    bool SetPriority(Priority priority);
    uint32_t GetId() const;

    static uint32_t GetCurrentId();
    static bool IsHaveAdminRights();
    static bool Elevate(const WCHAR *command_line);

private:
    Process(HANDLE process);

private:
    HANDLE process_;
};

#endif // _ASPIA_BASE__PROCESS_H
