//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/smbios.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios.h"
#include "base/strings/string_util.h"
#include "base/logging.h"

namespace aspia {

// static
std::unique_ptr<SMBios> SMBios::Create(std::unique_ptr<uint8_t[]> data, size_t data_size)
{
    if (!data || !data_size)
        return nullptr;

    Data* smbios = reinterpret_cast<Data*>(data.get());

    if (!GetTableCount(smbios->smbios_table_data, smbios->length))
    {
        LOG(WARNING) << "SMBios tables not found";
        return nullptr;
    }

    return std::unique_ptr<SMBios>(new SMBios(std::move(data), data_size));
}

SMBios::SMBios(std::unique_ptr<uint8_t[]> data, size_t data_size)
    : data_(std::move(data)),
      data_size_(data_size)
{
    DCHECK(data_ != nullptr);
    DCHECK(data_size_ != 0);
}

uint8_t SMBios::GetMajorVersion() const
{
    Data* smbios = reinterpret_cast<Data*>(data_.get());
    return smbios->smbios_major_version;
}

uint8_t SMBios::GetMinorVersion() const
{
    Data* smbios = reinterpret_cast<Data*>(data_.get());
    return smbios->smbios_minor_version;
}

// static
int SMBios::GetTableCount(const uint8_t* table_data, uint32_t length)
{
    int count = 0;

    const uint8_t* pos = table_data;
    const uint8_t* end = table_data + length;

    while (pos < end)
    {
        // Increase the position by the length of the structure.
        pos += pos[1];

        // Increses the offset to point to the next header that's after the strings at the end
        // of the structure.
        while (*reinterpret_cast<const uint16_t*>(pos) != 0 && pos < end)
            ++pos;

        // Points to the next stucture thas after two null bytes at the end of the strings.
        pos += 2;

        ++count;
    }

    return count;
}

//
// TableEnumeratorImpl
//

SMBios::TableEnumeratorImpl::TableEnumeratorImpl(const Data* data, uint8_t type)
    : data_(data)
{
    start_ = &data_->smbios_table_data[0];
    end_ = start_ + data_->length;
    current_ = start_;
    next_ = start_;

    Advance(type);
}

bool SMBios::TableEnumeratorImpl::IsAtEnd() const
{
    return current_ == nullptr;
}

void SMBios::TableEnumeratorImpl::Advance(uint8_t type)
{
    current_ = next_;
    DCHECK(current_);

    while (current_ + 4 <= end_)
    {
        const uint8_t table_type = current_[0];
        const uint8_t table_length = current_[1];

        if (table_length < 4)
        {
            // If a short entry is found(less than 4 bytes), not only it is invalid, but we
            // cannot reliably locate the next entry. Better stop at this point, and let the
            // user know his / her table is broken.
            LOG(WARNING) << "Invalid SMBIOS table length: " << table_length;
            break;
        }

        // Stop decoding at end of table marker.
        if (table_type == 127)
            break;

        // Look for the next handle.
        next_ = current_ + table_length;
        while (static_cast<uintptr_t>(next_ - start_ + 1) < data_->length &&
            (next_[0] != 0 || next_[1] != 0))
        {
            ++next_;
        }

        // Points to the next table thas after two null bytes at the end of the strings.
        next_ += 2;

        // The table of the specified type is found.
        if (table_type == type)
            return;

        current_ = next_;
    }

    current_ = nullptr;
    next_ = nullptr;
}

const SMBios::Data* SMBios::TableEnumeratorImpl::GetSMBiosData() const
{
    return data_;
}

const uint8_t* SMBios::TableEnumeratorImpl::GetTableData() const
{
    return current_;
}

//
// SMBiosTable
//

SMBios::TableReader::TableReader(const TableReader& other)
    : smbios_(other.smbios_),
      table_(other.table_)
{
    // Nothing
}

SMBios::TableReader::TableReader(const Data* smbios, const uint8_t* table)
    : smbios_(smbios),
      table_(table)
{
    DCHECK(smbios_);
    DCHECK(table_);
}

uint8_t SMBios::TableReader::GetByte(uint8_t offset) const
{
    return table_[offset];
}

uint16_t SMBios::TableReader::GetWord(uint8_t offset) const
{
    return *reinterpret_cast<const uint16_t*>(GetPointer(offset));
}

uint32_t SMBios::TableReader::GetDword(uint8_t offset) const
{
    return *reinterpret_cast<const uint32_t*>(GetPointer(offset));
}

uint64_t SMBios::TableReader::GetQword(uint8_t offset) const
{
    return *reinterpret_cast<const uint64_t*>(GetPointer(offset));
}

std::string SMBios::TableReader::GetString(uint8_t offset) const
{
    uint8_t handle = GetByte(offset);
    if (!handle)
        return std::string();

    char* string = reinterpret_cast<char*>(
        const_cast<uint8_t*>(table_)) + table_[1];

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

const uint8_t* SMBios::TableReader::GetPointer(uint8_t offset) const
{
    return &table_[offset];
}

uint8_t SMBios::TableReader::GetTableLength() const
{
    return GetByte(0x01);
}

//
// BiosTable
//

SMBios::BiosTable::BiosTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::BiosTable::GetManufacturer() const
{
    return reader_.GetString(0x04);
}

std::string SMBios::BiosTable::GetVersion() const
{
    return reader_.GetString(0x05);
}

std::string SMBios::BiosTable::GetDate() const
{
    return reader_.GetString(0x08);
}

int SMBios::BiosTable::GetSize() const
{
    return (reader_.GetByte(0x09) + 1) << 6;
}

std::string SMBios::BiosTable::GetBiosRevision() const
{
    uint8_t major = reader_.GetByte(0x14);
    uint8_t minor = reader_.GetByte(0x15);

    if (major != 0xFF && minor != 0xFF)
        return StringPrintf("%u.%u", major, minor);

    return std::string();
}

std::string SMBios::BiosTable::GetFirmwareRevision() const
{
    uint8_t major = reader_.GetByte(0x16);
    uint8_t minor = reader_.GetByte(0x17);

    if (major != 0xFF && minor != 0xFF)
        return StringPrintf("%u.%u", major, minor);

    return std::string();
}

std::string SMBios::BiosTable::GetAddress() const
{
    uint16_t address = reader_.GetWord(0x06);

    if (address != 0)
        return StringPrintf("%04X0h", address);

    return std::string();
}

int SMBios::BiosTable::GetRuntimeSize() const
{
    uint16_t address = reader_.GetWord(0x06);
    if (address == 0)
        return 0;

    uint32_t code = (0x10000 - address) << 4;

    if (code & 0x000003FF)
        return code;

    return (code >> 10) * 1024;
}

SMBios::BiosTable::FeatureList SMBios::BiosTable::GetCharacteristics() const
{
    FeatureList feature_list;

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

    uint64_t characteristics = reader_.GetQword(0x0A);
    if (!(characteristics & (1 << 3)))
    {
        for (int i = 4; i <= 31; ++i)
        {
            bool is_supported = (characteristics & (1ULL << i)) ? true : false;
            feature_list.emplace_back(characteristics_names[i - 3], is_supported);
        }
    }

    uint8_t table_length = reader_.GetTableLength();

    if (table_length >= 0x13)
    {
        uint8_t characteristics1 = reader_.GetByte(0x12);

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
            feature_list.emplace_back(characteristics1_names[i], is_supported);
        }
    }

    if (table_length >= 0x14)
    {
        uint8_t characteristics2 = reader_.GetByte(0x13);

        static const char* characteristics2_names[] =
        {
            "BIOS boot specification", // 0
            "Function key-initiated network boot",
            "Targeted content distribution" // 2
        };

        for (int i = 0; i <= 2; ++i)
        {
            bool is_supported = (characteristics2 & (1 << i)) ? true : false;
            feature_list.emplace_back(characteristics2_names[i], is_supported);
        }
    }

    return feature_list;
}

//
// SystemTable
//

SMBios::SystemTable::SystemTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::SystemTable::GetManufacturer() const
{
    return reader_.GetString(0x04);
}

std::string SMBios::SystemTable::GetProductName() const
{
    return reader_.GetString(0x05);
}

std::string SMBios::SystemTable::GetVersion() const
{
    return reader_.GetString(0x06);
}

std::string SMBios::SystemTable::GetSerialNumber() const
{
    return reader_.GetString(0x07);
}

std::string SMBios::SystemTable::GetUUID() const
{
    if (reader_.GetTableLength() < 0x19)
        return std::string();

    const uint8_t* ptr = reader_.GetPointer(0x08);

    bool only_0xFF = true;
    bool only_0x00 = true;

    for (int i = 0; i < 16 && (only_0x00 || only_0xFF); ++i)
    {
        if (ptr[i] != 0x00) only_0x00 = false;
        if (ptr[i] != 0xFF) only_0xFF = false;
    }

    if (only_0xFF || only_0x00)
        return std::string();

    if ((reader_.GetMajorVersion() << 8) + reader_.GetMinorVersion() >= 0x0206)
    {
        return StringPrintf("%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                     ptr[3], ptr[2], ptr[1], ptr[0], ptr[5], ptr[4], ptr[7], ptr[6],
                     ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13], ptr[14], ptr[15]);
    }

    return StringPrintf("%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                        ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7],
                        ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13], ptr[14], ptr[15]);
}

std::string SMBios::SystemTable::GetWakeupType() const
{
    if (reader_.GetTableLength() < 0x19)
        return std::string();

    switch (reader_.GetByte(0x18))
    {
        case 0x01:
            return "Other";

        case 0x02:
            return "Unknown";

        case 0x03:
            return "APM Timer";

        case 0x04:
            return "Modem Ring";

        case 0x05:
            return "LAN Remote";

        case 0x06:
            return "Power Switch";

        case 0x07:
            return "PCI PME#";

        case 0x08:
            return "AC Power Restored";

        default:
            return std::string();
    }
}

std::string SMBios::SystemTable::GetSKUNumber() const
{
    if (reader_.GetTableLength() < 0x1B)
        return std::string();

    return reader_.GetString(0x19);
}

std::string SMBios::SystemTable::GetFamily() const
{
    if (reader_.GetTableLength() < 0x1B)
        return std::string();

    return reader_.GetString(0x1A);
}

//
// BaseboardTable
//

SMBios::BaseboardTable::BaseboardTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::BaseboardTable::GetManufacturer() const
{
    return reader_.GetString(0x04);
}

std::string SMBios::BaseboardTable::GetProductName() const
{
    return reader_.GetString(0x05);
}

std::string SMBios::BaseboardTable::GetVersion() const
{
    return reader_.GetString(0x06);
}

std::string SMBios::BaseboardTable::GetSerialNumber() const
{
    return reader_.GetString(0x07);
}

std::string SMBios::BaseboardTable::GetAssetTag() const
{
    if (reader_.GetTableLength() < 0x09)
        return std::string();

    return reader_.GetString(0x08);
}

SMBios::BaseboardTable::FeatureList SMBios::BaseboardTable::GetFeatures() const
{
    if (reader_.GetTableLength() < 0x0A)
        return FeatureList();

    uint8_t features = reader_.GetByte(0x09);
    if ((features & 0x1F) == 0)
        return FeatureList();

    static const char* feature_names[] =
    {
        "Board is a hosting board", // 0
        "Board requires at least one daughter board",
        "Board is removable",
        "Board is replaceable",
        "Board is hot swappable" // 4
    };

    FeatureList list;

    for (int i = 0; i <= 4; ++i)
    {
        bool is_supported = (features & (1 << i)) ? true : false;
        list.emplace_back(feature_names[i], is_supported);
    }

    return list;
}

std::string SMBios::BaseboardTable::GetLocationInChassis() const
{
    if (reader_.GetTableLength() < 0x0E)
        return std::string();

    return reader_.GetString(0x0A);
}

SMBios::BaseboardTable::BoardType SMBios::BaseboardTable::GetBoardType() const
{
    if (reader_.GetTableLength() < 0x0E)
        return BoardType::UNKNOWN;

    switch (reader_.GetByte(0x0D))
    {
        case 0x02: return BoardType::OTHER;
        case 0x03: return BoardType::SERVER_BLADE;
        case 0x04: return BoardType::CONNECTIVITY_SWITCH;
        case 0x05: return BoardType::SYSTEM_MANAGEMENT_MODULE;
        case 0x06: return BoardType::PROCESSOR_MODULE;
        case 0x07: return BoardType::IO_MODULE;
        case 0x08: return BoardType::MEMORY_MODULE;
        case 0x09: return BoardType::DAUGHTER_BOARD;
        case 0x0A: return BoardType::MOTHERBOARD;
        case 0x0B: return BoardType::PROCESSOR_PLUS_MEMORY_MODULE;
        case 0x0C: return BoardType::PROCESSOR_PLUS_IO_MODULE;
        case 0x0D: return BoardType::INTERCONNECT_BOARD;
        default: return BoardType::UNKNOWN;
    }
}

SMBios::ChassisTable::ChassisTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::ChassisTable::GetManufacturer() const
{
    if (reader_.GetTableLength() < 0x09)
        return std::string();

    return reader_.GetString(0x04);
}

std::string SMBios::ChassisTable::GetVersion() const
{
    if (reader_.GetTableLength() < 0x09)
        return std::string();

    return reader_.GetString(0x06);
}

std::string SMBios::ChassisTable::GetSerialNumber() const
{
    if (reader_.GetTableLength() < 0x09)
        return std::string();

    return reader_.GetString(0x07);
}

std::string SMBios::ChassisTable::GetAssetTag() const
{
    if (reader_.GetTableLength() < 0x09)
        return std::string();

    return reader_.GetString(0x08);
}

SMBios::ChassisTable::Type SMBios::ChassisTable::GetType() const
{
    if (reader_.GetTableLength() < 0x09)
        return Type::UNKNOWN;

    switch (reader_.GetByte(0x05) & 0x7F)
    {
        case 0x03: return Type::DESKTOP;
        case 0x04: return Type::LOW_PROFILE_DESKTOP;
        case 0x05: return Type::PIZZA_BOX;
        case 0x06: return Type::MINI_TOWER;
        case 0x07: return Type::TOWER;
        case 0x08: return Type::PORTABLE;
        case 0x09: return Type::LAPTOP;
        case 0x0A: return Type::NOTEBOOK;
        case 0x0B: return Type::HAND_HELD;
        case 0x0C: return Type::DOCKING_STATION;
        case 0x0D: return Type::ALL_IN_ONE;
        case 0x0E: return Type::SUB_NOTEBOOK;
        case 0x0F: return Type::SPACE_SAVING;
        case 0x10: return Type::LUNCH_BOX;
        case 0x11: return Type::MAIN_SERVER_CHASSIS;
        case 0x12: return Type::EXPANSION_CHASSIS;
        case 0x13: return Type::SUB_CHASSIS;
        case 0x14: return Type::BUS_EXPANSION_CHASSIS;
        case 0x15: return Type::PERIPHERIAL_CHASSIS;
        case 0x16: return Type::RAID_CHASSIS;
        case 0x17: return Type::RACK_MOUNT_CHASSIS;
        case 0x18: return Type::SEALED_CASE_PC;
        case 0x19: return Type::MULTI_SYSTEM_CHASSIS;
        case 0x1A: return Type::COMPACT_PCI;
        case 0x1B: return Type::ADVANCED_TCA;
        case 0x1C: return Type::BLADE;
        case 0x1D: return Type::BLADE_ENCLOSURE;
        default: return Type::UNKNOWN;
    }
}

SMBios::ChassisTable::Status SMBios::ChassisTable::GetOSLoadStatus() const
{
    if (reader_.GetTableLength() < 0x0D)
        return Status::UNKNOWN;

    switch (reader_.GetByte(0x09))
    {
        case 0x01: return Status::OTHER;
        case 0x03: return Status::SAFE;
        case 0x04: return Status::WARNING;
        case 0x05: return Status::CRITICAL;
        case 0x06: return Status::NON_RECOVERABLE;
        default: return Status::UNKNOWN;
    }
}

SMBios::ChassisTable::Status SMBios::ChassisTable::GetPowerSourceStatus() const
{
    if (reader_.GetTableLength() < 0x0D)
        return Status::UNKNOWN;

    switch (reader_.GetByte(0x0A))
    {
        case 0x01: return Status::OTHER;
        case 0x03: return Status::SAFE;
        case 0x04: return Status::WARNING;
        case 0x05: return Status::CRITICAL;
        case 0x06: return Status::NON_RECOVERABLE;
        default: return Status::UNKNOWN;
    }
}

SMBios::ChassisTable::Status SMBios::ChassisTable::GetTemperatureStatus() const
{
    if (reader_.GetTableLength() < 0x0D)
        return Status::UNKNOWN;

    switch (reader_.GetByte(0x0B))
    {
        case 0x01: return Status::OTHER;
        case 0x03: return Status::SAFE;
        case 0x04: return Status::WARNING;
        case 0x05: return Status::CRITICAL;
        case 0x06: return Status::NON_RECOVERABLE;
        default: return Status::UNKNOWN;
    }
}

SMBios::ChassisTable::SecurityStatus SMBios::ChassisTable::GetSecurityStatus() const
{
    if (reader_.GetTableLength() < 0x0D)
        return SecurityStatus::UNKNOWN;

    switch (reader_.GetByte(0x0C))
    {
        case 0x01: return SecurityStatus::OTHER;
        case 0x03: return SecurityStatus::NONE;
        case 0x04: return SecurityStatus::EXTERNAL_INTERFACE_LOCKED_OUT;
        case 0x05: return SecurityStatus::EXTERNAL_INTERFACE_ENABLED;
        default: return SecurityStatus::UNKNOWN;
    }
}

int SMBios::ChassisTable::GetHeight() const
{
    if (reader_.GetTableLength() < 0x13)
        return 0;

    return reader_.GetByte(0x11);
}

int SMBios::ChassisTable::GetNumberOfPowerCords() const
{
    if (reader_.GetTableLength() < 0x13)
        return 0;

    return reader_.GetByte(0x12);
}

} // namespace aspia
