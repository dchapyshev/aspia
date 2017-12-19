//
// PROJECT:         Aspia
// FILE:            system_info/category_dmi_baseboard.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "base/bitset.h"
#include "system_info/category_dmi_baseboard.h"
#include "system_info/category_dmi_baseboard.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* BoardTypeToString(proto::DmiBaseboard::BoardType type)
{
    switch (type)
    {
        case proto::DmiBaseboard::BOARD_TYPE_OTHER:
            return "Other";

        case proto::DmiBaseboard::BOARD_TYPE_SERVER_BLADE:
            return "Server Blade";

        case proto::DmiBaseboard::BOARD_TYPE_CONNECTIVITY_SWITCH:
            return "Connectivity Switch";

        case proto::DmiBaseboard::BOARD_TYPE_SYSTEM_MANAGEMENT_MODULE:
            return "System Management Module";

        case proto::DmiBaseboard::BOARD_TYPE_PROCESSOR_MODULE:
            return "Processor Module";

        case proto::DmiBaseboard::BOARD_TYPE_IO_MODULE:
            return "I/O Module";

        case proto::DmiBaseboard::BOARD_TYPE_MEMORY_MODULE:
            return "Memory Module";

        case proto::DmiBaseboard::BOARD_TYPE_DAUGHTER_BOARD:
            return "Daughter Board";

        case proto::DmiBaseboard::BOARD_TYPE_MOTHERBOARD:
            return "Motherboard";

        case proto::DmiBaseboard::BOARD_TYPE_PROCESSOR_PLUS_MEMORY_MODULE:
            return "Processor + Memory Module";

        case proto::DmiBaseboard::BOARD_TYPE_PROCESSOR_PLUS_IO_MODULE:
            return "Processor + I/O Module";

        case proto::DmiBaseboard::BOARD_TYPE_INTERCONNECT_BOARD:
            return "Interconnect Board";

        default:
            return "Unknown";
    }
}

proto::DmiBaseboard::BoardType GetBoardType(const SMBios::Table& table)
{
    if (table.Length() < 0x0E)
        return proto::DmiBaseboard::BOARD_TYPE_UNKNOWN;

    switch (table.Byte(0x0D))
    {
        case 0x02: return proto::DmiBaseboard::BOARD_TYPE_OTHER;
        case 0x03: return proto::DmiBaseboard::BOARD_TYPE_SERVER_BLADE;
        case 0x04: return proto::DmiBaseboard::BOARD_TYPE_CONNECTIVITY_SWITCH;
        case 0x05: return proto::DmiBaseboard::BOARD_TYPE_SYSTEM_MANAGEMENT_MODULE;
        case 0x06: return proto::DmiBaseboard::BOARD_TYPE_PROCESSOR_MODULE;
        case 0x07: return proto::DmiBaseboard::BOARD_TYPE_IO_MODULE;
        case 0x08: return proto::DmiBaseboard::BOARD_TYPE_MEMORY_MODULE;
        case 0x09: return proto::DmiBaseboard::BOARD_TYPE_DAUGHTER_BOARD;
        case 0x0A: return proto::DmiBaseboard::BOARD_TYPE_MOTHERBOARD;
        case 0x0B: return proto::DmiBaseboard::BOARD_TYPE_PROCESSOR_PLUS_MEMORY_MODULE;
        case 0x0C: return proto::DmiBaseboard::BOARD_TYPE_PROCESSOR_PLUS_IO_MODULE;
        case 0x0D: return proto::DmiBaseboard::BOARD_TYPE_INTERCONNECT_BOARD;
        default: return proto::DmiBaseboard::BOARD_TYPE_UNKNOWN;
    }
}

} // namespace

CategoryDmiBaseboard::CategoryDmiBaseboard()
    : CategoryInfo((Type::INFO_PARAM_VALUE))
{
    // Nothing
}

const char* CategoryDmiBaseboard::Name() const
{
    return "Baseboard";
}

Category::IconId CategoryDmiBaseboard::Icon() const
{
    return IDI_MOTHERBOARD;
}

const char* CategoryDmiBaseboard::Guid() const
{
    return "{8143642D-3248-40F5-8FCF-629C581FFF01}";
}

void CategoryDmiBaseboard::Parse(Table& table, const std::string& data)
{
    proto::DmiBaseboard message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 300)
                     .AddColumn("Value", 260));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiBaseboard::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Baseboard #%d", index + 1));

        group.AddParam("Type", Value::String(BoardTypeToString(item.type())));

        if (!item.manufacturer().empty())
            group.AddParam("Manufacturer", Value::String(item.manufacturer()));

        if (!item.product_name().empty())
            group.AddParam("Product Name", Value::String(item.product_name()));

        if (!item.version().empty())
            group.AddParam("Version", Value::String(item.version()));

        if (!item.serial_number().empty())
            group.AddParam("Serial Number", Value::String(item.serial_number()));

        if (!item.asset_tag().empty())
            group.AddParam("Asset Tag", Value::String(item.asset_tag()));

        if (!item.location_in_chassis().empty())
            group.AddParam("Location in chassis", Value::String(item.location_in_chassis()));

        if (item.has_features())
        {
            Group features_group = group.AddGroup("Supported Features");
            const proto::DmiBaseboard::Features& features = item.features();

            features_group.AddParam("Board is a hosting board",
                                    Value::Bool(features.is_hosting_board()));
            features_group.AddParam("Board requires at least one daughter board",
                                    Value::Bool(features.is_requires_at_least_one_daughter_board()));
            features_group.AddParam("Board is removable",
                                    Value::Bool(features.is_removable()));
            features_group.AddParam("Board is replaceable",
                                    Value::Bool(features.is_replaceable()));
            features_group.AddParam("Board is hot swappable",
                                    Value::Bool(features.is_hot_swappable()));
        }
    }
}

std::string CategoryDmiBaseboard::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiBaseboard message;

    for (SMBios::TableEnumerator table_enumerator(*smbios, SMBios::TABLE_TYPE_BASEBOARD);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::Table table = table_enumerator.GetTable();
        proto::DmiBaseboard::Item* item = message.add_item();

        item->set_manufacturer(table.String(0x04));
        item->set_product_name(table.String(0x05));
        item->set_version(table.String(0x06));
        item->set_serial_number(table.String(0x07));

        if (table.Length() >= 0x09)
            item->set_asset_tag(table.String(0x08));

        if (table.Length() >= 0x0E)
            item->set_location_in_chassis(table.String(0x0A));

        item->set_type(GetBoardType(table));

        if (table.Length() >= 0x0A)
        {
            proto::DmiBaseboard::Features* features = item->mutable_features();
            BitSet<uint8_t> flags = table.Byte(0x09);

            features->set_is_hosting_board(flags.Test(0));
            features->set_is_requires_at_least_one_daughter_board(flags.Test(1));
            features->set_is_removable(flags.Test(2));
            features->set_is_replaceable(flags.Test(3));
            features->set_is_hot_swappable(flags.Test(4));
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
