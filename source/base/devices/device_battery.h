//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/device_battery.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DEVICES__DEVICE_BATTERY_H
#define _ASPIA_BASE__DEVICES__DEVICE_BATTERY_H

#include "base/devices/device.h"

#include <batclass.h>

namespace aspia {

class DeviceBattery : public Device
{
public:
    DeviceBattery() = default;
    virtual ~DeviceBattery() = default;

    bool GetBatteryTag(ULONG& tag);

    bool GetStatus(BATTERY_STATUS& status);

    bool GetBatteryInformation(BATTERY_QUERY_INFORMATION_LEVEL level,
                               LPVOID buffer,
                               ULONG buffer_size);

    bool SetBatteryInformation(BATTERY_SET_INFORMATION_LEVEL level, UCHAR buffer);

private:
    DISALLOW_COPY_AND_ASSIGN(DeviceBattery);
};

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__DEVICE_BATTERY_H
