/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/power.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__POWER_H
#define _ASPIA_BASE__POWER_H

#include "aspia_config.h"

#include "base/scoped_handle.h"

namespace aspia {

class PowerControl
{
public:
    PowerControl();
    ~PowerControl();

    static void Logoff();
    static void Shutdown();
    static void PowerOff();
    static void Reboot();
    static void Hibernate();
    static void Suspend();

private:
    static bool EnablePrivilege(const wchar_t *name, bool enable);
};

} // namespace aspia

#endif // _ASPIA_BASE__POWER_H
