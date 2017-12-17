//
// PROJECT:         Aspia
// FILE:            base/devices/battery_enumerator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DEVICES__BATTERY_ENUMERATOR_H
#define _ASPIA_BASE__DEVICES__BATTERY_ENUMERATOR_H

#include "base/devices/device.h"

#include <batclass.h>
#include <setupapi.h>

namespace aspia {

class BatteryEnumerator
{
public:
    BatteryEnumerator();
    virtual ~BatteryEnumerator();

    bool IsAtEnd() const;
    void Advance();

    enum State
    {
        STATE_CHARGING     = BATTERY_CHARGING,
        STATE_CRITICAL     = BATTERY_CRITICAL,
        STATE_DISCHARGING  = BATTERY_DISCHARGING,
        STATE_POWER_ONLINE = BATTERY_POWER_ON_LINE
    };

    std::string GetDeviceName() const;
    std::string GetManufacturer() const;
    std::string GetManufactureDate() const;
    std::string GetUniqueId() const;
    std::string GetSerialNumber() const;
    std::string GetTemperature() const;
    int GetDesignCapacity() const;
    std::string GetType() const;
    int GetFullChargedCapacity() const;
    int GetDepreciation() const;
    int GetCurrentCapacity() const;
    int GetVoltage() const;
    uint32_t GetState() const;

private:
    bool GetBatteryTag(ULONG& tag) const;
    bool GetStatus(BATTERY_STATUS& status) const;
    bool GetBatteryInformation(BATTERY_QUERY_INFORMATION_LEVEL level,
                               LPVOID buffer,
                               ULONG buffer_size) const;

    HDEVINFO device_info_;
    mutable DWORD device_index_ = 0;
    mutable Device device_;

    DISALLOW_COPY_AND_ASSIGN(BatteryEnumerator);
};

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__BATTERY_ENUMERATOR_H
