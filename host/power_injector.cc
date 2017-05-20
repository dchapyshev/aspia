//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/power_injector.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/power_injector.h"
#include "base/scoped_privilege.h"
#include "base/scoped_impersonator.h"

#include <wtsapi32.h>

namespace aspia {

bool InjectPowerEvent(const proto::PowerEvent& event)
{
    ScopedProcessPrivilege privilege(SE_SHUTDOWN_NAME);
    if (!privilege.IsSuccessed())
        return false;

    switch (event.action())
    {
        case proto::PowerEvent::SHUTDOWN:
        {
            if (!ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, 0))
            {
                LOG(ERROR) << "ExitWindowsEx() failed: "
                           << GetLastSystemErrorString();
            }
        }
        break;

        case proto::PowerEvent::REBOOT:
        {
            if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0))
            {
                LOG(ERROR) << "ExitWindowsEx() failed: "
                           << GetLastSystemErrorString();
            }
        }
        break;

        case proto::PowerEvent::HIBERNATE:
        {
            if (!SetSystemPowerState(FALSE, TRUE))
            {
                LOG(ERROR) << "SetSystemPowerState() failed: "
                           << GetLastSystemErrorString();
                return false;
            }
        }
        break;

        case proto::PowerEvent::SUSPEND:
        {
            if (!SetSystemPowerState(TRUE, TRUE))
            {
                LOG(ERROR) << "SetSystemPowerState() failed: "
                           << GetLastSystemErrorString();
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
