//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/power_injector.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/power_injector.h"
#include "base/scoped_privilege.h"

#include <wtsapi32.h>

namespace aspia {

bool InjectPowerEvent(const proto::PowerEvent& event)
{
    switch (event.action())
    {
        case proto::PowerEvent::LOGOFF:
        {
            if (!ExitWindowsEx(EWX_LOGOFF | EWX_FORCE, 0))
            {
                LOG(ERROR) << "ExitWindowsEx() failed: " << GetLastError();
                return false;
            }

            return true;
        }
        break;

        case proto::PowerEvent::LOCK:
        {
            if (!LockWorkStation())
            {
                LOG(ERROR) << "LockWorkStation() failed: " << GetLastError();
                return false;
            }

            return true;
        }
        break;
    }

    ScopedProcessPrivilege privilege(SE_SHUTDOWN_NAME);
    if (!privilege.IsSuccessed())
        return false;

    switch (event.action())
    {
        case proto::PowerEvent::SHUTDOWN:
        {
            if (!ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, 0))
            {
                LOG(ERROR) << "ExitWindowsEx() failed: " << GetLastError();
            }
        }
        break;

        case proto::PowerEvent::REBOOT:
        {
            if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0))
            {
                LOG(ERROR) << "ExitWindowsEx() failed: " << GetLastError();
            }
        }
        break;

        case proto::PowerEvent::HIBERNATE:
        {
            if (!SetSystemPowerState(FALSE, TRUE))
            {
                LOG(ERROR) << "SetSystemPowerState() failed: " << GetLastError();
                return false;
            }
        }
        break;

        case proto::PowerEvent::SUSPEND:
        {
            if (!SetSystemPowerState(TRUE, TRUE))
            {
                LOG(ERROR) << "SetSystemPowerState() failed: " << GetLastError();
                return false;
            }
        }
        break;

        default:
        {
            DLOG(WARNING) << "Unknown power control action requested: " << event.action();
            return false;
        }
        break;
    }

    return true;
}

} // namespace aspia
