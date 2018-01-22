//
// PROJECT:         Aspia
// FILE:            host/power_injector.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/scoped_privilege.h"
#include "base/logging.h"
#include "host/power_injector.h"

namespace aspia {

bool InjectPowerCommand(proto::power::Command command)
{
    ScopedProcessPrivilege privilege(SE_SHUTDOWN_NAME);
    if (!privilege.IsSuccessed())
        return false;

    switch (command)
    {
        case proto::power::COMMAND_SHUTDOWN:
        {
            if (!ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, 0))
            {
                LOG(LS_ERROR) << "ExitWindowsEx() failed: " << GetLastSystemErrorString();
            }
        }
        break;

        case proto::power::COMMAND_REBOOT:
        {
            if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0))
            {
                LOG(LS_ERROR) << "ExitWindowsEx() failed: " << GetLastSystemErrorString();
            }
        }
        break;

        case proto::power::COMMAND_HIBERNATE:
        {
            if (!SetSystemPowerState(FALSE, TRUE))
            {
                LOG(LS_ERROR) << "SetSystemPowerState() failed: " << GetLastSystemErrorString();
                return false;
            }
        }
        break;

        case proto::power::COMMAND_SUSPEND:
        {
            if (!SetSystemPowerState(TRUE, TRUE))
            {
                LOG(LS_ERROR) << "SetSystemPowerState() failed: " << GetLastSystemErrorString();
                return false;
            }
        }
        break;

        default:
        {
            DLOG(LS_WARNING) << "Unknown power control action requested: " << command;
            return false;
        }
    }

    return true;
}

} // namespace aspia
