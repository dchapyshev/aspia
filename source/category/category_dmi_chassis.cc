//
// PROJECT:         Aspia
// FILE:            category/category_dmi_chassis.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "category/category_dmi_chassis.h"
#include "category/category_dmi_chassis.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* TypeToString(proto::DmiChassis::Type type)
{
    switch (type)
    {
        case proto::DmiChassis::TYPE_OTHER:
            return "Other";

        case proto::DmiChassis::TYPE_DESKTOP:
            return "Desktop";

        case proto::DmiChassis::TYPE_LOW_PROFILE_DESKTOP:
            return "Low Profile Desktop";

        case proto::DmiChassis::TYPE_PIZZA_BOX:
            return "Pizza Box";

        case proto::DmiChassis::TYPE_MINI_TOWER:
            return "Mini Tower";

        case proto::DmiChassis::TYPE_TOWER:
            return "Tower";

        case proto::DmiChassis::TYPE_PORTABLE:
            return "Portable";

        case proto::DmiChassis::TYPE_LAPTOP:
            return "Laptop";

        case proto::DmiChassis::TYPE_NOTEBOOK:
            return "Notebook";

        case proto::DmiChassis::TYPE_HAND_HELD:
            return "Hand Held";

        case proto::DmiChassis::TYPE_DOCKING_STATION:
            return "Docking Station";

        case proto::DmiChassis::TYPE_ALL_IN_ONE:
            return "All In One";

        case proto::DmiChassis::TYPE_SUB_NOTEBOOK:
            return "Sub Notebook";

        case proto::DmiChassis::TYPE_SPACE_SAVING:
            return "Space Saving";

        case proto::DmiChassis::TYPE_LUNCH_BOX:
            return "Lunch Box";

        case proto::DmiChassis::TYPE_MAIN_SERVER_CHASSIS:
            return "Main Server Chassis";

        case proto::DmiChassis::TYPE_EXPANSION_CHASSIS:
            return "Expansion Chassis";

        case proto::DmiChassis::TYPE_SUB_CHASSIS:
            return "Sub Chassis";

        case proto::DmiChassis::TYPE_BUS_EXPANSION_CHASSIS:
            return "Bus Expansion Chassis";

        case proto::DmiChassis::TYPE_PERIPHERIAL_CHASSIS:
            return "Peripherial Chassis";

        case proto::DmiChassis::TYPE_RAID_CHASSIS:
            return "RAID Chassis";

        case proto::DmiChassis::TYPE_RACK_MOUNT_CHASSIS:
            return "Rack Mount Chassis";

        case proto::DmiChassis::TYPE_SEALED_CASE_PC:
            return "Sealed Case PC";

        case proto::DmiChassis::TYPE_MULTI_SYSTEM_CHASSIS:
            return "Multi System Chassis";

        case proto::DmiChassis::TYPE_COMPACT_PCI:
            return "Compact PCI";

        case proto::DmiChassis::TYPE_ADVANCED_TCA:
            return "Advanced TCA";

        case proto::DmiChassis::TYPE_BLADE:
            return "Blade";

        case proto::DmiChassis::TYPE_BLADE_ENCLOSURE:
            return "Blade Enclosure";

        default:
            return "Unknown";
    }
}

const char* StatusToString(proto::DmiChassis::Status status)
{
    switch (status)
    {
        case proto::DmiChassis::STATUS_OTHER:
            return "Other";

        case proto::DmiChassis::STATUS_SAFE:
            return "Safe";

        case proto::DmiChassis::STATUS_WARNING:
            return "Warning";

        case proto::DmiChassis::STATUS_CRITICAL:
            return "Critical";

        case proto::DmiChassis::STATUS_NON_RECOVERABLE:
            return "Non Recoverable";

        default:
            return "Unknown";
    }
}

const char* SecurityStatusToString(proto::DmiChassis::SecurityStatus status)
{
    switch (status)
    {
        case proto::DmiChassis::SECURITY_STATUS_OTHER:
            return "Other";

        case proto::DmiChassis::SECURITY_STATUS_NONE:
            return "None";

        case proto::DmiChassis::SECURITY_STATUS_EXTERNAL_INTERFACE_LOCKED_OUT:
            return "External Interface Locked Out";

        case proto::DmiChassis::SECURITY_STATUS_EXTERNAL_INTERFACE_ENABLED:
            return "External Interface Enabled";

        default:
            return "Unknown";
    }
}

proto::DmiChassis::Type GetType(const SMBios::Table& table)
{
    switch (table.Byte(0x05) & 0x7F)
    {
        case 0x03: return proto::DmiChassis::TYPE_DESKTOP;
        case 0x04: return proto::DmiChassis::TYPE_LOW_PROFILE_DESKTOP;
        case 0x05: return proto::DmiChassis::TYPE_PIZZA_BOX;
        case 0x06: return proto::DmiChassis::TYPE_MINI_TOWER;
        case 0x07: return proto::DmiChassis::TYPE_TOWER;
        case 0x08: return proto::DmiChassis::TYPE_PORTABLE;
        case 0x09: return proto::DmiChassis::TYPE_LAPTOP;
        case 0x0A: return proto::DmiChassis::TYPE_NOTEBOOK;
        case 0x0B: return proto::DmiChassis::TYPE_HAND_HELD;
        case 0x0C: return proto::DmiChassis::TYPE_DOCKING_STATION;
        case 0x0D: return proto::DmiChassis::TYPE_ALL_IN_ONE;
        case 0x0E: return proto::DmiChassis::TYPE_SUB_NOTEBOOK;
        case 0x0F: return proto::DmiChassis::TYPE_SPACE_SAVING;
        case 0x10: return proto::DmiChassis::TYPE_LUNCH_BOX;
        case 0x11: return proto::DmiChassis::TYPE_MAIN_SERVER_CHASSIS;
        case 0x12: return proto::DmiChassis::TYPE_EXPANSION_CHASSIS;
        case 0x13: return proto::DmiChassis::TYPE_SUB_CHASSIS;
        case 0x14: return proto::DmiChassis::TYPE_BUS_EXPANSION_CHASSIS;
        case 0x15: return proto::DmiChassis::TYPE_PERIPHERIAL_CHASSIS;
        case 0x16: return proto::DmiChassis::TYPE_RAID_CHASSIS;
        case 0x17: return proto::DmiChassis::TYPE_RACK_MOUNT_CHASSIS;
        case 0x18: return proto::DmiChassis::TYPE_SEALED_CASE_PC;
        case 0x19: return proto::DmiChassis::TYPE_MULTI_SYSTEM_CHASSIS;
        case 0x1A: return proto::DmiChassis::TYPE_COMPACT_PCI;
        case 0x1B: return proto::DmiChassis::TYPE_ADVANCED_TCA;
        case 0x1C: return proto::DmiChassis::TYPE_BLADE;
        case 0x1D: return proto::DmiChassis::TYPE_BLADE_ENCLOSURE;
        default: return proto::DmiChassis::TYPE_UNKNOWN;
    }
}

proto::DmiChassis::Status GetStatus(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiChassis::STATUS_OTHER;
        case 0x03: return proto::DmiChassis::STATUS_SAFE;
        case 0x04: return proto::DmiChassis::STATUS_WARNING;
        case 0x05: return proto::DmiChassis::STATUS_CRITICAL;
        case 0x06: return proto::DmiChassis::STATUS_NON_RECOVERABLE;
        default: return proto::DmiChassis::STATUS_UNKNOWN;
    }
}

proto::DmiChassis::SecurityStatus GetSecurityStatus(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiChassis::SECURITY_STATUS_OTHER;
        case 0x03: return proto::DmiChassis::SECURITY_STATUS_NONE;
        case 0x04: return proto::DmiChassis::SECURITY_STATUS_EXTERNAL_INTERFACE_LOCKED_OUT;
        case 0x05: return proto::DmiChassis::SECURITY_STATUS_EXTERNAL_INTERFACE_ENABLED;
        default: return proto::DmiChassis::SECURITY_STATUS_UNKNOWN;
    }
}

} // namespace

CategoryDmiChassis::CategoryDmiChassis()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryDmiChassis::Name() const
{
    return "Chassis";
}

Category::IconId CategoryDmiChassis::Icon() const
{
    return IDI_SERVER;
}

const char* CategoryDmiChassis::Guid() const
{
    return "{81D9E51F-4A86-49FC-A37F-232D6A62EC45}";
}

void CategoryDmiChassis::Parse(Table& table, const std::string& data)
{
    proto::DmiChassis message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiChassis::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Chassis #%d", index + 1));

        if (!item.manufacturer().empty())
            group.AddParam("Manufacturer", Value::String(item.manufacturer()));

        if (!item.version().empty())
            group.AddParam("Version", Value::String(item.version()));

        if (!item.serial_number().empty())
            group.AddParam("Serial Number", Value::String(item.serial_number()));

        if (!item.asset_tag().empty())
            group.AddParam("Asset Tag", Value::String(item.asset_tag()));

        group.AddParam("Type", Value::String(TypeToString(item.type())));
        group.AddParam("OS Load Status", Value::String(StatusToString(item.os_load_status())));
        group.AddParam("Power Source Status", Value::String(StatusToString(item.power_source_status())));
        group.AddParam("Temperature Status", Value::String(StatusToString(item.temparature_status())));
        group.AddParam("Security Status", Value::String(SecurityStatusToString(item.security_status())));

        if (item.height() != 0)
            group.AddParam("Height", Value::Number(item.height(), "U"));

        if (item.number_of_power_cords() != 0)
        {
            group.AddParam("Number Of Power Cords", Value::Number(item.number_of_power_cords()));
        }
    }
}

std::string CategoryDmiChassis::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiChassis message;

    for (SMBios::TableEnumerator table_enumerator(*smbios, SMBios::TABLE_TYPE_CHASSIS);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::Table table = table_enumerator.GetTable();

        if (table.Length() < 0x09)
            continue;

        proto::DmiChassis::Item* item = message.add_item();

        item->set_manufacturer(table.String(0x04));
        item->set_version(table.String(0x06));
        item->set_serial_number(table.String(0x07));
        item->set_asset_tag(table.String(0x08));
        item->set_type(GetType(table));

        if (table.Length() >= 0x0D)
        {
            item->set_os_load_status(GetStatus(table.Byte(0x09)));
            item->set_power_source_status(GetStatus(table.Byte(0x0A)));
            item->set_temparature_status(GetStatus(table.Byte(0x0B)));
            item->set_security_status(GetSecurityStatus(table.Byte(0x0C)));
        }

        if (table.Length() >= 0x13)
        {
            item->set_height(table.Byte(0x11));
            item->set_number_of_power_cords(table.Byte(0x12));
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
