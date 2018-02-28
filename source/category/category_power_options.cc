//
// PROJECT:         Aspia
// FILE:            category/category_power_options.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/device.h"
#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/scoped_device_info.h"
#include "base/logging.h"
#include "category/category_power_options.h"
#include "category/category_power_options.pb.h"
#include "ui/resource.h"

#include <batclass.h>
#include <winioctl.h>
#include <devguid.h>

namespace aspia {

namespace {

bool GetBatteryTag(Device& battery, ULONG& tag)
{
    ULONG bytes_returned;
    ULONG input_buffer = 0;
    ULONG output_buffer = 0;

    if (!battery.IoControl(IOCTL_BATTERY_QUERY_TAG,
                           &input_buffer, sizeof(input_buffer),
                           &output_buffer, sizeof(output_buffer),
                           &bytes_returned))
    {
        return false;
    }

    tag = output_buffer;
    return true;
}


bool GetBatteryInformation(Device& battery,
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

    return battery.IoControl(IOCTL_BATTERY_QUERY_INFORMATION,
                             &battery_info, sizeof(battery_info),
                             buffer, buffer_size,
                             &bytes_returned);
}

bool GetBatteryStatus(Device& battery, ULONG tag, BATTERY_STATUS& status)
{
    BATTERY_WAIT_STATUS status_request;
    memset(&status_request, 0, sizeof(status_request));
    status_request.BatteryTag = tag;

    BATTERY_STATUS status_reply;
    memset(&status_reply, 0, sizeof(status_reply));

    DWORD bytes_returned = 0;

    if (!battery.IoControl(IOCTL_BATTERY_QUERY_STATUS,
                           &status_request, sizeof(status_request),
                           &status_reply, sizeof(status_reply),
                           &bytes_returned))
    {
        return false;
    }

    status = status_reply;
    return true;
}

std::string GetBatteryName(Device& battery, ULONG tag)
{
    wchar_t buffer[256] = { 0 };

    if (!GetBatteryInformation(battery, tag, BatteryDeviceName, buffer, sizeof(buffer)))
        return std::string();

    return UTF8fromUNICODE(buffer);
}

std::string GetBatteryManufacturer(Device& battery, ULONG tag)
{
    wchar_t buffer[256] = { 0 };

    if (!GetBatteryInformation(battery, tag, BatteryManufactureName, buffer, sizeof(buffer)))
        return std::string();

    return UTF8fromUNICODE(buffer);
}

std::string GetBatteryManufactureDate(Device& battery, ULONG tag)
{
    BATTERY_MANUFACTURE_DATE date;

    memset(&date, 0, sizeof(date));

    if (!GetBatteryInformation(battery, tag, BatteryManufactureDate, &date, sizeof(date)))
        return std::string();

    return StringPrintf("%u-%u-%u", date.Day, date.Month, date.Year);
}

std::string GetBatteryUniqueId(Device& battery, ULONG tag)
{
    wchar_t buffer[256] = { 0 };

    if (!GetBatteryInformation(battery, tag, BatteryUniqueID, buffer, sizeof(buffer)))
        return std::string();

    return UTF8fromUNICODE(buffer);
}

std::string GetBatterySerialNumber(Device& battery, ULONG tag)
{
    wchar_t buffer[256] = { 0 };

    if (!GetBatteryInformation(battery, tag, BatterySerialNumber, buffer, sizeof(buffer)))
        return std::string();

    return UTF8fromUNICODE(buffer);
}

std::string GetBatteryTemperature(Device& battery, ULONG tag)
{
    wchar_t buffer[256] = { 0 };

    if (!GetBatteryInformation(battery, tag, BatteryTemperature, buffer, sizeof(buffer)))
        return std::string();

    return UTF8fromUNICODE(buffer);
}

int GetBatteryDesignCapacity(Device& battery, ULONG tag)
{
    BATTERY_INFORMATION battery_info;

    memset(&battery_info, 0, sizeof(battery_info));

    if (!GetBatteryInformation(battery, tag, BatteryInformation,
                               &battery_info, sizeof(battery_info)))
        return 0;

    return battery_info.DesignedCapacity;
}

proto::PowerOptions::Battery::Type GetBatteryType(Device& battery, ULONG tag)
{
    BATTERY_INFORMATION battery_info;
    memset(&battery_info, 0, sizeof(battery_info));

    if (GetBatteryInformation(battery, tag, BatteryInformation,
                              &battery_info, sizeof(battery_info)))
    {
        if (memcmp(&battery_info.Chemistry[0], "PbAc", 4) == 0)
            return proto::PowerOptions::Battery::TYPE_PBAC;

        if (memcmp(&battery_info.Chemistry[0], "LION", 4) == 0 ||
            memcmp(&battery_info.Chemistry[0], "Li-I", 4) == 0)
            return proto::PowerOptions::Battery::TYPE_LION;

        if (memcmp(&battery_info.Chemistry[0], "NiCd", 4) == 0)
            return proto::PowerOptions::Battery::TYPE_NICD;

        if (memcmp(&battery_info.Chemistry[0], "NiMH", 4) == 0)
            return proto::PowerOptions::Battery::TYPE_NIMH;

        if (memcmp(&battery_info.Chemistry[0], "NiZn", 4) == 0)
            return proto::PowerOptions::Battery::TYPE_NIZN;

        if (memcmp(&battery_info.Chemistry[0], "RAM", 3) == 0)
            return proto::PowerOptions::Battery::TYPE_RAM;
    }

    return proto::PowerOptions::Battery::TYPE_UNKNOWN;
}

int GetBatteryFullChargedCapacity(Device& battery, ULONG tag)
{
    BATTERY_INFORMATION battery_info;
    memset(&battery_info, 0, sizeof(battery_info));

    if (!GetBatteryInformation(battery, tag, BatteryInformation,
                               &battery_info, sizeof(battery_info)))
        return 0;

    return battery_info.FullChargedCapacity;
}

int GetBatteryDepreciation(Device& battery, ULONG tag)
{
    BATTERY_INFORMATION battery_info;
    memset(&battery_info, 0, sizeof(battery_info));

    if (!GetBatteryInformation(battery, tag, BatteryInformation,
                               &battery_info, sizeof(battery_info)) ||
        battery_info.DesignedCapacity == 0)
    {
        return 0;
    }

    const int percent = 100 - (battery_info.FullChargedCapacity * 100) /
        battery_info.DesignedCapacity;

    return (percent >= 0) ? percent : 0;
}

void AddBattery(proto::PowerOptions& message, const wchar_t* device_path)
{
    Device battery;
    if (!battery.Open(device_path))
        return;

    ULONG tag;
    if (!GetBatteryTag(battery, tag))
        return;

    proto::PowerOptions::Battery* item = message.add_battery();

    item->set_device_name(GetBatteryName(battery, tag));
    item->set_manufacturer(GetBatteryManufacturer(battery, tag));
    item->set_manufacture_date(GetBatteryManufactureDate(battery, tag));
    item->set_unique_id(GetBatteryUniqueId(battery, tag));
    item->set_serial_number(GetBatterySerialNumber(battery, tag));
    item->set_temperature(GetBatteryTemperature(battery, tag));
    item->set_design_capacity(GetBatteryDesignCapacity(battery, tag));
    item->set_type(GetBatteryType(battery, tag));
    item->set_full_charged_capacity(GetBatteryFullChargedCapacity(battery, tag));
    item->set_depreciation(GetBatteryDepreciation(battery, tag));

    BATTERY_STATUS battery_status;
    if (GetBatteryStatus(battery, tag, battery_status))
    {
        item->set_current_capacity(battery_status.Capacity);
        item->set_voltage(battery_status.Voltage);

        if (battery_status.PowerState & BATTERY_CHARGING)
            item->set_state(item->state() | proto::PowerOptions::Battery::STATE_CHARGING);

        if (battery_status.PowerState & BATTERY_CRITICAL)
            item->set_state(item->state() | proto::PowerOptions::Battery::STATE_CRITICAL);

        if (battery_status.PowerState & BATTERY_DISCHARGING)
            item->set_state(item->state() | proto::PowerOptions::Battery::STATE_DISCHARGING);

        if (battery_status.PowerState & BATTERY_POWER_ON_LINE)
            item->set_state(item->state() | proto::PowerOptions::Battery::STATE_POWER_ONLINE);
    }
}

const char* BatteryTypeToString(proto::PowerOptions::Battery::Type value)
{
    switch (value)
    {
        case proto::PowerOptions::Battery::TYPE_PBAC:
            return "Lead Acid";

        case proto::PowerOptions::Battery::TYPE_LION:
            return "Lithium Ion";

        case proto::PowerOptions::Battery::TYPE_NICD:
            return "Nickel Cadmium";

        case proto::PowerOptions::Battery::TYPE_NIMH:
            return "Nickel Metal Hydride";

        case proto::PowerOptions::Battery::TYPE_NIZN:
            return "Nickel Zinc";

        case proto::PowerOptions::Battery::TYPE_RAM:
            return "Rechargeable Alkaline-Manganese";

        default:
            return "Unknown";
    }
}

const char* BatteryStatusToString(proto::PowerOptions::BatteryStatus value)
{
    switch (value)
    {
        case proto::PowerOptions::BATTERY_STATUS_HIGH:
            return "High";

        case proto::PowerOptions::BATTERY_STATUS_LOW:
            return "Low";

        case proto::PowerOptions::BATTERY_STATUS_CRITICAL:
            return "Critical";

        case proto::PowerOptions::BATTERY_STATUS_CHARGING:
            return "Charging";

        case proto::PowerOptions::BATTERY_STATUS_NO_BATTERY:
            return "No Battery";

        default:
            return "Unknown";
    }
}

const char* PowerSourceToString(proto::PowerOptions::PowerSource value)
{
    switch (value)
    {
        case proto::PowerOptions::POWER_SOURCE_DC_BATTERY:
            return "DC Battery";

        case proto::PowerOptions::POWER_SOURCE_AC_LINE:
            return "AC Line";

        default:
            return "Unknown";
    }
}

} // namespace

CategoryPowerOptions::CategoryPowerOptions()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryPowerOptions::Name() const
{
    return "Power Options";
}

Category::IconId CategoryPowerOptions::Icon() const
{
    return IDI_POWER_SUPPLY;
}

const char* CategoryPowerOptions::Guid() const
{
    return "{42E04A9E-36F7-42A1-BCDA-F3ED70112DFF}";
}

void CategoryPowerOptions::Parse(Table& table, const std::string& data)
{
    proto::PowerOptions message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    table.AddParam("Power Source", Value::String(PowerSourceToString(message.power_source())));
    table.AddParam("Battery Status", Value::String(BatteryStatusToString(message.battery_status())));

    if (message.battery_status() != proto::PowerOptions::BATTERY_STATUS_NO_BATTERY &&
        message.battery_status() != proto::PowerOptions::BATTERY_STATUS_UNKNOWN)
    {
        table.AddParam("Battery Life Percent",
                       Value::Number(message.battery_life_percent(), "%"));

        table.AddParam("Full Battery Life Time",
            (message.full_battery_life_time() != 0) ?
                Value::Number(message.full_battery_life_time(), "s") : Value::String("Unknown"));

        table.AddParam("Remaining Battery Life Time",
            (message.remaining_battery_life_time() != 0) ?
                Value::Number(message.remaining_battery_life_time(), "s") : Value::String("Unknown"));
    }

    for (int index = 0; index < message.battery_size(); ++index)
    {
        const proto::PowerOptions::Battery& battery = message.battery(index);

        Group group = table.AddGroup(StringPrintf("Battery #%d", index + 1));

        if (!battery.device_name().empty())
            group.AddParam("Device Name", Value::String(battery.device_name()));

        if (!battery.manufacturer().empty())
            group.AddParam("Manufacturer", Value::String(battery.manufacturer()));

        if (!battery.manufacture_date().empty())
            group.AddParam("Manufacture Date", Value::String(battery.manufacture_date()));

        if (!battery.unique_id().empty())
            group.AddParam("Unique Id", Value::String(battery.unique_id()));

        if (!battery.serial_number().empty())
            group.AddParam("Serial Number", Value::String(battery.serial_number()));

        if (!battery.temperature().empty())
            group.AddParam("Tempareture", Value::String(battery.temperature()));

        if (battery.design_capacity() != 0)
        {
            group.AddParam("Design Capacity", Value::Number(battery.design_capacity(), "mWh"));
        }

        group.AddParam("Type", Value::String(BatteryTypeToString(battery.type())));

        if (battery.full_charged_capacity() != 0)
        {
            group.AddParam("Full Charged Capacity",
                           Value::Number(battery.full_charged_capacity(), "mWh"));
        }

        if (battery.depreciation() != 0)
            group.AddParam("Depreciation", Value::Number(battery.depreciation(), "%"));

        if (battery.current_capacity() != 0)
            group.AddParam("Current Capacity", Value::Number(battery.current_capacity(), "mWh"));

        if (battery.voltage() != 0)
            group.AddParam("Voltage", Value::Number(battery.voltage(), "mV"));

        if (battery.state() != 0)
        {
            Group state_group = group.AddGroup("State");

            if (battery.state() & proto::PowerOptions::Battery::STATE_CHARGING)
                state_group.AddParam("Charging", Value::String("Yes"));

            if (battery.state() & proto::PowerOptions::Battery::STATE_CRITICAL)
                state_group.AddParam("Critical", Value::String("Yes"));

            if (battery.state() & proto::PowerOptions::Battery::STATE_DISCHARGING)
                state_group.AddParam("Discharging", Value::String("Yes"));

            if (battery.state() & proto::PowerOptions::Battery::STATE_POWER_ONLINE)
                state_group.AddParam("Power OnLine", Value::String("Yes"));
        }
    }
}

std::string CategoryPowerOptions::Serialize()
{
    proto::PowerOptions message;

    SYSTEM_POWER_STATUS power_status;
    memset(&power_status, 0, sizeof(power_status));

    if (GetSystemPowerStatus(&power_status))
    {
        switch (power_status.ACLineStatus)
        {
            case 0:
                message.set_power_source(proto::PowerOptions::POWER_SOURCE_DC_BATTERY);
                break;

            case 1:
                message.set_power_source(proto::PowerOptions::POWER_SOURCE_AC_LINE);
                break;

            default:
                break;
        }

        switch (power_status.BatteryFlag)
        {
            case 1:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_HIGH);
                break;

            case 2:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_LOW);
                break;

            case 4:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_CRITICAL);
                break;

            case 8:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_CHARGING);
                break;

            case 128:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_NO_BATTERY);
                break;

            default:
                break;
        }

        if (power_status.BatteryFlag != 128)
        {
            message.set_battery_life_percent(power_status.BatteryLifePercent);

            if (power_status.BatteryFullLifeTime != -1)
                message.set_full_battery_life_time(power_status.BatteryFullLifeTime);

            if (power_status.BatteryLifeTime != -1)
                message.set_remaining_battery_life_time(power_status.BatteryLifeTime);
        }
    }

    const DWORD flags = DIGCF_PROFILE | DIGCF_PRESENT | DIGCF_DEVICEINTERFACE;

    ScopedDeviceInfo device_info(
        SetupDiGetClassDevsW(&GUID_DEVCLASS_BATTERY, nullptr, nullptr, flags));

    if (device_info.IsValid())
    {
        DWORD device_index = 0;

        for (;;)
        {
            SP_DEVICE_INTERFACE_DATA device_iface_data;

            memset(&device_iface_data, 0, sizeof(device_iface_data));
            device_iface_data.cbSize = sizeof(device_iface_data);

            if (!SetupDiEnumDeviceInterfaces(device_info,
                                             nullptr,
                                             &GUID_DEVCLASS_BATTERY,
                                             device_index,
                                             &device_iface_data))
            {
                SystemErrorCode error_code = GetLastError();

                if (error_code != ERROR_NO_MORE_ITEMS)
                {
                    DLOG(LS_WARNING) << "SetupDiEnumDeviceInfo failed: "
                                     << SystemErrorCodeToString(error_code);
                }

                break;
            }

            DWORD required_size = 0;

            if (SetupDiGetDeviceInterfaceDetailW(device_info,
                                                 &device_iface_data,
                                                 nullptr,
                                                 0,
                                                 &required_size,
                                                 nullptr) ||
                GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            {
                DPLOG(LS_WARNING) << "Unexpected return value";
                break;
            }

            std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(required_size);
            PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail_data =
                reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.get());

            detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

            if (!SetupDiGetDeviceInterfaceDetailW(device_info,
                                                  &device_iface_data,
                                                  detail_data,
                                                  required_size,
                                                  &required_size,
                                                  nullptr))
            {
                DPLOG(LS_WARNING) << "SetupDiGetDeviceInterfaceDetailW failed";
                break;
            }

            AddBattery(message, detail_data->DevicePath);

            ++device_index;
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
