//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/win/battery_enumerator.h"

#include "base/logging.h"

#include <batclass.h>
#include <winioctl.h>
#include <devguid.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
bool batteryInformation(Device& battery,
                        ULONG tag,
                        BATTERY_QUERY_INFORMATION_LEVEL level,
                        LPVOID buffer,
                        ULONG buffer_size)
{
    BATTERY_QUERY_INFORMATION battery_info;

    memset(&battery_info, 0, sizeof(battery_info));
    battery_info.BatteryTag = tag;
    battery_info.InformationLevel = level;

    ULONG bytes_returned;

    return battery.ioControl(IOCTL_BATTERY_QUERY_INFORMATION,
                             &battery_info, sizeof(battery_info),
                             buffer, buffer_size,
                             &bytes_returned);
}

//--------------------------------------------------------------------------------------------------
bool batteryStatus(Device& battery, ULONG tag, BATTERY_STATUS* status)
{
    BATTERY_WAIT_STATUS status_request;
    memset(&status_request, 0, sizeof(status_request));
    status_request.BatteryTag = tag;

    BATTERY_STATUS status_reply;
    memset(&status_reply, 0, sizeof(status_reply));

    DWORD bytes_returned = 0;

    if (!battery.ioControl(IOCTL_BATTERY_QUERY_STATUS,
                           &status_request, sizeof(status_request),
                           &status_reply, sizeof(status_reply),
                           &bytes_returned))
    {
        return false;
    }

    *status = status_reply;
    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
BatteryEnumerator::BatteryEnumerator()
{
    const DWORD flags = DIGCF_PROFILE | DIGCF_PRESENT | DIGCF_DEVICEINTERFACE;
    device_info_.reset(SetupDiGetClassDevsW(&GUID_DEVCLASS_BATTERY, nullptr, nullptr, flags));
    if (!device_info_.isValid())
    {
        PLOG(LS_ERROR) << "SetupDiGetClassDevsW failed";
        return;
    }
}

//--------------------------------------------------------------------------------------------------
BatteryEnumerator::~BatteryEnumerator() = default;

//--------------------------------------------------------------------------------------------------
bool BatteryEnumerator::isAtEnd() const
{
    SP_DEVICE_INTERFACE_DATA device_iface_data;

    memset(&device_iface_data, 0, sizeof(device_iface_data));
    device_iface_data.cbSize = sizeof(device_iface_data);

    if (!SetupDiEnumDeviceInterfaces(device_info_.get(),
                                     nullptr,
                                     &GUID_DEVCLASS_BATTERY,
                                     device_index_,
                                     &device_iface_data))
    {
        DWORD error_code = GetLastError();

        if (error_code != ERROR_NO_MORE_ITEMS)
        {
            LOG(LS_ERROR) << "SetupDiEnumDeviceInfo failed:"
                          << SystemError(error_code).toString();
        }

        return true;
    }

    DWORD required_size = 0;
    if (SetupDiGetDeviceInterfaceDetailW(device_info_.get(),
                                         &device_iface_data,
                                         nullptr,
                                         0,
                                         &required_size,
                                         nullptr) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        PLOG(LS_ERROR) << "Unexpected return value";
        return true;
    }

    std::unique_ptr<quint8[]> buffer = std::make_unique<quint8[]>(required_size);
    PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail_data =
        reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.get());

    detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

    if (!SetupDiGetDeviceInterfaceDetailW(device_info_.get(),
                                          &device_iface_data,
                                          detail_data,
                                          required_size,
                                          &required_size,
                                          nullptr))
    {
        PLOG(LS_ERROR) << "SetupDiGetDeviceInterfaceDetailW failed";
        return true;
    }

    if (!battery_.open(QString::fromWCharArray(detail_data->DevicePath)))
        return true;

    ULONG bytes_returned;
    ULONG input_buffer = 0;
    ULONG output_buffer = 0;

    if (!battery_.ioControl(IOCTL_BATTERY_QUERY_TAG,
                            &input_buffer, sizeof(input_buffer),
                            &output_buffer, sizeof(output_buffer),
                            &bytes_returned))
    {
        return true;
    }

    battery_tag_ = output_buffer;
    return false;
}

//--------------------------------------------------------------------------------------------------
void BatteryEnumerator::advance()
{
    ++device_index_;
}

//--------------------------------------------------------------------------------------------------
QString BatteryEnumerator::deviceName() const
{
    wchar_t buffer[256] = { 0 };

    if (!batteryInformation(battery_, battery_tag_, BatteryDeviceName, buffer, sizeof(buffer)))
        return QString();

    return QString::fromWCharArray(buffer);
}

//--------------------------------------------------------------------------------------------------
QString BatteryEnumerator::manufacturer() const
{
    wchar_t buffer[256] = { 0 };

    if (!batteryInformation(battery_, battery_tag_, BatteryManufactureName, buffer, sizeof(buffer)))
        return QString();

    return QString::fromWCharArray(buffer);
}

//--------------------------------------------------------------------------------------------------
QString BatteryEnumerator::manufactureDate() const
{
    BATTERY_MANUFACTURE_DATE date;

    memset(&date, 0, sizeof(date));

    if (!batteryInformation(battery_, battery_tag_, BatteryManufactureDate, &date, sizeof(date)))
        return QString();

    return QString("%1-%2-%3").arg(date.Day).arg(date.Month).arg(date.Year);
}

//--------------------------------------------------------------------------------------------------
QString BatteryEnumerator::uniqueId() const
{
    wchar_t buffer[256] = { 0 };

    if (!batteryInformation(battery_, battery_tag_, BatteryUniqueID, buffer, sizeof(buffer)))
        return QString();

    return QString::fromWCharArray(buffer);
}

//--------------------------------------------------------------------------------------------------
QString BatteryEnumerator::serialNumber() const
{
    wchar_t buffer[256] = { 0 };

    if (!batteryInformation(battery_, battery_tag_, BatterySerialNumber, buffer, sizeof(buffer)))
        return QString();

    return QString::fromWCharArray(buffer);
}

//--------------------------------------------------------------------------------------------------
QString BatteryEnumerator::temperature() const
{
    wchar_t buffer[256] = { 0 };

    if (!batteryInformation(battery_, battery_tag_, BatteryTemperature, buffer, sizeof(buffer)))
        return QString();

    return QString::fromWCharArray(buffer);
}

//--------------------------------------------------------------------------------------------------
quint32 BatteryEnumerator::designCapacity() const
{
    BATTERY_INFORMATION battery_info;

    memset(&battery_info, 0, sizeof(battery_info));

    if (!batteryInformation(battery_, battery_tag_, BatteryInformation,
                            &battery_info, sizeof(battery_info)))
        return 0;

    return battery_info.DesignedCapacity;
}

//--------------------------------------------------------------------------------------------------
QString BatteryEnumerator::type() const
{
    BATTERY_INFORMATION battery_info;
    memset(&battery_info, 0, sizeof(battery_info));

    if (batteryInformation(battery_, battery_tag_, BatteryInformation,
                           &battery_info, sizeof(battery_info)))
    {
        if (memcmp(&battery_info.Chemistry[0], "PbAc", 4) == 0)
            return "Lead Acid";

        if (memcmp(&battery_info.Chemistry[0], "LION", 4) == 0 ||
            memcmp(&battery_info.Chemistry[0], "Li-I", 4) == 0)
            return "Lithium Ion";

        if (memcmp(&battery_info.Chemistry[0], "NiCd", 4) == 0)
            return "Nickel Cadmium";

        if (memcmp(&battery_info.Chemistry[0], "NiMH", 4) == 0)
            return "Nickel Metal Hydride";

        if (memcmp(&battery_info.Chemistry[0], "NiZn", 4) == 0)
            return "Nickel Zinc";

        if (memcmp(&battery_info.Chemistry[0], "RAM", 3) == 0)
            return "Rechargeable Alkaline-Manganese";
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
quint32 BatteryEnumerator::fullChargedCapacity() const
{
    BATTERY_INFORMATION battery_info;
    memset(&battery_info, 0, sizeof(battery_info));

    if (!batteryInformation(battery_, battery_tag_, BatteryInformation,
                            &battery_info, sizeof(battery_info)))
        return 0;

    return battery_info.FullChargedCapacity;
}

//--------------------------------------------------------------------------------------------------
quint32 BatteryEnumerator::depreciation() const
{
    BATTERY_INFORMATION battery_info;
    memset(&battery_info, 0, sizeof(battery_info));

    if (!batteryInformation(battery_, battery_tag_, BatteryInformation,
                            &battery_info, sizeof(battery_info)) ||
        battery_info.DesignedCapacity == 0)
    {
        return 0;
    }

    const int percent = 100 - (static_cast<int>(battery_info.FullChargedCapacity) * 100) /
        static_cast<int>(battery_info.DesignedCapacity);

    return (percent > 0) ? static_cast<quint32>(percent) : 0;
}

//--------------------------------------------------------------------------------------------------
quint32 BatteryEnumerator::currentCapacity() const
{
    BATTERY_STATUS battery_status;
    if (!batteryStatus(battery_, battery_tag_, &battery_status))
        return 0;

    return battery_status.Capacity;
}

//--------------------------------------------------------------------------------------------------
quint32 BatteryEnumerator::voltage() const
{
    BATTERY_STATUS battery_status;
    if (!batteryStatus(battery_, battery_tag_, &battery_status))
        return 0;

    return battery_status.Voltage;
}

//--------------------------------------------------------------------------------------------------
quint32 BatteryEnumerator::state() const
{
    BATTERY_STATUS battery_status;
    if (!batteryStatus(battery_, battery_tag_, &battery_status))
        return 0;

    quint32 result = 0;

    if (battery_status.PowerState & BATTERY_CHARGING)
        result |= CHARGING;

    if (battery_status.PowerState & BATTERY_CRITICAL)
        result |= CRITICAL;

    if (battery_status.PowerState & BATTERY_DISCHARGING)
        result |= DISCHARGING;

    if (battery_status.PowerState & BATTERY_POWER_ON_LINE)
        result |= POWER_ONLINE;

    return result;
}

} // namespace base
