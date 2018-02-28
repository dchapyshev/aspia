//
// PROJECT:         Aspia
// FILE:            category/category_dmi_memory_device.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "category/category_dmi_memory_device.h"
#include "category/category_dmi_memory_device.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* TypeToString(proto::DmiMemoryDevice::Type value)
{
    switch (value)
    {
        case proto::DmiMemoryDevice::TYPE_OTHER:
            return "Other";

        case proto::DmiMemoryDevice::TYPE_DRAM:
            return "DRAM";

        case proto::DmiMemoryDevice::TYPE_EDRAM:
            return "EDRAM";

        case proto::DmiMemoryDevice::TYPE_VRAM:
            return "VRAM";

        case proto::DmiMemoryDevice::TYPE_SRAM:
            return "SRAM";

        case proto::DmiMemoryDevice::TYPE_RAM:
            return "RAM";

        case proto::DmiMemoryDevice::TYPE_ROM:
            return "ROM";

        case proto::DmiMemoryDevice::TYPE_FLASH:
            return "Flash";

        case proto::DmiMemoryDevice::TYPE_EEPROM:
            return "EEPROM";

        case proto::DmiMemoryDevice::TYPE_FEPROM:
            return "FEPROM";

        case proto::DmiMemoryDevice::TYPE_EPROM:
            return "EPROM";

        case proto::DmiMemoryDevice::TYPE_CDRAM:
            return "CDRAM";

        case proto::DmiMemoryDevice::TYPE_3DRAM:
            return "3DRAM";

        case proto::DmiMemoryDevice::TYPE_SDRAM:
            return "SDRAM";

        case proto::DmiMemoryDevice::TYPE_SGRAM:
            return "SGRAM";

        case proto::DmiMemoryDevice::TYPE_RDRAM:
            return "RDRAM";

        case proto::DmiMemoryDevice::TYPE_DDR:
            return "DDR";

        case proto::DmiMemoryDevice::TYPE_DDR2:
            return "DDR2";

        case proto::DmiMemoryDevice::TYPE_DDR2_FB_DIMM:
            return "DDR2 FB-DIMM";

        case proto::DmiMemoryDevice::TYPE_DDR3:
            return "DDR3";

        case proto::DmiMemoryDevice::TYPE_FBD2:
            return "FBD2";

        case proto::DmiMemoryDevice::TYPE_DDR4:
            return "DDR4";

        case proto::DmiMemoryDevice::TYPE_LPDDR:
            return "LPDDR";

        case proto::DmiMemoryDevice::TYPE_LPDDR2:
            return "LPDDR2";

        case proto::DmiMemoryDevice::TYPE_LPDDR3:
            return "LPDDR3";

        case proto::DmiMemoryDevice::TYPE_LPDDR4:
            return "LPDDR4";

        default:
            return "Unknown";
    }
}

const char* FormFactorToString(proto::DmiMemoryDevice::FormFactor value)
{
    switch (value)
    {
        case proto::DmiMemoryDevice::FORM_FACTOR_OTHER:
            return "Other";

        case proto::DmiMemoryDevice::FORM_FACTOR_SIMM:
            return "SIMM";

        case proto::DmiMemoryDevice::FORM_FACTOR_SIP:
            return "SIP";

        case proto::DmiMemoryDevice::FORM_FACTOR_CHIP:
            return "Chip";

        case proto::DmiMemoryDevice::FORM_FACTOR_DIP:
            return "DIP";

        case proto::DmiMemoryDevice::FORM_FACTOR_ZIP:
            return "ZIP";

        case proto::DmiMemoryDevice::FORM_FACTOR_PROPRIETARY_CARD:
            return "Proprietary Card";

        case proto::DmiMemoryDevice::FORM_FACTOR_DIMM:
            return "DIMM";

        case proto::DmiMemoryDevice::FORM_FACTOR_TSOP:
            return "TSOP";

        case proto::DmiMemoryDevice::FORM_FACTOR_ROW_OF_CHIPS:
            return "Row Of Chips";

        case proto::DmiMemoryDevice::FORM_FACTOR_RIMM:
            return "RIMM";

        case proto::DmiMemoryDevice::FORM_FACTOR_SODIMM:
            return "SODIMM";

        case proto::DmiMemoryDevice::FORM_FACTOR_SRIMM:
            return "SRIMM";

        case proto::DmiMemoryDevice::FORM_FACTOR_FB_DIMM:
            return "FB-DIMM";

        default:
            return "Unknown";
    }
}

proto::DmiMemoryDevice::Type GetType(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiMemoryDevice::TYPE_OTHER;
        case 0x02: return proto::DmiMemoryDevice::TYPE_UNKNOWN;
        case 0x03: return proto::DmiMemoryDevice::TYPE_DRAM;
        case 0x04: return proto::DmiMemoryDevice::TYPE_EDRAM;
        case 0x05: return proto::DmiMemoryDevice::TYPE_VRAM;
        case 0x06: return proto::DmiMemoryDevice::TYPE_SRAM;
        case 0x07: return proto::DmiMemoryDevice::TYPE_RAM;
        case 0x08: return proto::DmiMemoryDevice::TYPE_ROM;
        case 0x09: return proto::DmiMemoryDevice::TYPE_FLASH;
        case 0x0A: return proto::DmiMemoryDevice::TYPE_EEPROM;
        case 0x0B: return proto::DmiMemoryDevice::TYPE_FEPROM;
        case 0x0C: return proto::DmiMemoryDevice::TYPE_EPROM;
        case 0x0D: return proto::DmiMemoryDevice::TYPE_CDRAM;
        case 0x0E: return proto::DmiMemoryDevice::TYPE_3DRAM;
        case 0x0F: return proto::DmiMemoryDevice::TYPE_SDRAM;
        case 0x10: return proto::DmiMemoryDevice::TYPE_SGRAM;
        case 0x11: return proto::DmiMemoryDevice::TYPE_RDRAM;
        case 0x12: return proto::DmiMemoryDevice::TYPE_DDR;
        case 0x13: return proto::DmiMemoryDevice::TYPE_DDR2;
        case 0x14: return proto::DmiMemoryDevice::TYPE_DDR2_FB_DIMM;

        case 0x18: return proto::DmiMemoryDevice::TYPE_DDR3;
        case 0x19: return proto::DmiMemoryDevice::TYPE_FBD2;
        case 0x1A: return proto::DmiMemoryDevice::TYPE_DDR4;
        case 0x1B: return proto::DmiMemoryDevice::TYPE_LPDDR;
        case 0x1C: return proto::DmiMemoryDevice::TYPE_LPDDR2;
        case 0x1D: return proto::DmiMemoryDevice::TYPE_LPDDR3;
        case 0x1E: return proto::DmiMemoryDevice::TYPE_LPDDR4;

        default: return proto::DmiMemoryDevice::TYPE_UNKNOWN;
    }
}

proto::DmiMemoryDevice::FormFactor GetFormFactor(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiMemoryDevice::FORM_FACTOR_OTHER;
        case 0x02: return proto::DmiMemoryDevice::FORM_FACTOR_UNKNOWN;
        case 0x03: return proto::DmiMemoryDevice::FORM_FACTOR_SIMM;
        case 0x04: return proto::DmiMemoryDevice::FORM_FACTOR_SIP;
        case 0x05: return proto::DmiMemoryDevice::FORM_FACTOR_CHIP;
        case 0x06: return proto::DmiMemoryDevice::FORM_FACTOR_DIP;
        case 0x07: return proto::DmiMemoryDevice::FORM_FACTOR_ZIP;
        case 0x08: return proto::DmiMemoryDevice::FORM_FACTOR_PROPRIETARY_CARD;
        case 0x09: return proto::DmiMemoryDevice::FORM_FACTOR_DIMM;
        case 0x0A: return proto::DmiMemoryDevice::FORM_FACTOR_TSOP;
        case 0x0B: return proto::DmiMemoryDevice::FORM_FACTOR_ROW_OF_CHIPS;
        case 0x0C: return proto::DmiMemoryDevice::FORM_FACTOR_RIMM;
        case 0x0D: return proto::DmiMemoryDevice::FORM_FACTOR_SODIMM;
        case 0x0E: return proto::DmiMemoryDevice::FORM_FACTOR_SRIMM;
        case 0x0F: return proto::DmiMemoryDevice::FORM_FACTOR_FB_DIMM;
        default: return proto::DmiMemoryDevice::FORM_FACTOR_UNKNOWN;
    }
}

uint64_t GetSize(const SMBios::Table& table)
{
    uint16_t size = table.Word(0x0C);

    if (table.Length() >= 0x20 && size == 0x7FFF)
    {
        uint32_t ext_size = table.Dword(0x1C) & 0x7FFFFFFFUL;

        if (ext_size & 0x3FFUL)
        {
            // Size in MB. Convert to bytes and return.
            return static_cast<uint64_t>(ext_size) * 1024ULL * 1024ULL;
        }
        else if (ext_size & 0xFFC00UL)
        {
            // Size in GB. Convert to bytes and return.
            return static_cast<uint64_t>(ext_size >> 10) * 1024ULL * 1024ULL * 1024ULL;
        }
        else
        {
            // Size in TB. Convert to bytes and return.
            return static_cast<uint64_t>(ext_size >> 20) * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
        }
    }
    else
    {
        if (size == 0xFFFF)
        {
            // No installed memory device in the socket.
            return 0;
        }

        if (size & 0x8000)
        {
            // Size in kB. Convert to bytes and return.
            return static_cast<uint64_t>(size & 0x7FFF) * 1024ULL;
        }
        else
        {
            // Size in MB. Convert to bytes and return.
            return static_cast<uint64_t>(size) * 1024ULL * 1024ULL;
        }
    }
}

} // namespace

CategoryDmiMemoryDevice::CategoryDmiMemoryDevice()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryDmiMemoryDevice::Name() const
{
    return "Memory Devices";
}

Category::IconId CategoryDmiMemoryDevice::Icon() const
{
    return IDI_MEMORY;
}

const char* CategoryDmiMemoryDevice::Guid() const
{
    return "{9C591459-A83F-4F48-883D-927765C072B0}";
}

void CategoryDmiMemoryDevice::Parse(Table& table, const std::string& data)
{
    proto::DmiMemoryDevice message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiMemoryDevice::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Memory Device #%d", index + 1));

        if (!item.device_locator().empty())
            group.AddParam("Device Locator", Value::String(item.device_locator()));

        if (item.size() != 0)
            group.AddParam("Size", Value::MemorySize(item.size()));

        group.AddParam("Type", Value::String(TypeToString(item.type())));

        if (item.speed() != 0)
            group.AddParam("Speed", Value::Number(item.speed(), "MHz"));

        group.AddParam("Form Factor", Value::String(FormFactorToString(item.form_factor())));

        if (!item.serial_number().empty())
            group.AddParam("Serial Number", Value::String(item.serial_number()));

        if (!item.part_number().empty())
            group.AddParam("Part Number", Value::String(item.part_number()));

        if (!item.manufactorer().empty())
            group.AddParam("Manufacturer", Value::String(item.manufactorer()));

        if (!item.bank().empty())
            group.AddParam("Bank", Value::String(item.bank()));

        if (item.total_width() != 0)
            group.AddParam("Total Width", Value::Number(item.total_width(), "Bit"));

        if (item.data_width() != 0)
            group.AddParam("Data Width", Value::Number(item.data_width(), "Bit"));
    }
}

std::string CategoryDmiMemoryDevice::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiMemoryDevice message;

    for (SMBios::TableEnumerator table_enumerator(*smbios, SMBios::TABLE_TYPE_MEMORY_DEVICE);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::Table table = table_enumerator.GetTable();
        if (table.Length() < 0x15)
            continue;

        proto::DmiMemoryDevice::Item* item = message.add_item();

        item->set_total_width(table.Word(0x08));
        item->set_data_width(table.Word(0x0A));
        item->set_size(GetSize(table));

        item->set_form_factor(GetFormFactor(table.Byte(0x0E)));
        item->set_device_locator(table.String(0x10));
        item->set_bank(table.String(0x11));
        item->set_type(GetType(table.Byte(0x12)));

        if (table.Length() >= 0x17)
            item->set_speed(table.Word(0x15));

        if (table.Length() >= 0x1B)
        {
            item->set_serial_number(table.String(0x18));
            item->set_manufactorer(table.String(0x17));
            item->set_part_number(table.String(0x1A));
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
