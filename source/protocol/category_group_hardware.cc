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
// CategoryDmiOnboardDevices
//

const char* CategoryDmiOnboardDevices::Name() const
{
    return "Onboard Devices";
}

Category::IconId CategoryDmiOnboardDevices::Icon() const
{
    return IDI_MOTHERBOARD;
}

const char* CategoryDmiOnboardDevices::Guid() const
{
    return "{6C62195C-5E5F-41BA-B6AD-99041594DAC6}";
}

void CategoryDmiOnboardDevices::Parse(Table& table, const std::string& data)
{
    proto::DmiOnBoardDevices message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiOnBoardDevices::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("OnBoard Device #%d", index + 1));

        if (!item.description().empty())
            group.AddParam("Description", Value::String(item.description()));

        group.AddParam("Type", Value::String(TypeToString(item.type())));
        group.AddParam("Status", Value::String(item.enabled() ? "Enabled" : "Disabled"));
    }
}

std::string CategoryDmiOnboardDevices::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiOnBoardDevices message;

    for (SMBios::TableEnumerator<SMBios::OnBoardDeviceTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::OnBoardDeviceTable table = table_enumerator.GetTable();

        for (int index = 0; index < table.GetDeviceCount(); ++index)
        {
            proto::DmiOnBoardDevices::Item* item = message.add_item();

            item->set_description(table.GetDescription(index));
            item->set_type(table.GetType(index));
            item->set_enabled(table.IsEnabled(index));
        }
    }

    return message.SerializeAsString();
}

// static
const char* CategoryDmiOnboardDevices::TypeToString(proto::DmiOnBoardDevices::Type value)
{
    switch (value)
    {
        case proto::DmiOnBoardDevices::TYPE_OTHER:
            return "Other";

        case proto::DmiOnBoardDevices::TYPE_VIDEO:
            return "Video";

        case proto::DmiOnBoardDevices::TYPE_SCSI_CONTROLLER:
            return "SCSI Controller";

        case proto::DmiOnBoardDevices::TYPE_ETHERNET:
            return "Ethernet";

        case proto::DmiOnBoardDevices::TYPE_TOKEN_RING:
            return "Token Ring";

        case proto::DmiOnBoardDevices::TYPE_SOUND:
            return "Sound";

        case proto::DmiOnBoardDevices::TYPE_PATA_CONTROLLER:
            return "PATA Controller";

        case proto::DmiOnBoardDevices::TYPE_SATA_CONTROLLER:
            return "SATA Controller";

        case proto::DmiOnBoardDevices::TYPE_SAS_CONTROLLER:
            return "SAS Controller";

        default:
            return "Unknown";
    }
}

//
// CategoryDmiPointingDevice
//

const char* CategoryDmiPointingDevices::Name() const
{
    return "Pointing Devices";
}

Category::IconId CategoryDmiPointingDevices::Icon() const
{
    return IDI_MOUSE;
}

const char* CategoryDmiPointingDevices::Guid() const
{
    return "{6883684B-3CEC-451B-A2E3-34C16348BA1B}";
}

void CategoryDmiPointingDevices::Parse(Table& table, const std::string& data)
{
    proto::DmiPointingDevices message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiPointingDevices::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Build-in Pointing Device #%d", index + 1));

        group.AddParam("Device Type", Value::String(TypeToString(item.device_type())));
        group.AddParam("Device Interface", Value::String(InterfaceToString(item.device_interface())));
        group.AddParam("Buttons Count", Value::Number(item.button_count()));
    }
}

std::string CategoryDmiPointingDevices::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiPointingDevices message;

    for (SMBios::TableEnumerator<SMBios::PointingDeviceTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::PointingDeviceTable table = table_enumerator.GetTable();
        proto::DmiPointingDevices::Item* item = message.add_item();

        item->set_device_type(table.GetDeviceType());
        item->set_device_interface(table.GetInterface());
        item->set_button_count(table.GetButtonCount());
    }

    return message.SerializeAsString();
}

// static
const char* CategoryDmiPointingDevices::TypeToString(proto::DmiPointingDevices::Type value)
{
    switch (value)
    {
        case proto::DmiPointingDevices::TYPE_OTHER:
            return "Other";

        case proto::DmiPointingDevices::TYPE_MOUSE:
            return "Mouse";

        case proto::DmiPointingDevices::TYPE_TRACK_BALL:
            return "Track Ball";

        case proto::DmiPointingDevices::TYPE_TRACK_POINT:
            return "Track Point";

        case proto::DmiPointingDevices::TYPE_GLIDE_POINT:
            return "Glide Point";

        case proto::DmiPointingDevices::TYPE_TOUCH_PAD:
            return "Touch Pad";

        case proto::DmiPointingDevices::TYPE_TOUCH_SCREEN:
            return "Touch Screen";

        case proto::DmiPointingDevices::TYPE_OPTICAL_SENSOR:
            return "Optical Sensor";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiPointingDevices::InterfaceToString(
    proto::DmiPointingDevices::Interface value)
{
    switch (value)
    {
        case proto::DmiPointingDevices::INTERFACE_OTHER:
            return "Other";

        case proto::DmiPointingDevices::INTERFACE_SERIAL:
            return "Serial";

        case proto::DmiPointingDevices::INTERFACE_PS_2:
            return "PS/2";

        case proto::DmiPointingDevices::INTERFACE_INFRARED:
            return "Infrared";

        case proto::DmiPointingDevices::INTERFACE_HP_HIL:
            return "HP-HIL";

        case proto::DmiPointingDevices::INTERFACE_BUS_MOUSE:
            return "Bus mouse";

        case proto::DmiPointingDevices::INTERFACE_ADB:
            return "ADB (Apple Desktop Bus)";

        case proto::DmiPointingDevices::INTERFACE_BUS_MOUSE_DB_9:
            return "Bus mouse DB-9";

        case proto::DmiPointingDevices::INTERFACE_BUS_MOUSE_MICRO_DIN:
            return "Bus mouse micro-DIN";

        case proto::DmiPointingDevices::INTERFACE_USB:
            return "USB";

        default:
            return "Unknown";
    }
}

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
