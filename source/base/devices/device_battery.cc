//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/device_battery.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/device_battery.h"

#include <winioctl.h>

namespace aspia {

bool DeviceBattery::GetBatteryTag(ULONG& tag)
{
    ULONG bytes_returned;
    ULONG input_buffer = 0;
    ULONG output_buffer = 0;

    if (!SendIoControl(IOCTL_BATTERY_QUERY_TAG,
                       &input_buffer,
                       sizeof(input_buffer),
                       &output_buffer,
                       sizeof(output_buffer),
                       &bytes_returned))
    {
        return false;
    }

    tag = output_buffer;
    return true;
}

bool DeviceBattery::GetBatteryInformation(BATTERY_QUERY_INFORMATION_LEVEL level,
                                          LPVOID buffer,
                                          ULONG buffer_size)
{
    BATTERY_QUERY_INFORMATION battery_info;

    memset(&battery_info, 0, sizeof(battery_info));
    battery_info.InformationLevel = level;

    if (!GetBatteryTag(battery_info.BatteryTag))
        return false;

    ULONG bytes_returned;

    return SendIoControl(IOCTL_BATTERY_QUERY_INFORMATION,
                         &battery_info,
                         sizeof(battery_info),
                         buffer,
                         buffer_size,
                         &bytes_returned);
}

bool DeviceBattery::SetBatteryInformation(BATTERY_SET_INFORMATION_LEVEL level, UCHAR value)
{
    BATTERY_SET_INFORMATION battery_info;

    memset(&battery_info, 0, sizeof(battery_info));
    battery_info.InformationLevel = level;
    battery_info.Buffer[0] = value;

    if (!GetBatteryTag(battery_info.BatteryTag))
        return false;

    ULONG bytes_returned;

    return SendIoControl(IOCTL_BATTERY_SET_INFORMATION,
                         &battery_info, sizeof(battery_info),
                         nullptr, 0,
                         &bytes_returned);
}

bool DeviceBattery::GetStatus(BATTERY_STATUS& status)
{
    BATTERY_WAIT_STATUS status_request;
    memset(&status_request, 0, sizeof(status_request));

    if (!GetBatteryTag(status_request.BatteryTag))
        return false;

    BATTERY_STATUS status_reply;
    memset(&status_reply, 0, sizeof(status_reply));

    DWORD bytes_returned;

    if (!SendIoControl(IOCTL_BATTERY_QUERY_STATUS,
                       &status_request, sizeof(status_request),
                       &status_reply, sizeof(status_reply),
                       &bytes_returned))
    {
        return false;
    }

    status = status_reply;
    return true;
}

} // namespace aspia
