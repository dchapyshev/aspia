//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/power.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

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
    if (!ExitWindowsEx(EWX_LOGOFF | EWX_FORCE, 0))
    {
        LOG(ERROR) << "ExitWindowsEx() failed: " << GetLastError();
    }
}

// static
void PowerControl::Shutdown()
{
    if (EnablePrivilege(SE_SHUTDOWN_NAME, true))
    {
        if (!ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, 0))
        {
            LOG(ERROR) << "ExitWindowsEx() failed: " << GetLastError();
        }

        EnablePrivilege(SE_SHUTDOWN_NAME, false);
    }
}

// static
void PowerControl::Reboot()
{
    if (EnablePrivilege(SE_SHUTDOWN_NAME, true))
    {
        if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0))
        {
            LOG(ERROR) << "ExitWindowsEx() failed: " << GetLastError();
        }

        EnablePrivilege(SE_SHUTDOWN_NAME, false);
    }
}

// static
void PowerControl::Hibernate()
{
    if (EnablePrivilege(SE_SHUTDOWN_NAME, true))
    {
        if (!SetSystemPowerState(FALSE, TRUE))
        {
            LOG(ERROR) << "SetSystemPowerState() failed: " << GetLastError();
        }

        EnablePrivilege(SE_SHUTDOWN_NAME, false);
    }
}

// static
void PowerControl::Suspend()
{
    if (EnablePrivilege(SE_SHUTDOWN_NAME, true))
    {
        if (!SetSystemPowerState(TRUE, TRUE))
        {
            LOG(ERROR) << "SetSystemPowerState() failed: " << GetLastError();
        }

        EnablePrivilege(SE_SHUTDOWN_NAME, false);
    }
}

// static
bool PowerControl::EnablePrivilege(const WCHAR *name, bool enable)
{
    HANDLE handle;

    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES,
                          &handle))
    {
        LOG(ERROR) << "OpenProcessToken() failed: " << GetLastError();
        return false;
    }

    ScopedHandle token(handle);
    TOKEN_PRIVILEGES privileges;

    if (!LookupPrivilegeValueW(nullptr,
                               name,
                               &privileges.Privileges[0].Luid))
    {
        LOG(ERROR) << "LookupPrivilegeValueW() failed: " << GetLastError();
        return false;
    }

    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Attributes =enable ? SE_PRIVILEGE_ENABLED : 0;

    if (!AdjustTokenPrivileges(token,
                               FALSE,
                               &privileges,
                               0,
                               nullptr,
                               nullptr))
    {
        LOG(ERROR) << "AdjustTokenPrivileges() failed: " << GetLastError();
        return false;
    }

    return true;
}

} // namespace aspia
