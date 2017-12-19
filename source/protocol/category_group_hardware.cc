//
// PROJECT:         Aspia
// FILE:            protocol/category_group_hardware.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/printer_enumerator.h"
#include "base/devices/battery_enumerator.h"
#include "base/devices/monitor_enumerator.h"
#include "base/devices/video_adapter_enumarator.h"
#include "base/devices/physical_drive_enumerator.h"
#include "base/files/logical_drive_enumerator.h"
#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/bitset.h"
#include "base/cpu_info.h"
#include "protocol/category_group_hardware.h"
#include "ui/system_info/output.h"
#include "ui/resource.h"

namespace aspia {

//
// CategoryDmiPortableBattery
//

const char* CategoryDmiPortableBattery::Name() const
{
    return "Portable Battery";
}

Category::IconId CategoryDmiPortableBattery::Icon() const
{
    return IDI_BATTERY;
}

const char* CategoryDmiPortableBattery::Guid() const
{
    return "{0CA213B5-12EE-4828-A399-BA65244E65FD}";
}

void CategoryDmiPortableBattery::Parse(Table& table, const std::string& data)
{
    proto::DmiPortableBattery message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiPortableBattery::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Portable Battery #%d", index + 1));

        if (!item.location().empty())
            group.AddParam("Location", Value::String(item.location()));

        if (!item.manufacturer().empty())
            group.AddParam("Manufacturer", Value::String(item.manufacturer()));

        if (!item.manufacture_date().empty())
            group.AddParam("Manufacture Date", Value::String(item.manufacture_date()));

        if (!item.sbds_serial_number().empty())
            group.AddParam("Serial Number", Value::String(item.sbds_serial_number()));

        if (!item.device_name().empty())
            group.AddParam("Device Name", Value::String(item.device_name()));

        group.AddParam("Chemistry", Value::String(ChemistryToString(item.chemistry())));

        if (item.design_capacity() != 0)
        {
            group.AddParam("Design Capacity", Value::Number(item.design_capacity(), "mWh"));
        }

        if (item.design_voltage() != 0)
        {
            group.AddParam("Design Voltage", Value::Number(item.design_voltage(), "mV"));
        }

        if (item.max_error_in_battery_data() != 0)
        {
            group.AddParam("Max. Error in Battery Data",
                           Value::Number(item.max_error_in_battery_data(), "%"));
        }

        if (!item.sbds_version_number().empty())
            group.AddParam("SBDS Version Number", Value::String(item.sbds_version_number()));

        if (!item.sbds_serial_number().empty())
            group.AddParam("SBDS Serial Number", Value::String(item.sbds_serial_number()));

        if (!item.sbds_manufacture_date().empty())
            group.AddParam("SBDS Manufacture Date", Value::String(item.sbds_manufacture_date()));

        if (!item.sbds_device_chemistry().empty())
            group.AddParam("SBDS Device Chemistry", Value::String(item.sbds_device_chemistry()));
    }
}

std::string CategoryDmiPortableBattery::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiPortableBattery message;

    for (SMBios::TableEnumerator<SMBios::PortableBatteryTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::PortableBatteryTable table = table_enumerator.GetTable();
        proto::DmiPortableBattery::Item* item = message.add_item();

        item->set_location(table.GetLocation());
        item->set_manufacturer(table.GetManufacturer());
        item->set_manufacture_date(table.GetManufactureDate());
        item->set_serial_number(table.GetSerialNumber());
        item->set_device_name(table.GetDeviceName());
        item->set_chemistry(table.GetChemistry());
        item->set_design_capacity(table.GetDesignCapacity());
        item->set_design_voltage(table.GetDesignVoltage());
        item->set_sbds_version_number(table.GetSBDSVersionNumber());
        item->set_max_error_in_battery_data(table.GetMaxErrorInBatteryData());
        item->set_sbds_serial_number(table.GetSBDSSerialNumber());
        item->set_sbds_manufacture_date(table.GetSBDSManufactureDate());
        item->set_sbds_device_chemistry(table.GetSBDSDeviceChemistry());
    }

    return message.SerializeAsString();
}

// static
const char* CategoryDmiPortableBattery::ChemistryToString(
    proto::DmiPortableBattery::Chemistry value)
{
    switch (value)
    {
        case proto::DmiPortableBattery::CHEMISTRY_OTHER:
            return "Other";

        case proto::DmiPortableBattery::CHEMISTRY_LEAD_ACID:
            return "Lead Acid";

        case proto::DmiPortableBattery::CHEMISTRY_NICKEL_CADMIUM:
            return "Nickel Cadmium";

        case proto::DmiPortableBattery::CHEMISTRY_NICKEL_METAL_HYDRIDE:
            return "Nickel Metal Hydride";

        case proto::DmiPortableBattery::CHEMISTRY_LITHIUM_ION:
            return "Lithium-ion";

        case proto::DmiPortableBattery::CHEMISTRY_ZINC_AIR:
            return "Zinc Air";

        case proto::DmiPortableBattery::CHEMISTRY_LITHIUM_POLYMER:
            return "Lithium Polymer";

        default:
            return "Unknown";
    }
}

//
// CategoryGroupDMI
//

const char* CategoryGroupDMI::Name() const
{
    return "DMI";
}

Category::IconId CategoryGroupDMI::Icon() const
{
    return IDI_COMPUTER;
}

//
// CategoryGroupStorage
//

const char* CategoryGroupStorage::Name() const
{
    return "Storage";
}

Category::IconId CategoryGroupStorage::Icon() const
{
    return IDI_DRIVE;
}

//
// CategoryGroupDisplay
//

const char* CategoryGroupDisplay::Name() const
{
    return "Display";
}

Category::IconId CategoryGroupDisplay::Icon() const
{
    return IDI_MONITOR;
}

//
// CategoryGroupHardware
//

const char* CategoryGroupHardware::Name() const
{
    return "Hardware";
}

Category::IconId CategoryGroupHardware::Icon() const
{
    return IDI_HARDWARE;
}

} // namespace aspia
