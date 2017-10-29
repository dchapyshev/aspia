//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/smbios_parser.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_parser.h"
#include "base/strings/string_util.h"
#include "base/logging.h"

namespace aspia {

// static
std::unique_ptr<SMBiosParser> SMBiosParser::Create(std::unique_ptr<uint8_t[]> data,
                                                   size_t data_size)
{
    if (!data || !data_size)
        return nullptr;

    SMBiosData* smbios = reinterpret_cast<SMBiosData*>(data.get());

    int table_count = GetTableCount(smbios->smbios_table_data, smbios->length);
    if (!table_count)
    {
        LOG(WARNING) << "SMBios tables not found";
        return nullptr;
    }

    return std::unique_ptr<SMBiosParser>(
        new SMBiosParser(std::move(data), data_size, table_count));
}

SMBiosParser::SMBiosParser(std::unique_ptr<uint8_t[]> data, size_t data_size, int table_count)
    : data_(std::move(data)),
      data_size_(data_size),
      table_count_(table_count)
{
    DCHECK(data_ != nullptr);
    DCHECK(data_size_ != 0);
    DCHECK(table_count_ != 0);
}

// static
int SMBiosParser::GetTableCount(uint8_t* table_data, uint32_t length)
{
    int count = 0;

    uint8_t* offset = table_data;
    uint8_t* end = table_data + length;

    while (offset < end)
    {
        // Get the table header to read te length.
        Table* table = reinterpret_cast<Table*>(offset);

        // Increase the offset.
        offset += table->length;

        // Increses the offset to point to the next header that's after the strings at the end
        // of the structure.
        while (*reinterpret_cast<uint16_t*>(offset) != 0 && offset < end)
        {
            ++offset;
        }

        // Points to the next stucture thas after two null bytes at the end of the strings.
        offset += 2;

        // Increase tables count.
        ++count;
    }

    return count;
}

// static
std::string SMBiosParser::GetString(const Table* table, uint8_t offset)
{
    if (!offset)
        return std::string();

    char* string = reinterpret_cast<char*>(const_cast<Table*>(table)) + table->length;

    while (offset > 1 && *string)
    {
        string += strlen(string);
        ++string;
        --offset;
    }

    if (!*string)
        return std::string();

    return string;
}

SMBiosParser::Table* SMBiosParser::GetTable(TableType type)
{
    SMBiosData* smbios = reinterpret_cast<SMBiosData*>(data_.get());

    uint8_t* start = &smbios->smbios_table_data[0];
    uint8_t* end = start + smbios->length;
    uint8_t* pos = start;

    int table_index = 0;

    while (table_index < table_count_ && pos + 4 <= end)
    {
        Table* table = reinterpret_cast<Table*>(pos);

        if (table->length < 4)
        {
            // If a short entry is found(less than 4 bytes), not only it is invalid, but we
            // cannot reliably locate the next entry. Better stop at this point, and let the
            // user know his / her table is broken.
            LOG(WARNING) << "Invalid SMBIOS table length: " << table->length;
            break;
        }

        // Stop decoding at end of table marker.
        if (table->type == TABLE_TYPE_END_OF_TABLE)
            break;

        // Look for the next handle.
        uint8_t* next = pos + table->length;
        while (static_cast<size_t>(next - start + 1) < smbios->length &&
               (next[0] != 0 || next[1] != 0))
        {
            ++next;
        }

        // Points to the next stucture thas after two null bytes at the end of the strings.
        next += 2;

        if (static_cast<size_t>(next - start) <= smbios->length && table->type == type)
        {
            // The table of the specified type is found.
            return table;
        }

        pos = next;

        // Increase table index.
        ++table_index;
    }

    if (table_index + 1 != table_count_)
    {
        LOG(WARNING) << "The number of processed tables does not correspond "
                        "to the total number of tables: "
                     << (table_index + 1) << "/" << table_count_;
    }

    if (pos - start != smbios->length)
    {
        LOG(WARNING) << "The announced size does not match the processed: "
                     << static_cast<size_t>(pos - start) << "/" << smbios->length;
    }

    // Table not found.
    return nullptr;
}

//
// TableParser
//

SMBiosParser::TableParser::TableParser(const Table* table)
    : data_(reinterpret_cast<const uint8_t*>(table))
{
    // Nothing
}

uint8_t SMBiosParser::TableParser::GetByte(uint8_t offset) const
{
    return data_[offset];
}

uint16_t SMBiosParser::TableParser::GetWord(uint8_t offset) const
{
    return *reinterpret_cast<const uint16_t*>(&data_[offset]);
}

uint32_t SMBiosParser::TableParser::GetDword(uint8_t offset) const
{
    return *reinterpret_cast<const uint32_t*>(&data_[offset]);
}

uint64_t SMBiosParser::TableParser::GetQword(uint8_t offset) const
{
    return *reinterpret_cast<const uint64_t*>(&data_[offset]);
}

std::string SMBiosParser::TableParser::GetString(uint8_t offset) const
{
    DCHECK(offset < 0x04);

    uint8_t handle = GetByte(offset);
    if (!handle)
        return std::string();

    char* string = reinterpret_cast<char*>(const_cast<uint8_t*>(data_)) + data_[1];

    while (handle > 1 && *string)
    {
        string += strlen(string);
        ++string;
        --handle;
    }

    if (!*string || !string[0])
        return std::string();

    return string;
}

//
// BiosTableParser
//

SMBiosParser::BiosTableParser::BiosTableParser(Table* table)
    : TableParser(table)
{
    DCHECK(table->type == TABLE_TYPE_BIOS);
}

std::string SMBiosParser::BiosTableParser::GetManufacturer() const
{
    return GetString(0x04);
}

std::string SMBiosParser::BiosTableParser::GetVersion() const
{
    return GetString(0x05);
}

std::string SMBiosParser::BiosTableParser::GetDate() const
{
    return GetString(0x08);
}

int SMBiosParser::BiosTableParser::GetSize() const
{
    return (GetByte(0x09) + 1) << 6;
}

std::string SMBiosParser::BiosTableParser::GetBiosRevision() const
{
    uint8_t major = GetByte(0x14);
    uint8_t minor = GetByte(0x15);

    if (major != 0xFF && minor != 0xFF)
        return StringPrintf("%u.%u", major, minor);

    return std::string();
}

std::string SMBiosParser::BiosTableParser::GetFirmwareRevision() const
{
    uint8_t major = GetByte(0x16);
    uint8_t minor = GetByte(0x17);

    if (major != 0xFF && minor != 0xFF)
        return StringPrintf("%u.%u", major, minor);

    return std::string();
}

std::string SMBiosParser::BiosTableParser::GetAddress() const
{
    uint16_t address = GetWord(0x06);

    if (address != 0)
        return StringPrintf("%04X0h", address);

    return std::string();
}

int SMBiosParser::BiosTableParser::GetRuntimeSize() const
{
    uint16_t address = GetWord(0x06);
    if (address == 0)
        return 0;

    uint32_t code = (0x10000 - address) << 4;

    if (code & 0x000003FF)
        return code;

    return (code >> 10) * 1024;
}

std::vector<SMBiosParser::BiosTableParser::Characteristic>
    SMBiosParser::BiosTableParser::GetCharacteristics() const
{
    std::vector<Characteristic> table;

    static const char* characteristics_names[] =
    {
        "BIOS characteristics not supported", // 3
        "ISA", // 4
        "MCA",
        "EISA",
        "PCI",
        "PC Card (PCMCIA)",
        "PNP",
        "APM",
        "BIOS is upgradeable",
        "BIOS shadowing",
        "VLB",
        "ESCD",
        "Boot from CD",
        "Selectable boot",
        "BIOS ROM is socketed",
        "Boot from PC Card (PCMCIA)",
        "EDD",
        "Japanese floppy for NEC 9800 1.2 MB (int 13h)",
        "Japanese floppy for Toshiba 1.2 MB (int 13h)",
        "5.25\"/360 kB floppy (int 13h)",
        "5.25\"/1.2 MB floppy (int 13h)",
        "3.5\"/720 kB floppy (int 13h)",
        "3.5\"/2.88 MB floppy (int 13h)",
        "Print screen (int 5h)",
        "8042 keyboard (int 9h)",
        "Serial (int 14h)",
        "Printer (int 17h)",
        "CGA/mono video (int 10h)",
        "NEC PC-98" // 31
    };

    uint64_t characteristics = GetQword(0x0A);
    if (!(characteristics & (1 << 3)))
    {
        for (int i = 4; i <= 31; ++i)
        {
            bool is_supported = (characteristics & (1ULL << i)) ? true : false;
            table.emplace_back(characteristics_names[i - 3], is_supported);
        }
    }

    uint8_t table_length = GetByte(0x02);

    if (table_length >= 0x13)
    {
        uint8_t characteristics1 = GetByte(0x12);

        static const char* characteristics1_names[] =
        {
            "ACPI", // 0
            "USB legacy",
            "AGP",
            "I2O boot",
            "LS-120 boot",
            "ATAPI Zip drive boot",
            "IEEE 1394 boot",
            "Smart battery" // 7
        };

        for (int i = 0; i <= 7; ++i)
        {
            bool is_supported = (characteristics1 & (1 << i)) ? true : false;
            table.emplace_back(characteristics1_names[i], is_supported);
        }
    }

    if (table_length >= 0x14)
    {
        uint8_t characteristics2 = GetByte(0x13);

        static const char* characteristics2_names[] =
        {
            "BIOS boot specification", // 0
            "Function key-initiated network boot",
            "Targeted content distribution" // 2
        };

        for (int i = 0; i <= 2; ++i)
        {
            bool is_supported = (characteristics2 & (1 << i)) ? true : false;
            table.emplace_back(characteristics2_names[i], is_supported);
        }
    }

    return table;
}

} // namespace aspia
