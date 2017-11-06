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
        const_cast<uint8_t*>(GetPointer(0))) + GetTableLength();

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

//
// ChassisTable
//

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

//
// ProcessorTable
//

SMBios::ProcessorTable::ProcessorTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::ProcessorTable::GetManufacturer() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    return reader_.GetString(0x07);
}

std::string SMBios::ProcessorTable::GetVersion() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    return reader_.GetString(0x10);
}

std::string SMBios::ProcessorTable::GetFamily() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    struct FamilyList
    {
        uint16_t family;
        const char* name;
    } const list[] =
    {
        { 0x01, "Other" },
        { 0x02, "Unknown" },
        { 0x03, "Intel 8086" },
        { 0x04, "Intel 80286" },
        { 0x05, "Intel386" },
        { 0x06, "Intel486" },
        { 0x07, "Intel 8087" },
        { 0x08, "Intel 80287" },
        { 0x09, "Intel 80387" },
        { 0x0A, "Intel 80487" },
        { 0x0B, "Intel Pentium" },
        { 0x0C, "Intel Pentium Pro" },
        { 0x0D, "Intel Pentium 2" },
        { 0x0E, "Pentium with MMX technology" },
        { 0x0F, "Intel Celeron" },
        { 0x10, "Intel Pentium 2 Xeon" },
        { 0x11, "Intel Pentium 3" },
        { 0x12, "M1 Family" },
        { 0x13, "M2 Family" },
        { 0x14, "Intel Celeron M" },
        { 0x15, "Intel Pentium 4 HT" },
        // 0x16 - 0x17 Available for assignment.
        { 0x18, "AMD Duron" },
        { 0x19, "AMD K5 Family" },
        { 0x1A, "AMD K6 Family" },
        { 0x1B, "AMD K6-2" },
        { 0x1C, "AMD K6-3" },
        { 0x1D, "AMD Athlon" },
        { 0x1E, "AMD 29000" },
        { 0x1F, "AMD K6-2+" },
        { 0x20, "Power PC" },
        { 0x21, "Power PC 601" },
        { 0x22, "Power PC 603" },
        { 0x23, "Power PC 603+" },
        { 0x24, "Power PC 604" },
        { 0x25, "Power PC 620" },
        { 0x26, "Power PC x704" },
        { 0x27, "Power PC 750" },
        { 0x28, "Intel Core Duo" },
        { 0x29, "Intel Core Duo Mobile" },
        { 0x2A, "Intel Core Solo Mobile" },
        { 0x2B, "Intel Atom" },
        // 0x2C - 0x2F Available for assignment.
        { 0x30, "Alpha" },
        { 0x31, "Alpha 21064" },
        { 0x32, "Alpha 21066" },
        { 0x33, "Alpha 21164" },
        { 0x34, "Alpha 21164PC" },
        { 0x35, "Alpha 21164a" },
        { 0x36, "Alpha 21264" },
        { 0x37, "Alpha 21364" },
        { 0x38, "AMD Turion 2 Ultra Dual-Core Mobile M" },
        { 0x39, "AMD Turion 2 Dual-Core Mobile M" },
        { 0x3A, "AMD Athlon 2 Dual-Core M" },
        { 0x3B, "AMD Opteron 6100 Series" },
        { 0x3C, "AMD Opteron 4100 Series" },
        { 0x3D, "AMD Opteron 6200 Series" },
        { 0x3E, "AMD Opteron 4200 Series" },
        // 0x3F Available for assignment.
        { 0x40, "MIPS" },
        { 0x41, "MIPS R4000" },
        { 0x42, "MIPS R4200" },
        { 0x43, "MIPS R4400" },
        { 0x44, "MIPS R4600" },
        { 0x45, "MIPS R10000" },
        { 0x46, "AMD C-Series" },
        { 0x47, "AMD E-Series" },
        { 0x48, "AMD S-Series" },
        { 0x49, "AMD G-Series" },
        // 0x4A - 0x4F Available for assignment.
        { 0x50, "SPARC" },
        { 0x51, "SuperSPARC" },
        { 0x52, "microSPARC 2" },
        { 0x53, "microSPARC 2ep" },
        { 0x54, "UltraSPARC" },
        { 0x55, "UltraSPARC 2" },
        { 0x56, "UltraSPARC 2i" },
        { 0x57, "UltraSPARC 3" },
        { 0x58, "UltraSPARC 3i" },
        // 0x59 - 0x5F Available for assignment.
        { 0x60, "68040" },
        { 0x61, "68xxx" },
        { 0x62, "68000" },
        { 0x63, "68010" },
        { 0x64, "68020" },
        { 0x65, "68030" },
        // 0x66 - 0x6F Available for assignment.
        { 0x70, "Hobbit" },
        // 0x71 - 0x77 Available for assignment.
        { 0x78, "Crusoe TM5000" },
        { 0x79, "Crusoe TM3000" },
        { 0x7A, "Efficeon TM8000" },
        // 0x7B - 0x7F Available for assignment.
        { 0x80, "Weitek" },
        // 0x81 Available for assignment.
        { 0x82, "Itanium" },
        { 0x83, "AMD Athlon 64" },
        { 0x84, "AMD Opteron" },
        { 0x85, "AMD Sempron" },
        { 0x86, "AMD Turion 64 Mobile" },
        { 0x87, "AMD Opteron Dual-Core" },
        { 0x88, "AMD Athlon 64 X2 Dual-Core" },
        { 0x89, "AMD Turion 64 X2 Mobile" },
        { 0x8A, "AMD Opteron Quad-Core" },
        { 0x8B, "Third-Generation AMD Opteron" },
        { 0x8C, "AMD Phenom FX Quad-Core" },
        { 0x8D, "AMD Phenom X4 Quad-Core" },
        { 0x8E, "AMD Phenom X2 Dual-Core" },
        { 0x8F, "AMD Athlon X2 Dual-Core" },
        { 0x90, "PA-RISC" },
        { 0x91, "PA-RISC 8500" },
        { 0x92, "PA-RISC 8000" },
        { 0x93, "PA-RISC 7300LC" },
        { 0x94, "PA-RISC 7200" },
        { 0x95, "PA-RISC 7100LC" },
        { 0x96, "PA-RISC 7100" },
        // 0x97 - 0x9F Available for assignment.
        { 0xA0, "V30" },
        { 0xA1, "Intel Xeon Quad-Core 3200" },
        { 0xA2, "Intel Xeon Dual-Core 3000" },
        { 0xA3, "Intel Xeon Quad-Core 5300" },
        { 0xA4, "Intel Xeon Dual-Core 5100" },
        { 0xA5, "Intel Xeon Dual-Core 5000" },
        { 0xA6, "Intel Xeon Dual-Core LV" },
        { 0xA7, "Intel Xeon Dual-Core ULV" },
        { 0xA8, "Intel Xeon Dual-Core 7100" },
        { 0xA9, "Intel Xeon Quad-Core 5400" },
        { 0xAA, "Intel Xeon Quad-Core" },
        { 0xAB, "Intel Xeon Dual-Core 5200" },
        { 0xAC, "Intel Xeon Dual-Core 7200" },
        { 0xAD, "Intel Xeon Quad-Core 7300" },
        { 0xAE, "Intel Xeon Quad-Core 7400" },
        { 0xAF, "Intel Xeon Multi-Core 7400" },
        { 0xB0, "Intel Pentium 3 Xeon" },
        { 0xB1, "Intel Pentium 3 with SpeedStep Technology" },
        { 0xB2, "Intel Pentium 4" },
        { 0xB3, "Intel Xeon" },
        { 0xB4, "AS400" },
        { 0xB5, "Intel Xeon MP" },
        { 0xB6, "AMD Athlon XP" },
        { 0xB7, "AMD Athlon MP" },
        { 0xB8, "Intel Itanium 2" },
        { 0xB9, "Intel Pentium M" },
        { 0xBA, "Intel Celeron D" },
        { 0xBB, "Intel Pentium D" },
        { 0xBC, "Intel Pentium Extreme Edition" },
        { 0xBD, "Intel Core Solo" },
        // 0xBE - Reserved.
        { 0xBF, "Intel Core 2 Duo" },
        { 0xC0, "Intel Core 2 Solo" },
        { 0xC1, "Intel Core 2 Extreme" },
        { 0xC2, "Intel Core 2 Quad" },
        { 0xC3, "Intel Core 2 Extreme Mobile" },
        { 0xC4, "Intel Core 2 Duo Mobile" },
        { 0xC5, "Intel Core 2 Solo Mobile" },
        { 0xC6, "Intel Core i7" },
        { 0xC7, "Intel Celeron Dual-Core" },
        { 0xC8, "IBM390" },
        { 0xC9, "G4" },
        { 0xCA, "G5" },
        { 0xCB, "ESA/390 G6" },
        { 0xCC, "z/Architectur base" },
        { 0xCD, "Intel Core i5" },
        { 0xCE, "Intel Core i3" },
        // 0xCF - 0xD1 Available for assignment.
        { 0xD2, "VIA C7-M" },
        { 0xD3, "VIA C7-D" },
        { 0xD4, "VIA C7" },
        { 0xD5, "VIA Eden" },
        { 0xD6, "Intel Xeon Multi-Core" },
        { 0xD7, "Intel Xeon Dual-Core 3xxx" },
        { 0xD8, "Intel Xeon Quad-Core 3xxx" },
        { 0xD9, "VIA Nano" },
        { 0xDA, "Intel Xeon Dual-Core 5xxx" },
        { 0xDB, "Intel Xeon Quad-Core 5xxx" },
        // 0xDC Available for assignment.
        { 0xDD, "Intel Xeon Dual-Core 7xxx" },
        { 0xDE, "Intel Xeon Quad-Core 7xxx" },
        { 0xDF, "Intel Xeon Multi-Core 7xxx" },
        { 0xE0, "Intel Xeon Multi-Core 3400" },
        // 0xE1 - 0xE5 Available for assignment.
        { 0xE6, "AMD Opteron Quad-Core Embedded" },
        { 0xE7, "AMD Phenom Triple-Core" },
        { 0xE8, "AMD Turion Ultra Dual-Core Mobile" },
        { 0xE9, "AMD Turion Dual-Core Mobile" },
        { 0xEA, "AMD Athlon Dual-Core" },
        { 0xEB, "AMD Sempron SI" },
        { 0xEC, "AMD Phenom 2" },
        { 0xED, "AMD Athlon 2" },
        { 0xEE, "AMD Opteron Six-Core" },
        { 0xEF, "AMD Sempron M" },
        // 0xF0 - 0xF9 Available for assignment.
        { 0xFA, "i860" },
        { 0xFB, "i960" },
        // 0xFC - 0xFD Available for assignment.
        // 0xFE - Indicator to obtain the processor family from the Processor Family 2 field.
        // 0xFF Reserved */
        // 0x100 - 0x1FF These values are available for assignment, except for the following:
        { 0x104, "SH-3" },
        { 0x105, "SH-4" },
        { 0x118, "ARM" },
        { 0x119, "StrongARM" },
        { 0x12C, "6x86" },
        { 0x12D, "MediaGX" },
        { 0x12E, "MII" },
        { 0x140, "WinChip" },
        { 0x15E, "DSP" },
        { 0x1F4, "Video Processor" }
        // 0x200 - 0xFFFD Available for assignment.
        // 0xFFFE - 0xFFFF - Reserved.
    };

    const uint8_t family = reader_.GetByte(0x06);

    for (size_t i = 0; i < _countof(list); ++i)
    {
        if (list[i].family == family)
            return list[i].name;
    }

    return std::string();
}

SMBios::ProcessorTable::Type SMBios::ProcessorTable::GetType() const
{
    if (reader_.GetTableLength() < 0x1A)
        return Type::UNKNOWN;

    return static_cast<Type>(reader_.GetByte(0x05));
}

SMBios::ProcessorTable::Status SMBios::ProcessorTable::GetStatus() const
{
    if (reader_.GetTableLength() < 0x1A)
        return Status::UNKNOWN;

    return static_cast<Status>(reader_.GetByte(0x18) & 0x07);
}

std::string SMBios::ProcessorTable::GetSocket() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    return reader_.GetString(0x04);
}

std::string SMBios::ProcessorTable::GetUpgrade() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    static const char* names[] =
    {
        "Other", // 0x01
        "Unknown",
        "Daughter Board",
        "ZIF Socket",
        "Replaceable Piggy Back",
        "None",
        "LIF Socket",
        "Slot 1",
        "Slot 2",
        "370-pin Socket",
        "Slot A",
        "Slot M",
        "Socket 423",
        "Socket A (Socket 462)",
        "Socket 478",
        "Socket 754",
        "Socket 940",
        "Socket 939",
        "Socket mPGA604",
        "Socket LGA771",
        "Socket LGA775",
        "Socket S1",
        "Socket AM2",
        "Socket F (1207)",
        "Socket LGA1366",
        "Socket G34",
        "Socket AM3",
        "Socket C32",
        "Socket LGA1156",
        "Socket LGA1567",
        "Socket PGA988A",
        "Socket BGA1288" // 0x20
    };

    const uint8_t upgrade = reader_.GetByte(0x19);
    if (upgrade >= 0x01 && upgrade <= 0x20)
        return names[upgrade - 0x01];

    return std::string();
}

int SMBios::ProcessorTable::GetExternalClock() const
{
    if (reader_.GetTableLength() < 0x1A)
        return 0;

    return reader_.GetWord(0x12);
}

int SMBios::ProcessorTable::GetCurrentSpeed() const
{
    if (reader_.GetTableLength() < 0x1A)
        return 0;

    return reader_.GetWord(0x16);
}

int SMBios::ProcessorTable::GetMaximumSpeed() const
{
    if (reader_.GetTableLength() < 0x1A)
        return 0;

    return reader_.GetWord(0x14);
}

double SMBios::ProcessorTable::GetVoltage() const
{
    if (reader_.GetTableLength() < 0x1A)
        return 0.0;

    const uint8_t voltage = reader_.GetByte(0x11);
    if (!voltage)
        return 0.0;

    if (voltage & 0x80)
        return double(voltage & 0x7F) / 10.0;

    if (voltage & 0x01)
        return 5.0;

    if (voltage & 0x02)
        return 3.3;

    if (voltage & 0x04)
        return 2.9;

    return 0.0;
}

std::string SMBios::ProcessorTable::GetSerialNumber() const
{
    if (reader_.GetTableLength() < 0x23)
        return std::string();

    return reader_.GetString(0x20);
}

std::string SMBios::ProcessorTable::GetAssetTag() const
{
    if (reader_.GetTableLength() < 0x23)
        return std::string();

    return reader_.GetString(0x21);
}

std::string SMBios::ProcessorTable::GetPartNumber() const
{
    if (reader_.GetTableLength() < 0x23)
        return std::string();

    return reader_.GetString(0x22);
}

int SMBios::ProcessorTable::GetCoreCount() const
{
    if (reader_.GetTableLength() < 0x28)
        return 0;

    return reader_.GetByte(0x23);
}

int SMBios::ProcessorTable::GetCoreEnabled() const
{
    if (reader_.GetTableLength() < 0x28)
        return 0;

    return reader_.GetByte(0x24);
}

int SMBios::ProcessorTable::GetThreadCount() const
{
    if (reader_.GetTableLength() < 0x28)
        return 0;

    return reader_.GetByte(0x25);
}

uint16_t SMBios::ProcessorTable::GetCharacteristics() const
{
    if (reader_.GetTableLength() < 0x28)
        return 0;

    return reader_.GetWord(0x26);
}

//
// CacheTable
//

SMBios::CacheTable::CacheTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::CacheTable::GetName() const
{
    return reader_.GetString(0x04);
}

SMBios::CacheTable::Location SMBios::CacheTable::GetLocation() const
{
    const uint16_t type = (reader_.GetWord(0x05) & 0x60) >> 5;

    switch (type)
    {
        case 0x00: return Location::INTERNAL;
        case 0x01: return Location::EXTERNAL;
        case 0x02: return Location::RESERVED;
        default: return Location::UNKNOWN;
    }
}

SMBios::CacheTable::Status SMBios::CacheTable::GetStatus() const
{
    const uint16_t status = (reader_.GetWord(0x05) & 0x80) >> 7;

    switch (status)
    {
        case 0x00: return Status::DISABLED;
        case 0x01: return Status::ENABLED;
        default: return Status::UNKNOWN;
    }
}

SMBios::CacheTable::Mode SMBios::CacheTable::GetMode() const
{
    const uint16_t mode = (reader_.GetWord(0x05) & 0x300) >> 8;

    switch (mode)
    {
        case 0x00: return Mode::WRITE_THRU;
        case 0x01: return Mode::WRITE_BACK;
        case 0x02: return Mode::WRITE_WITH_MEMORY_ADDRESS;
        default: return Mode::UNKNOWN;
    }
}

SMBios::CacheTable::Level SMBios::CacheTable::GetLevel() const
{
    const uint16_t level = reader_.GetWord(0x05) & 0x07;

    switch (level)
    {
        case 0x00: return Level::L1;
        case 0x01: return Level::L2;
        case 0x02: return Level::L3;
        default: return Level::UNKNOWN;
    }
}

int SMBios::CacheTable::GetMaximumSize() const
{
    return reader_.GetWord(0x07) & 0x7FFF;
}

int SMBios::CacheTable::GetCurrentSize() const
{
    return reader_.GetWord(0x09) & 0x7FFF;
}

uint16_t SMBios::CacheTable::GetSupportedSRAMTypes() const
{
    const uint16_t bitfield = reader_.GetWord(0x0B);

    if (!(bitfield & 0x007F))
        return 0;

    return bitfield;
}

SMBios::CacheTable::SRAMType SMBios::CacheTable::GetCurrentSRAMType() const
{
    return static_cast<SRAMType>(reader_.GetWord(0x0D));
}

int SMBios::CacheTable::GetSpeed() const
{
    if (reader_.GetTableLength() < 0x13)
        return 0;

    return reader_.GetByte(0x0F);
}

SMBios::CacheTable::ErrorCorrectionType SMBios::CacheTable::GetErrorCorrectionType() const
{
    if (reader_.GetTableLength() < 0x13)
        return ErrorCorrectionType::UNKNOWN;

    switch (reader_.GetByte(0x10))
    {
        case 0x01: return ErrorCorrectionType::OTHER;
        case 0x03: return ErrorCorrectionType::NONE;
        case 0x04: return ErrorCorrectionType::PARITY;
        case 0x05: return ErrorCorrectionType::SINGLE_BIT_ECC;
        case 0x06: return ErrorCorrectionType::MULTI_BIT_ECC;
        default: return ErrorCorrectionType::UNKNOWN;
    }
}

SMBios::CacheTable::Type SMBios::CacheTable::GetType() const
{
    if (reader_.GetTableLength() < 0x13)
        return Type::UNKNOWN;

    switch (reader_.GetByte(0x11))
    {
        case 0x01: return Type::OTHER;
        case 0x03: return Type::INSTRUCTION;
        case 0x04: return Type::DATA;
        case 0x05: return Type::UNIFIED;
        default: return Type::UNKNOWN;
    }
}

SMBios::CacheTable::Associativity SMBios::CacheTable::GetAssociativity() const
{
    if (reader_.GetTableLength() < 0x13)
        return Associativity::UNKNOWN;

    switch (reader_.GetByte(0x12))
    {
        case 0x01: return Associativity::OTHER;
        case 0x03: return Associativity::DIRECT_MAPPED;
        case 0x04: return Associativity::WAY_2_SET_ASSOCIATIVE;
        case 0x05: return Associativity::WAY_4_SET_ASSOCIATIVE;
        case 0x06: return Associativity::FULLY_ASSOCIATIVE;
        case 0x07: return Associativity::WAY_8_SET_ASSOCIATIVE;
        case 0x08: return Associativity::WAY_16_SET_ASSOCIATIVE;
        case 0x09: return Associativity::WAY_12_SET_ASSOCIATIVE;
        case 0x0A: return Associativity::WAY_24_SET_ASSOCIATIVE;
        case 0x0B: return Associativity::WAY_32_SET_ASSOCIATIVE;
        case 0x0C: return Associativity::WAY_48_SET_ASSOCIATIVE;
        case 0x0D: return Associativity::WAY_64_SET_ASSOCIATIVE;
        case 0x0E: return Associativity::WAY_20_SET_ASSOCIATIVE;
        default: return Associativity::UNKNOWN;
    }
}

//
// PortConnectorTable
//

SMBios::PortConnectorTable::PortConnectorTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::PortConnectorTable::GetInternalDesignation() const
{
    if (reader_.GetTableLength() < 0x09)
        return std::string();

    return reader_.GetString(0x04);
}

std::string SMBios::PortConnectorTable::GetExternalDesignation() const
{
    if (reader_.GetTableLength() < 0x09)
        return std::string();

    return reader_.GetString(0x06);
}

std::string SMBios::PortConnectorTable::GetType() const
{
    if (reader_.GetTableLength() < 0x09)
        return std::string();

    struct PortType
    {
        uint16_t type;
        const char* name;
    } const list[] =
    {
        { 0x01, "Parallel Port XT/AT Compatible" },
        { 0x02, "Parallel Port PS/2" },
        { 0x03, "Parallel Port ECP" },
        { 0x04, "Parallel Port EPP" },
        { 0x05, "Parallel Port ECP/EPP" },
        { 0x06, "Serial Port XT/AT Compatible" },
        { 0x07, "Serial Port 16450 Compatible" },
        { 0x08, "Serial Port 16550 Compatible" },
        { 0x09, "Serial Port 16550A Compatible" },
        { 0x0A, "SCSI Port" },
        { 0x0B, "MIDI Port" },
        { 0x0C, "Joy Stick Port" },
        { 0x0D, "Keyboard Port" },
        { 0x0E, "Mouse Port" },
        { 0x0F, "SSA SCSI" },
        { 0x10, "USB" },
        { 0x11, "FireWire (IEEE P1394)" },
        { 0x12, "PCMCIA Type I" },
        { 0x13, "PCMCIA Type II" },
        { 0x14, "PCMCIA Type III" },
        { 0x15, "Cardbus" },
        { 0x16, "Access Bus Port" },
        { 0x17, "SCSI II" },
        { 0x18, "SCSI Wide" },
        { 0x19, "PC-98" },
        { 0x1A, "PC-98-Hireso" },
        { 0x1B, "PC-H98" },
        { 0x1C, "Video Port" },
        { 0x1D, "Audio Port" },
        { 0x1E, "Modem Port" },
        { 0x1F, "Network Port" },
        { 0x20, "SATA" },
        { 0x21, "SAS" },
        { 0xA0, "8251 Compatible" },
        { 0xA1, "8251 FIFO Compatible" },
        { 0x0FF, "Other" }
    };

    const uint8_t type = reader_.GetByte(0x08);

    for (size_t i = 0; i < _countof(list); ++i)
    {
        if (list[i].type == type)
            return list[i].name;
    }

    return std::string();
}

// static
std::string SMBios::PortConnectorTable::ConnectorTypeToString(uint8_t type)
{
    struct PortConnectors
    {
        uint8_t type;
        const char* name;
    } const list[] =
    {
        { 0x01, "Centronics" },
        { 0x02, "Mini Centronics" },
        { 0x03, "Proprietary" },
        { 0x04, "DB-25 pin male" },
        { 0x05, "DB-25 pin female" },
        { 0x06, "DB-15 pin male" },
        { 0x07, "DB-15 pin female" },
        { 0x08, "DB-9 pin male" },
        { 0x09, "DB-9 pin female" },
        { 0x0A, "RJ-11" },
        { 0x0B, "RJ-45" },
        { 0x0C, "50-pin MiniSCSI" },
        { 0x0D, "Mini-DIN" },
        { 0x0E, "Micro-DIN" },
        { 0x0F, "PS/2" },
        { 0x10, "Infrared" },
        { 0x11, "HP-HIL" },
        { 0x12, "Access Bus (USB)" },
        { 0x13, "SSA SCSI" },
        { 0x14, "Circular DIN-8 male" },
        { 0x15, "Circular DIN-8 female" },
        { 0x16, "On Board IDE" },
        { 0x17, "On Board Floppy" },
        { 0x18, "9-pin Dual Inline (pin 10 cut)" },
        { 0x19, "25-pin Dual Inline (pin 26 cut)" },
        { 0x1A, "50-pin Dual Inline" },
        { 0x1B, "68-pin Dual Inline" },
        { 0x1C, "On Board Sound Input from CD-ROM" },
        { 0x1D, "Mini-Centronics Type-14" },
        { 0x1E, "Mini-Centronics Type-26" },
        { 0x1F, "Mini-jack (headphones)" },
        { 0x20, "BNC" },
        { 0x21, "1394" },
        { 0x22, "SAS/SATA Plug Receptacle" },
        { 0xA0, "PC-98" },
        { 0xA1, "PC-98Hireso" },
        { 0xA2, "PC-H98" },
        { 0xA3, "PC-98Note" },
        { 0xA4, "PC-98Full" },
        { 0xFF, "Other" }
    };

    for (size_t i = 0; i < _countof(list); ++i)
    {
        if (list[i].type == type)
            return list[i].name;
    }

    return std::string();
}

std::string SMBios::PortConnectorTable::GetInternalConnectorType() const
{
    if (reader_.GetTableLength() < 0x09)
        return std::string();

    return ConnectorTypeToString(reader_.GetByte(0x05));
}

std::string SMBios::PortConnectorTable::GetExternalConnectorType() const
{
    if (reader_.GetTableLength() < 0x09)
        return std::string();

    return ConnectorTypeToString(reader_.GetByte(0x07));
}

//
// SystemSlotTable
//

SMBios::SystemSlotTable::SystemSlotTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::SystemSlotTable::GetSlotDesignation() const
{
    if (reader_.GetTableLength() < 0x0C)
        return std::string();

    return reader_.GetString(0x04);
}

std::string SMBios::SystemSlotTable::GetType() const
{
    if (reader_.GetTableLength() < 0x0C)
        return std::string();

    const uint8_t type = reader_.GetByte(0x05);

    static const char* names[] =
    {
        "Other", // 0x01
        "Unknown",
        "ISA",
        "MCA",
        "EISA",
        "PCI",
        "PC Card (PCMCIA)",
        "VLB",
        "Proprietary",
        "Processor Card",
        "Proprietary Memory Card",
        "I/O Riser Card",
        "NuBus",
        "PCI-66",
        "AGP",
        "AGP 2x",
        "AGP 4x",
        "PCI-X",
        "AGP 8x" // 0x13
    };

    if (type >= 0x01 && type <= 0x13)
        return names[type - 0x01];

    static const char* names_0xA0[] =
    {
        "PC-98/C20", // 0xA0
        "PC-98/C24",
        "PC-98/E",
        "PC-98/Local Bus",
        "PC-98/Card",
        "PCI Express",
        "PCI Express x1",
        "PCI Express x2",
        "PCI Express x4",
        "PCI Express x8",
        "PCI Express x16",
        "PCI Express 2",
        "PCI Express 2 x1",
        "PCI Express 2 x2",
        "PCI Express 2 x4",
        "PCI Express 2 x8",
        "PCI Express 2 x16" // 0xB0
    };

    if (type >= 0xA0 && type <= 0xB0)
        return names_0xA0[type - 0xA0];

    return std::string();
}

SMBios::SystemSlotTable::Usage SMBios::SystemSlotTable::GetUsage() const
{
    if (reader_.GetTableLength() < 0x0C)
        return Usage::UNKNOWN;

    return static_cast<Usage>(reader_.GetByte(0x07));
}

std::string SMBios::SystemSlotTable::GetBusWidth() const
{
    if (reader_.GetTableLength() < 0x0C)
        return std::string();

    static const char* names[] =
    {
        "Other", // 0x01, Other
        nullptr, // Unknown
        "8-bit",
        "16-bit",
        "32-bit",
        "64-bit",
        "128-bit",
        "x1",
        "x2",
        "x4",
        "x8",
        "x12",
        "x16",
        "x32" // 0x0E
    };

    const uint8_t width = reader_.GetByte(0x06);

    if (width >= 0x01 && width <= 0x0E && names[width - 0x01])
        return names[width - 0x01];

    return std::string();
}

SMBios::SystemSlotTable::Length SMBios::SystemSlotTable::GetLength() const
{
    if (reader_.GetTableLength() < 0x0C)
        return Length::UNKNOWN;

    return static_cast<Length>(reader_.GetByte(0x08));
}

//
// OnBoardDeviceTable
//

SMBios::OnBoardDeviceTable::OnBoardDeviceTable(const TableReader& reader)
    : count_((reader.GetTableLength() - 4) / 2),
      ptr_(reader.GetPointer(0) + 4),
      reader_(reader)
{
    // Nothing
}

int SMBios::OnBoardDeviceTable::GetDeviceCount() const
{
    return count_;
}

std::string SMBios::OnBoardDeviceTable::GetDescription(int index) const
{
    DCHECK(index < count_);

    uint8_t handle = ptr_[2 * index + 1];
    if (!handle)
        return std::string();

    char* string = reinterpret_cast<char*>(const_cast<uint8_t*>(
        reader_.GetPointer(0))) + reader_.GetTableLength();

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

std::string SMBios::OnBoardDeviceTable::GetType(int index) const
{
    DCHECK(index < count_);
    const uint8_t type = ptr_[2 * index] & 0x7F;

    static const char* names[] =
    {
        "Other", // 0x01
        "Unknown",
        "Video",
        "SCSI Controller",
        "Ethernet",
        "Token Ring",
        "Sound",
        "PATA Controller",
        "SATA Controller",
        "SAS Controller" // 0x0A
    };

    if (type >= 0x01 && type <= 0x0A)
        return names[type - 0x01];

    return std::string();
}

bool SMBios::OnBoardDeviceTable::IsEnabled(int index) const
{
    DCHECK(index < count_);
    return !!(ptr_[2 * index] & 0x80);
}

//
// MemoryDeviceTable
//

SMBios::MemoryDeviceTable::MemoryDeviceTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::MemoryDeviceTable::GetDeviceLocator() const
{
    if (reader_.GetTableLength() < 0x15)
        return std::string();

    return reader_.GetString(0x10);
}

int SMBios::MemoryDeviceTable::GetSize() const
{
    if (reader_.GetTableLength() < 0x15)
        return 0;

    return reader_.GetWord(0x0C) & 0x07FFF;
}

std::string SMBios::MemoryDeviceTable::GetType() const
{
    if (reader_.GetTableLength() < 0x15)
        return std::string();

    const uint8_t type = reader_.GetByte(0x12);
    if (type == 0x02)
        return std::string();

    struct TypeList
    {
        uint8_t type;
        const char* name;
    } const list[] =
    {
        { 0x01, "Other" },
        { 0x02, "Unknown" },
        { 0x03, "DRAM" },
        { 0x04, "EDRAM" },
        { 0x05, "VRAM" },
        { 0x06, "SRAM" },
        { 0x07, "RAM" },
        { 0x08, "ROM" },
        { 0x09, "Flash" },
        { 0x0A, "EEPROM" },
        { 0x0B, "FEPROM" },
        { 0x0C, "EPROM" },
        { 0x0D, "CDRAM" },
        { 0x0E, "3DRAM" },
        { 0x0F, "SDRAM" },
        { 0x10, "SGRAM" },
        { 0x11, "RDRAM" },
        { 0x12, "DDR" },
        { 0x13, "DDR2" },
        { 0x14, "DDR2 FB-DIMM" },
        // 0x15 - 0x17 Reserved.
        { 0x18, "DDR3" },
        { 0x19, "FBD2" },
        { 0x1A, "DDR4" },
        { 0x1B, "LPDDR" },
        { 0x1C, "LPDDR2" },
        { 0x1D, "LPDDR3" },
        { 0x1E, "LPDDR4" }
    };

    for (size_t i = 0; i < _countof(list); ++i)
    {
        if (list[i].type == type)
            return list[i].name;
    }

    return std::string();
}

int SMBios::MemoryDeviceTable::GetSpeed() const
{
    if (reader_.GetTableLength() < 0x15)
        return 0;

    return reader_.GetWord(0x15);
}

std::string SMBios::MemoryDeviceTable::GetFormFactor() const
{
    if (reader_.GetTableLength() < 0x15)
        return std::string();

    const uint8_t form_factor = reader_.GetByte(0x0E);

    struct FormFactor
    {
        uint8_t form_factor;
        const char* name;
    } const list[] =
    {
        { 0x01, "Other" },
        { 0x02, "Unknown" },
        { 0x03, "SIMM" },
        { 0x04, "SIP" },
        { 0x05, "Chip" },
        { 0x06, "DIP" },
        { 0x07, "ZIP" },
        { 0x08, "Proprietary Card" },
        { 0x09, "DIMM" },
        { 0x0A, "TSOP" },
        { 0x0B, "Row of chips" },
        { 0x0C, "RIMM" },
        { 0x0D, "SODIMM" },
        { 0x0E, "SRIMM" },
        { 0x0F, "FB-DIMM" }
    };

    for (size_t i = 0; i < _countof(list); ++i)
    {
        if (list[i].form_factor == form_factor)
            return list[i].name;
    }

    return std::string();
}

std::string SMBios::MemoryDeviceTable::GetSerialNumber() const
{
    if (reader_.GetTableLength() < 0x15)
        return std::string();

    return reader_.GetString(0x18);
}

std::string SMBios::MemoryDeviceTable::GetPartNumber() const
{
    if (reader_.GetTableLength() < 0x15)
        return std::string();

    return reader_.GetString(0x1A);
}

std::string SMBios::MemoryDeviceTable::GetManufacturer() const
{
    if (reader_.GetTableLength() < 0x15)
        return std::string();

    return reader_.GetString(0x17);
}

std::string SMBios::MemoryDeviceTable::GetBank() const
{
    if (reader_.GetTableLength() < 0x15)
        return std::string();

    return reader_.GetString(0x11);
}

int SMBios::MemoryDeviceTable::GetTotalWidth() const
{
    if (reader_.GetTableLength() < 0x15)
        return 0;

    return reader_.GetWord(0x08);
}

int SMBios::MemoryDeviceTable::GetDataWidth() const
{
    if (reader_.GetTableLength() < 0x15)
        return 0;

    return reader_.GetWord(0x0A);
}

//
// BuildinPointingTable
//

SMBios::BuildinPointingTable::BuildinPointingTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::BuildinPointingTable::GetDeviceType() const
{
    if (reader_.GetTableLength() < 0x07)
        return std::string();

    static const char* names[] =
    {
        "Other", // 0x01
        nullptr,
        "Mouse",
        "Track Ball",
        "Track Point",
        "Glide Point",
        "Touch Pad",
        "Touch Screen",
        "Optical Sensor" // 0x09
    };

    const uint8_t type = reader_.GetByte(0x04);

    if (type >= 0x01 && type <= 0x09 && names[type - 0x01])
        return names[type - 0x01];

    return std::string();
}

std::string SMBios::BuildinPointingTable::GetInterface() const
{
    if (reader_.GetTableLength() < 0x07)
        return std::string();

    const uint8_t type = reader_.GetByte(0x05);

    static const char* names[] =
    {
        "Other", // 0x01
        nullptr,
        "Serial",
        "PS/2",
        "Infrared",
        "HIP-HIL",
        "Bus Mouse",
        "ADB (Apple Desktop Bus)" // 0x08
    };

    if (type >= 0x01 && type <= 0x08 && names[type - 0x01])
        return names[type - 0x01];

    static const char* names_0xA0[] =
    {
        "Bus Mouse DB-9", // 0xA0
        "Bus Mouse Micro DIN",
        "USB" // 0xA2
    };

    if (type >= 0xA0 && type <= 0xA2)
        return names_0xA0[type - 0xA0];

    return std::string();
}

int SMBios::BuildinPointingTable::GetButtonCount() const
{
    if (reader_.GetTableLength() < 0x07)
        return 0;

    return reader_.GetByte(0x06);
}

} // namespace aspia
