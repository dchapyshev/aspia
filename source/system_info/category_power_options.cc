//
// PROJECT:         Aspia
// FILE:            system_info/category_power_options.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/battery_enumerator.h"
#include "base/strings/string_printf.h"
#include "system_info/category_power_options.h"
#include "system_info/category_power_options.pb.h"
#include "ui/resource.h"

namespace aspia {

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

    const char* power_source;
    switch (message.power_source())
    {
        case proto::PowerOptions::POWER_SOURCE_DC_BATTERY:
            power_source = "DC Battery";
            break;

        case proto::PowerOptions::POWER_SOURCE_AC_LINE:
            power_source = "AC Line";
            break;

        default:
            power_source = "Unknown";
            break;
    }

    table.AddParam("Power Source", Value::String(power_source));

    const char* battery_status;
    switch (message.battery_status())
    {
        case proto::PowerOptions::BATTERY_STATUS_HIGH:
            battery_status = "High";
            break;

        case proto::PowerOptions::BATTERY_STATUS_LOW:
            battery_status = "Low";
            break;

        case proto::PowerOptions::BATTERY_STATUS_CRITICAL:
            battery_status = "Critical";
            break;

        case proto::PowerOptions::BATTERY_STATUS_CHARGING:
            battery_status = "Charging";
            break;

        case proto::PowerOptions::BATTERY_STATUS_NO_BATTERY:
            battery_status = "No Battery";
            break;

        default:
            battery_status = "Unknown";
            break;
    }

    table.AddParam("Battery Status", Value::String(battery_status));

    if (message.battery_status() != proto::PowerOptions::BATTERY_STATUS_NO_BATTERY &&
        message.battery_status() != proto::PowerOptions::BATTERY_STATUS_UNKNOWN)
    {
        table.AddParam("Battery Life Percent",
                       Value::Number(message.battery_life_percent(), "%"));

        table.AddParam("Full Battery Life Time",
                       Value::Number(message.full_battery_life_time(), "s"));

        table.AddParam("Remaining Battery Life Time",
                       Value::Number(message.remaining_battery_life_time(), "s"));
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

        if (!battery.type().empty())
            group.AddParam("Type", Value::String(battery.type()));

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

    for (BatteryEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::PowerOptions::Battery* battery = message.add_battery();

        battery->set_device_name(enumerator.GetDeviceName());
        battery->set_manufacturer(enumerator.GetManufacturer());
        battery->set_manufacture_date(enumerator.GetManufactureDate());
        battery->set_unique_id(enumerator.GetUniqueId());
        battery->set_serial_number(enumerator.GetSerialNumber());
        battery->set_temperature(enumerator.GetTemperature());
        battery->set_design_capacity(enumerator.GetDesignCapacity());
        battery->set_type(enumerator.GetType());
        battery->set_full_charged_capacity(enumerator.GetFullChargedCapacity());
        battery->set_depreciation(enumerator.GetDepreciation());
        battery->set_current_capacity(enumerator.GetCurrentCapacity());
        battery->set_voltage(enumerator.GetVoltage());

        const uint32_t state = enumerator.GetState();

        if (state & BatteryEnumerator::STATE_CHARGING)
        {
            battery->set_state(
                battery->state() | proto::PowerOptions::Battery::STATE_CHARGING);
        }

        if (state & BatteryEnumerator::STATE_CRITICAL)
        {
            battery->set_state(
                battery->state() | proto::PowerOptions::Battery::STATE_CRITICAL);
        }

        if (state & BatteryEnumerator::STATE_DISCHARGING)
        {
            battery->set_state(
                battery->state() | proto::PowerOptions::Battery::STATE_DISCHARGING);
        }

        if (state & BatteryEnumerator::STATE_POWER_ONLINE)
        {
            battery->set_state(
                battery->state() | proto::PowerOptions::Battery::STATE_POWER_ONLINE);
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
