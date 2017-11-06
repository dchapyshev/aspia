//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/battery_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/battery_enumerator.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

#include <winioctl.h>
#include <devguid.h>

namespace aspia {

BatteryEnumerator::BatteryEnumerator()
{
    const DWORD flags = DIGCF_PROFILE | DIGCF_PRESENT | DIGCF_DEVICEINTERFACE;

    device_info_ = SetupDiGetClassDevsW(&GUID_DEVCLASS_BATTERY, nullptr, nullptr, flags);
    if (!device_info_ || device_info_ == INVALID_HANDLE_VALUE)
    {
        LOG(WARNING) << "SetupDiGetClassDevsW() failed: " << GetLastSystemErrorString();
    }
}

BatteryEnumerator::~BatteryEnumerator()
{
    if (device_info_ && device_info_ != INVALID_HANDLE_VALUE)
    {
        SetupDiDestroyDeviceInfoList(device_info_);
    }
}

bool BatteryEnumerator::IsAtEnd() const
{
    for (;;)
    {
        SP_DEVICE_INTERFACE_DATA device_iface_data;

        memset(&device_iface_data, 0, sizeof(device_iface_data));
        device_iface_data.cbSize = sizeof(device_iface_data);

        if (!SetupDiEnumDeviceInterfaces(device_info_,
                                         nullptr,
                                         &GUID_DEVCLASS_BATTERY,
                                         device_index_,
                                         &device_iface_data))
        {
            SystemErrorCode error_code = GetLastError();

            if (error_code != ERROR_NO_MORE_ITEMS)
            {
                LOG(WARNING) << "SetupDiEnumDeviceInfo() failed: "
                             << SystemErrorCodeToString(error_code);
            }

            return true;
        }

        DWORD required_size = 0;

        if (SetupDiGetDeviceInterfaceDetailW(device_info_,
                                             &device_iface_data,
                                             nullptr,
                                             0,
                                             &required_size,
                                             nullptr) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            LOG(WARNING) << "Unexpected return value: " << GetLastSystemErrorString();
            ++device_index_;
            continue;
        }

        std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(required_size);
        PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail_data =
            reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.get());

        detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(device_info_,
                                              &device_iface_data,
                                              detail_data,
                                              required_size,
                                              &required_size,
                                              nullptr))
        {
            LOG(WARNING) << "SetupDiGetDeviceInterfaceDetailW() failed: "
                         << GetLastSystemErrorString();
            ++device_index_;
            continue;
        }

        if (device_.Open(detail_data->DevicePath))
            break;

        ++device_index_;
    }

    return false;
}

void BatteryEnumerator::Advance()
{
    ++device_index_;
}

bool BatteryEnumerator::GetBatteryTag(ULONG& tag) const
{
    ULONG bytes_returned;
    ULONG input_buffer = 0;
    ULONG output_buffer = 0;

    if (!device_.SendIoControl(IOCTL_BATTERY_QUERY_TAG,
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

bool BatteryEnumerator::GetBatteryInformation(BATTERY_QUERY_INFORMATION_LEVEL level,
                                              LPVOID buffer,
                                              ULONG buffer_size) const
{
    BATTERY_QUERY_INFORMATION battery_info;

    memset(&battery_info, 0, sizeof(battery_info));
    battery_info.InformationLevel = level;

    if (!GetBatteryTag(battery_info.BatteryTag))
        return false;

    ULONG bytes_returned;

    return device_.SendIoControl(IOCTL_BATTERY_QUERY_INFORMATION,
                                 &battery_info,
                                 sizeof(battery_info),
                                 buffer,
                                 buffer_size,
                                 &bytes_returned);
}

bool BatteryEnumerator::GetStatus(BATTERY_STATUS& status) const
{
    BATTERY_WAIT_STATUS status_request;
    memset(&status_request, 0, sizeof(status_request));

    if (!GetBatteryTag(status_request.BatteryTag))
        return false;

    BATTERY_STATUS status_reply;
    memset(&status_reply, 0, sizeof(status_reply));

    DWORD bytes_returned = 0;

    if (!device_.SendIoControl(IOCTL_BATTERY_QUERY_STATUS,
                               &status_request, sizeof(status_request),
                               &status_reply, sizeof(status_reply),
                               &bytes_returned))
    {
        return false;
    }

    status = status_reply;
    return true;
}

std::string BatteryEnumerator::GetDeviceName() const
{
    WCHAR buffer[256] = { 0 };

    if (!GetBatteryInformation(BatteryDeviceName, buffer, sizeof(buffer)))
        return std::string();

    return UTF8fromUNICODE(buffer);
}

std::string BatteryEnumerator::GetManufacturer() const
{
    WCHAR buffer[256] = { 0 };

    if (!GetBatteryInformation(BatteryManufactureName, buffer, sizeof(buffer)))
        return std::string();

    return UTF8fromUNICODE(buffer);
}

std::string BatteryEnumerator::GetManufactureDate() const
{
    BATTERY_MANUFACTURE_DATE date;

    memset(&date, 0, sizeof(date));

    if (!GetBatteryInformation(BatteryManufactureDate, &date, sizeof(date)))
        return std::string();

    return StringPrintf("%u//%u//%u", date.Day, date.Month, date.Year);
}

std::string BatteryEnumerator::GetUniqueId() const
{
    WCHAR buffer[256] = { 0 };

    if (!GetBatteryInformation(BatteryUniqueID, buffer, sizeof(buffer)))
        return std::string();

    return UTF8fromUNICODE(buffer);
}

std::string BatteryEnumerator::GetSerialNumber() const
{
    WCHAR buffer[256] = { 0 };

    if (!GetBatteryInformation(BatterySerialNumber, buffer, sizeof(buffer)))
        return std::string();

    return UTF8fromUNICODE(buffer);
}

std::string BatteryEnumerator::GetTemperature() const
{
    WCHAR buffer[256] = { 0 };

    if (!GetBatteryInformation(BatteryTemperature, buffer, sizeof(buffer)))
        return std::string();

    return UTF8fromUNICODE(buffer);
}

int BatteryEnumerator::GetDesignCapacity() const
{
    BATTERY_INFORMATION battery_info;

    memset(&battery_info, 0, sizeof(battery_info));

    if (!GetBatteryInformation(BatteryInformation, &battery_info, sizeof(battery_info)))
        return 0;

    return battery_info.DesignedCapacity;
}

std::string BatteryEnumerator::GetType() const
{
    BATTERY_INFORMATION battery_info;

    memset(&battery_info, 0, sizeof(battery_info));

    if (!GetBatteryInformation(BatteryInformation, &battery_info, sizeof(battery_info)))
        return std::string();

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

    return std::string();
}

int BatteryEnumerator::GetFullChargedCapacity() const
{
    BATTERY_INFORMATION battery_info;

    memset(&battery_info, 0, sizeof(battery_info));

    if (!GetBatteryInformation(BatteryInformation, &battery_info, sizeof(battery_info)))
        return 0;

    return battery_info.FullChargedCapacity;
}

int BatteryEnumerator::GetDepreciation() const
{
    BATTERY_INFORMATION battery_info;

    memset(&battery_info, 0, sizeof(battery_info));

    if (!GetBatteryInformation(BatteryInformation, &battery_info, sizeof(battery_info)))
        return 0;

    const int percent = 100 - (battery_info.FullChargedCapacity * 100) /
        battery_info.DesignedCapacity;

    return (percent >= 0) ? percent : 0;
}

int BatteryEnumerator::GetCurrentCapacity() const
{
    BATTERY_STATUS status;

    memset(&status, 0, sizeof(status));

    if (!GetStatus(status))
        return 0;

    return status.Capacity;
}

int BatteryEnumerator::GetVoltage() const
{
    BATTERY_STATUS status;

    memset(&status, 0, sizeof(status));

    if (!GetStatus(status))
        return 0;

    return status.Voltage;
}

uint32_t BatteryEnumerator::GetState() const
{
    BATTERY_STATUS status;

    memset(&status, 0, sizeof(status));

    if (!GetStatus(status))
        return 0;

    return status.PowerState;
}

} // namespace aspia
