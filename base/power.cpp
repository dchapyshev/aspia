/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/power.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/power.h"

namespace aspia {

PowerControl::PowerControl()
{
    // Nothing
}

PowerControl::~PowerControl()
{
    // Nothing
}

// static
void PowerControl::Logoff()
{
    ExitWindowsEx(EWX_LOGOFF | EWX_FORCE, 0);
}

// static
void PowerControl::Shutdown()
{
    if (EnablePrivilege(SE_SHUTDOWN_NAME, true))
    {
        ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, 0);
        EnablePrivilege(SE_SHUTDOWN_NAME, false);
    }
}

//static
void PowerControl::PowerOff()
{
    if (EnablePrivilege(SE_SHUTDOWN_NAME, true))
    {
        ExitWindowsEx(EWX_POWEROFF | EWX_FORCE, 0);
        EnablePrivilege(SE_SHUTDOWN_NAME, false);
    }
}

// static
void PowerControl::Reboot()
{
    if (EnablePrivilege(SE_SHUTDOWN_NAME, true))
    {
        ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0);
        EnablePrivilege(SE_SHUTDOWN_NAME, false);
    }
}

// static
void PowerControl::Hibernate()
{
    if (EnablePrivilege(SE_SHUTDOWN_NAME, true))
    {
        SetSystemPowerState(FALSE, TRUE);
        EnablePrivilege(SE_SHUTDOWN_NAME, false);
    }
}

// static
void PowerControl::Suspend()
{
    if (EnablePrivilege(SE_SHUTDOWN_NAME, true))
    {
        SetSystemPowerState(TRUE, TRUE);
        EnablePrivilege(SE_SHUTDOWN_NAME, false);
    }
}

// static
bool PowerControl::EnablePrivilege(const wchar_t *name, bool enable)
{
    bool result = false;
    HANDLE handle;

    if (OpenProcessToken(GetCurrentProcess(),
                         TOKEN_ADJUST_PRIVILEGES,
                         &handle))
    {
        ScopedHandle token(handle);

        TOKEN_PRIVILEGES privileges;

        if (LookupPrivilegeValueW(nullptr,
                                  name,
                                  &privileges.Privileges[0].Luid))
        {
            privileges.PrivilegeCount = 1;
            privileges.Privileges[0].Attributes =
                enable ? SE_PRIVILEGE_ENABLED : 0;

            if (AdjustTokenPrivileges(token,
                                      FALSE,
                                      &privileges,
                                      0,
                                      nullptr,
                                      nullptr))
            {
                result = true;
            }
        }
    }

    return result;
}

} // namespace aspia
