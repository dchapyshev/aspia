//
// PROJECT:         Aspia
// FILE:            category/category_dmi_cache.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "base/bitset.h"
#include "category/category_dmi_cache.h"
#include "category/category_dmi_cache.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* LocationToString(proto::DmiCaches::Location value)
{
    switch (value)
    {
        case proto::DmiCaches::LOCATION_INTERNAL:
            return "Internal";

        case proto::DmiCaches::LOCATION_EXTERNAL:
            return "External";

        case proto::DmiCaches::LOCATION_RESERVED:
            return "Reserved";

        default:
            return "Unknown";
    }
}

const char* StatusToString(proto::DmiCaches::Status value)
{
    switch (value)
    {
        case proto::DmiCaches::STATUS_ENABLED:
            return "Enabled";

        case proto::DmiCaches::STATUS_DISABLED:
            return "Disabled";

        default:
            return "Unknown";
    }
}

const char* ModeToString(proto::DmiCaches::Mode value)
{
    switch (value)
    {
        case proto::DmiCaches::MODE_WRITE_THRU:
            return "Write Thru";

        case proto::DmiCaches::MODE_WRITE_BACK:
            return "Write Back";

        case proto::DmiCaches::MODE_WRITE_WITH_MEMORY_ADDRESS:
            return "Write with memory address";

        default:
            return "Unknown";
    }
}

const char* SRAMTypeToString(proto::DmiCaches::SRAMType value)
{
    switch (value)
    {
        case proto::DmiCaches::SRAM_TYPE_OTHER:
            return "Other";

        case proto::DmiCaches::SRAM_TYPE_UNKNOWN:
            return "Unknown";

        case proto::DmiCaches::SRAM_TYPE_NON_BURST:
            return "Non-burst";

        case proto::DmiCaches::SRAM_TYPE_BURST:
            return "Burst";

        case proto::DmiCaches::SRAM_TYPE_PIPELINE_BURST:
            return "Pipeline Burst";

        case proto::DmiCaches::SRAM_TYPE_SYNCHRONOUS:
            return "Synchronous";

        case proto::DmiCaches::SRAM_TYPE_ASYNCHRONOUS:
            return "Asynchronous";

        default:
            return "Unknown";
    }
}

const char* ErrorCorrectionTypeToString(proto::DmiCaches::ErrorCorrectionType value)
{
    switch (value)
    {
        case proto::DmiCaches::ERROR_CORRECTION_TYPE_OTHER:
            return "Other";

        case proto::DmiCaches::ERROR_CORRECTION_TYPE_NONE:
            return "None";

        case proto::DmiCaches::ERROR_CORRECTION_TYPE_PARITY:
            return "Parity";

        case proto::DmiCaches::ERROR_CORRECTION_TYPE_SINGLE_BIT_ECC:
            return "Single bit ECC";

        case proto::DmiCaches::ERROR_CORRECTION_TYPE_MULTI_BIT_ECC:
            return "Multi bit ECC";

        default:
            return "Unknown";
    }
}

const char* TypeToString(proto::DmiCaches::Type value)
{
    switch (value)
    {
        case proto::DmiCaches::TYPE_OTHER:
            return "Other";

        case proto::DmiCaches::TYPE_INSTRUCTION:
            return "Instruction";

        case proto::DmiCaches::TYPE_DATA:
            return "Data";

        case proto::DmiCaches::TYPE_UNIFIED:
            return "Unified";

        default:
            return "Unknown";
    }
}

const char* AssociativityToString(proto::DmiCaches::Associativity value)
{
    switch (value)
    {
        case proto::DmiCaches::ASSOCIATIVITY_OTHER:
            return "Other";

        case proto::DmiCaches::ASSOCIATIVITY_DIRECT_MAPPED:
            return "Direct Mapped";

        case proto::DmiCaches::ASSOCIATIVITY_2_WAY:
            return "2-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_4_WAY:
            return "4-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_FULLY:
            return "Fully Associative";

        case proto::DmiCaches::ASSOCIATIVITY_8_WAY:
            return "8-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_16_WAY:
            return "16-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_12_WAY:
            return "12-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_24_WAY:
            return "24-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_32_WAY:
            return "32-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_48_WAY:
            return "48-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_64_WAY:
            return "64-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_20_WAY:
            return "20-way Set-Associative";

        default:
            return "Unknown";
    }
}

proto::DmiCaches::Location GetLocation(uint16_t value)
{
    switch (BitSet<uint16_t>(value).Range(5, 6))
    {
        case 0x00: return proto::DmiCaches::LOCATION_INTERNAL;
        case 0x01: return proto::DmiCaches::LOCATION_EXTERNAL;
        case 0x02: return proto::DmiCaches::LOCATION_RESERVED;
        default: return proto::DmiCaches::LOCATION_UNKNOWN;
    }
}

proto::DmiCaches::Status GetStatus(uint16_t value)
{
    if (BitSet<uint16_t>(value & 0x0080).Test(7))
        return proto::DmiCaches::STATUS_ENABLED;

    return proto::DmiCaches::STATUS_DISABLED;
}

proto::DmiCaches::Mode GetMode(uint16_t value)
{
    switch (BitSet<uint16_t>(value).Range(8, 9))
    {
        case 0x00: return proto::DmiCaches::MODE_WRITE_THRU;
        case 0x01: return proto::DmiCaches::MODE_WRITE_BACK;
        case 0x02: return proto::DmiCaches::MODE_WRITE_WITH_MEMORY_ADDRESS;
        default: return proto::DmiCaches::MODE_UNKNOWN;
    }
}

int GetLevel(uint16_t value)
{
    return BitSet<uint16_t>(value).Range(0, 2) + 1;
}

uint32_t GetSupportedSRAMTypes(uint16_t value)
{
    BitSet<uint16_t> bitfield(value);

    if (bitfield.None())
        return proto::DmiCaches::SRAM_TYPE_BAD;

    uint32_t types = 0;

    if (bitfield.Test(0))
        types |= proto::DmiCaches::SRAM_TYPE_OTHER;

    if (bitfield.Test(1))
        types |= proto::DmiCaches::SRAM_TYPE_UNKNOWN;

    if (bitfield.Test(2))
        types |= proto::DmiCaches::SRAM_TYPE_NON_BURST;

    if (bitfield.Test(3))
        types |= proto::DmiCaches::SRAM_TYPE_BURST;

    if (bitfield.Test(4))
        types |= proto::DmiCaches::SRAM_TYPE_PIPELINE_BURST;

    if (bitfield.Test(5))
        types |= proto::DmiCaches::SRAM_TYPE_SYNCHRONOUS;

    if (bitfield.Test(6))
        types |= proto::DmiCaches::SRAM_TYPE_ASYNCHRONOUS;

    return types;
}

proto::DmiCaches::SRAMType GetCurrentSRAMType(uint16_t value)
{
    BitSet<uint16_t> type(value);

    if (type.Test(0))
        return proto::DmiCaches::SRAM_TYPE_OTHER;

    if (type.Test(1))
        return proto::DmiCaches::SRAM_TYPE_UNKNOWN;

    if (type.Test(2))
        return proto::DmiCaches::SRAM_TYPE_NON_BURST;

    if (type.Test(3))
        return proto::DmiCaches::SRAM_TYPE_BURST;

    if (type.Test(4))
        return proto::DmiCaches::SRAM_TYPE_PIPELINE_BURST;

    if (type.Test(5))
        return proto::DmiCaches::SRAM_TYPE_SYNCHRONOUS;

    if (type.Test(6))
        return proto::DmiCaches::SRAM_TYPE_ASYNCHRONOUS;

    return proto::DmiCaches::SRAM_TYPE_BAD;
}

proto::DmiCaches::ErrorCorrectionType GetErrorCorrectionType(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiCaches::ERROR_CORRECTION_TYPE_OTHER;
        case 0x03: return proto::DmiCaches::ERROR_CORRECTION_TYPE_NONE;
        case 0x04: return proto::DmiCaches::ERROR_CORRECTION_TYPE_PARITY;
        case 0x05: return proto::DmiCaches::ERROR_CORRECTION_TYPE_SINGLE_BIT_ECC;
        case 0x06: return proto::DmiCaches::ERROR_CORRECTION_TYPE_MULTI_BIT_ECC;
        default: return proto::DmiCaches::ERROR_CORRECTION_TYPE_UNKNOWN;
    }
}

proto::DmiCaches::Type GetType(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiCaches::TYPE_OTHER;
        case 0x03: return proto::DmiCaches::TYPE_INSTRUCTION;
        case 0x04: return proto::DmiCaches::TYPE_DATA;
        case 0x05: return proto::DmiCaches::TYPE_UNIFIED;
        default: return proto::DmiCaches::TYPE_UNKNOWN;
    }
}

proto::DmiCaches::Associativity GetAssociativity(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiCaches::ASSOCIATIVITY_OTHER;
        case 0x03: return proto::DmiCaches::ASSOCIATIVITY_DIRECT_MAPPED;
        case 0x04: return proto::DmiCaches::ASSOCIATIVITY_2_WAY;
        case 0x05: return proto::DmiCaches::ASSOCIATIVITY_4_WAY;
        case 0x06: return proto::DmiCaches::ASSOCIATIVITY_FULLY;
        case 0x07: return proto::DmiCaches::ASSOCIATIVITY_8_WAY;
        case 0x08: return proto::DmiCaches::ASSOCIATIVITY_16_WAY;
        case 0x09: return proto::DmiCaches::ASSOCIATIVITY_12_WAY;
        case 0x0A: return proto::DmiCaches::ASSOCIATIVITY_24_WAY;
        case 0x0B: return proto::DmiCaches::ASSOCIATIVITY_32_WAY;
        case 0x0C: return proto::DmiCaches::ASSOCIATIVITY_48_WAY;
        case 0x0D: return proto::DmiCaches::ASSOCIATIVITY_64_WAY;
        case 0x0E: return proto::DmiCaches::ASSOCIATIVITY_20_WAY;
        default: return proto::DmiCaches::ASSOCIATIVITY_UNKNOWN;
    }
}

} // namespace

CategoryDmiCache::CategoryDmiCache()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryDmiCache::Name() const
{
    return "Caches";
}

Category::IconId CategoryDmiCache::Icon() const
{
    return IDI_CHIP;
}

const char* CategoryDmiCache::Guid() const
{
    return "{BA9258E7-0046-4A77-A97B-0407453706A3}";
}

void CategoryDmiCache::Parse(Table& table, const std::string& data)
{
    proto::DmiCaches message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiCaches::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Cache #%d", index + 1));

        if (!item.name().empty())
            group.AddParam("Name", Value::String(item.name()));

        group.AddParam("Location", Value::String(LocationToString(item.location())));
        group.AddParam("Status", Value::String(StatusToString(item.status())));
        group.AddParam("Mode", Value::String(ModeToString(item.mode())));

        if (item.level() != 0)
            group.AddParam("Level", Value::String(StringPrintf("L%d", item.level())));

        if (item.maximum_size() != 0)
            group.AddParam("Maximum Size", Value::Number(item.maximum_size(), "kB"));

        if (item.current_size() != 0)
            group.AddParam("Current Size", Value::Number(item.current_size(), "kB"));

        if (item.supported_sram_types())
        {
            Group types_group = group.AddGroup("Supported SRAM Types");

            types_group.AddParam("Non-burst",
                                 Value::Bool(item.supported_sram_types() &
                                             proto::DmiCaches::SRAM_TYPE_NON_BURST));

            types_group.AddParam("Burst",
                                 Value::Bool(item.supported_sram_types() &
                                             proto::DmiCaches::SRAM_TYPE_BURST));

            types_group.AddParam("Pipeline Burst",
                                 Value::Bool(item.supported_sram_types() &
                                             proto::DmiCaches::SRAM_TYPE_PIPELINE_BURST));

            types_group.AddParam("Synchronous",
                                 Value::Bool(item.supported_sram_types() &
                                             proto::DmiCaches::SRAM_TYPE_SYNCHRONOUS));

            types_group.AddParam("Asynchronous",
                                 Value::Bool(item.supported_sram_types() &
                                             proto::DmiCaches::SRAM_TYPE_ASYNCHRONOUS));
        }

        group.AddParam("Current SRAM Type", Value::String(SRAMTypeToString(item.current_sram_type())));

        if (item.speed() != 0)
            group.AddParam("Speed", Value::Number(item.speed(), "ns"));

        group.AddParam("Error Correction Type", Value::String(ErrorCorrectionTypeToString(item.error_correction_type())));
        group.AddParam("Type", Value::String(TypeToString(item.type())));
        group.AddParam("Associativity", Value::String(AssociativityToString(item.associativity())));
    }
}

std::string CategoryDmiCache::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiCaches message;

    for (SMBios::TableEnumerator table_enumerator(*smbios, SMBios::TABLE_TYPE_CACHE);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::Table table = table_enumerator.GetTable();
        proto::DmiCaches::Item* item = message.add_item();

        item->set_name(table.String(0x04));

        item->set_location(GetLocation(table.Word(0x05)));
        item->set_status(GetStatus(table.Word(0x05)));
        item->set_mode(GetMode(table.Word(0x05)));
        item->set_level(GetLevel(table.Word(0x05)));

        item->set_maximum_size(BitSet<uint16_t>(table.Word(0x07)).Range(0, 14));
        item->set_current_size(BitSet<uint16_t>(table.Word(0x09)).Range(0, 14));
        item->set_supported_sram_types(GetSupportedSRAMTypes(table.Word(0x0B)));
        item->set_current_sram_type(GetCurrentSRAMType(table.Word(0x0D)));

        if (table.Length() >= 0x13)
        {
            item->set_speed(table.Byte(0x0F));
            item->set_error_correction_type(GetErrorCorrectionType(table.Byte(0x10)));
            item->set_type(GetType(table.Byte(0x11)));
            item->set_associativity(GetAssociativity(table.Byte(0x12)));
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
