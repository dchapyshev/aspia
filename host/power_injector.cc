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

// Delay for shutdown and reboot.
static const DWORD kActionDelayInSeconds = 60;

bool InjectPowerEvent(const proto::PowerEvent& event)
{
    switch (event.action())
    {
        case proto::PowerEvent::LOGOFF:
        {
            if (!WTSLogoffSession(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, FALSE))
            {
                LOG(ERROR) << "WTSLogoffSession() failed: " << GetLastError();
                return false;
            }

            return true;
        }
        break;

        case proto::PowerEvent::LOCK:
        {
            if (!WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, FALSE))
            {
                LOG(ERROR) << "WTSDisconnectSession() failed: " << GetLastError();
                return false;
            }

            return true;
        }
        break;
    }

    ScopedProcessPrivilege privilege;
    if (!privilege.Enable(SE_SHUTDOWN_NAME))
        return false;

    switch (event.action())
    {
        case proto::PowerEvent::SHUTDOWN:
        {
            if (!InitiateSystemShutdownExW(nullptr,
                                           nullptr,
                                           kActionDelayInSeconds,
                                           TRUE,  // Force close apps.
                                           FALSE, // Shutdown.
                                           SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_MAINTENANCE))
            {
                LOG(ERROR) << "InitiateSystemShutdownExW() failed: " << GetLastError();
                return false;
            }
        }
        break;

        case proto::PowerEvent::REBOOT:
        {
            if (!InitiateSystemShutdownExW(nullptr,
                                           nullptr,
                                           kActionDelayInSeconds,
                                           TRUE, // Force close apps.
                                           TRUE, // Reboot.
                                           SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_MAINTENANCE))
            {
                LOG(ERROR) << "InitiateSystemShutdownExW() failed: " << GetLastError();
                return false;
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
