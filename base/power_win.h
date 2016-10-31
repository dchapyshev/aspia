/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/power_win.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_POWER_WIN_H
#define _ASPIA_POWER_WIN_H

#include "aspia_config.h"

class PowerControl
{
public:
    static void Logoff()
    {
        ExitWindowsEx(EWX_LOGOFF | EWX_FORCE, 0);
    }

    static void Shutdown()
    {
        if (EnablePrivilege(SE_SHUTDOWN_NAME, true))
        {
            ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, 0);
            EnablePrivilege(SE_SHUTDOWN_NAME, false);
        }
    }

    static void PowerOff()
    {
        if (EnablePrivilege(SE_SHUTDOWN_NAME, true))
        {
            ExitWindowsEx(EWX_POWEROFF | EWX_FORCE, 0);
            EnablePrivilege(SE_SHUTDOWN_NAME, false);
        }
    }

    static void Reboot()
    {
        if (EnablePrivilege(SE_SHUTDOWN_NAME, true))
        {
            ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0);
            EnablePrivilege(SE_SHUTDOWN_NAME, false);
        }
    }

    static void Hibernate()
    {
        if (EnablePrivilege(SE_SHUTDOWN_NAME, true))
        {
            SetSystemPowerState(FALSE, TRUE);
            EnablePrivilege(SE_SHUTDOWN_NAME, false);
        }
    }

    static void Suspend()
    {
        if (EnablePrivilege(SE_SHUTDOWN_NAME, true))
        {
            SetSystemPowerState(TRUE, TRUE);
            EnablePrivilege(SE_SHUTDOWN_NAME, false);
        }
    }

private:
    static bool EnablePrivilege(const wchar_t *name, bool enable)
    {
        bool bResult = false;
        HANDLE hToken;

        if (OpenProcessToken(GetCurrentProcess(),
                             TOKEN_ADJUST_PRIVILEGES,
                             &hToken))
        {
            TOKEN_PRIVILEGES TokenPrivileges;

            if (LookupPrivilegeValueW(nullptr,
                                      name,
                                      &TokenPrivileges.Privileges[0].Luid))
            {
                TokenPrivileges.PrivilegeCount = 1;
                TokenPrivileges.Privileges[0].Attributes =
                    enable ? SE_PRIVILEGE_ENABLED : 0;

                if (AdjustTokenPrivileges(hToken,
                                          FALSE,
                                          &TokenPrivileges,
                                          0,
                                          nullptr,
                                          nullptr))
                {
                    bResult = true;
                }
            }

            CloseHandle(hToken);
        }

        return bResult;
    }
};

#endif // _ASPIA_POWER_WIN_H
