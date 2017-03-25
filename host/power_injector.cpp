//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/power_injector.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/power_injector.h"

#include <wtsapi32.h>

#include "base/scoped_privilege.h"

namespace aspia {

void InjectPowerEvent(const proto::PowerEvent& event)
{
    switch (event.action())
    {
        // Завершение сеанса текущего пользователя не требует дополнительных привилегий.
        case proto::PowerEvent::LOGOFF:
        {
            if (!WTSLogoffSession(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, FALSE))
            {
                LOG(ERROR) << "WTSLogoffSession() failed: " << GetLastError();
            }

            return;
        }
        break;

        case proto::PowerEvent::LOCK:
        {
            if (!WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, FALSE))
            {
                LOG(ERROR) << "WTSDisconnectSession() failed: " << GetLastError();
            }

            return;
        }
        break;
    }

    // Если получить привилегии не удалось.
    ScopedProcessPrivilege privilege;

    if (!privilege.Enable(SE_SHUTDOWN_NAME))
        return;

    switch (event.action())
    {
        case proto::PowerEvent::SHUTDOWN:
        {
            if (!InitiateSystemShutdownExW(nullptr, // Локальный компьютер.
                                           nullptr, // Без вывода сообщения пользователю.
                                           0,       // Без задержки перед выполнением.
                                           TRUE,    // Закрывать приложения без предупреждения.
                                           FALSE,   // Выключение.
                                           SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_MAINTENANCE))
            {
                LOG(ERROR) << "InitiateSystemShutdownExW() failed: " << GetLastError();
            }
        }
        break;

        case proto::PowerEvent::REBOOT:
        {
            if (!InitiateSystemShutdownExW(nullptr, // Локальный компьютер.
                                           nullptr, // Без вывода сообщения пользователю.
                                           0,       // Без задержки перед выполнением.
                                           TRUE,    // Закрывать приложения без предупреждения.
                                           TRUE,    // Перезагрузка.
                                           SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_MAINTENANCE))
            {
                LOG(ERROR) << "InitiateSystemShutdownExW() failed: " << GetLastError();
            }
        }
        break;

        case proto::PowerEvent::HIBERNATE:
        {
            if (!SetSystemPowerState(FALSE, TRUE))
            {
                LOG(ERROR) << "SetSystemPowerState() failed: " << GetLastError();
            }
        }
        break;

        case proto::PowerEvent::SUSPEND:
        {
            if (!SetSystemPowerState(TRUE, TRUE))
            {
                LOG(ERROR) << "SetSystemPowerState() failed: " << GetLastError();
            }
        }
        break;

        default:
        {
            DLOG(WARNING) << "Unknown power control action requested: " << event.action();
        }
        break;
    }
}

} // namespace aspia
