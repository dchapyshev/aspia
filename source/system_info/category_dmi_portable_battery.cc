//
// PROJECT:         Aspia
// FILE:            system_info/category_dmi_portable_battery.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "base/bitset.h"
#include "system_info/category_dmi_portable_battery.h"
#include "system_info/category_dmi_portable_battery.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* ChemistryToString(proto::DmiPortableBattery::Chemistry value)
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

proto::DmiPortableBattery::Chemistry GetChemistry(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiPortableBattery::CHEMISTRY_OTHER;
        case 0x02: return proto::DmiPortableBattery::CHEMISTRY_UNKNOWN;
        case 0x03: return proto::DmiPortableBattery::CHEMISTRY_LEAD_ACID;
        case 0x04: return proto::DmiPortableBattery::CHEMISTRY_NICKEL_CADMIUM;
        case 0x05: return proto::DmiPortableBattery::CHEMISTRY_NICKEL_METAL_HYDRIDE;
        case 0x06: return proto::DmiPortableBattery::CHEMISTRY_LITHIUM_ION;
        case 0x07: return proto::DmiPortableBattery::CHEMISTRY_ZINC_AIR;
        case 0x08: return proto::DmiPortableBattery::CHEMISTRY_LITHIUM_POLYMER;
        default: return proto::DmiPortableBattery::CHEMISTRY_UNKNOWN;
    }
}

} // namespace

CategoryDmiPortableBattery::CategoryDmiPortableBattery()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

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

    for (SMBios::TableEnumeratorNew table_enumerator(*smbios, SMBios::TABLE_TYPE_PORTABLE_BATTERY);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::TableReader table = table_enumerator.GetTable();
        if (table.GetTableLength() < 0x10)
            continue;

        proto::DmiPortableBattery::Item* item = message.add_item();

        item->set_location(table.GetString(0x04));
        item->set_manufacturer(table.GetString(0x05));
        item->set_manufacture_date(table.GetString(0x06));
        item->set_serial_number(table.GetString(0x07));
        item->set_device_name(table.GetString(0x08));
        item->set_chemistry(GetChemistry(table.GetByte(0x09)));

        if (table.GetTableLength() < 0x16)
            item->set_design_capacity(table.GetWord(0x0A));
        else
            item->set_design_capacity(table.GetWord(0x0A) + table.GetByte(0x15));

        item->set_design_voltage(table.GetWord(0x0C));

        item->set_sbds_version_number(table.GetString(0x0E));
        item->set_max_error_in_battery_data(table.GetByte(0x0F));

        if (table.GetTableLength() >= 0x1A)
        {
            item->set_sbds_serial_number(StringPrintf("%04X", table.GetWord(0x10)));

            BitSet<uint16_t> date = table.GetWord(0x12);

            item->set_sbds_manufacture_date(
                StringPrintf("%02u-%02u-%u",
                             date.Range(0, 4), // Day.
                             date.Range(5, 8), // Month.
                             1980U + date.Range(9, 15))); // Year.

            item->set_sbds_device_chemistry(table.GetString(0x14));
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
