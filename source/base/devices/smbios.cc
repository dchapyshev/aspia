//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/smbios.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios.h"
#include "base/strings/string_util.h"
#include "base/bitset.h"
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

SMBios::TableReader& SMBios::TableReader::operator=(const TableReader& other)
{
    smbios_ = other.smbios_;
    table_ = other.table_;
    return *this;
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

    std::string output;
    TrimWhitespaceASCII(string, TRIM_ALL, output);
    return output;
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

uint64_t SMBios::BiosTable::GetSize() const
{
    const uint8_t old_size = reader_.GetByte(0x09);
    if (old_size != 0xFF)
        return (old_size + 1) << 6;

    BitSet<uint16_t> bitfield(reader_.GetWord(0x18));

    uint16_t size = 16; // By default 16 MBytes.

    if (reader_.GetTableLength() >= 0x1A)
        size = bitfield.Range(0, 13);

    switch (bitfield.Range(14, 15))
    {
        case 0x0000: // MB
            return static_cast<uint64_t>(size) * 1024ULL;

        case 0x0001: // GB
            return static_cast<uint64_t>(size) * 1024ULL * 1024ULL;

        default:
            return 0;
    }
}

std::string SMBios::BiosTable::GetBiosRevision() const
{
    const uint8_t major = reader_.GetByte(0x14);
    const uint8_t minor = reader_.GetByte(0x15);

    if (major != 0xFF && minor != 0xFF)
        return StringPrintf("%u.%u", major, minor);

    return std::string();
}

std::string SMBios::BiosTable::GetFirmwareRevision() const
{
    const uint8_t major = reader_.GetByte(0x16);
    const uint8_t minor = reader_.GetByte(0x17);

    if (major != 0xFF && minor != 0xFF)
        return StringPrintf("%u.%u", major, minor);

    return std::string();
}

std::string SMBios::BiosTable::GetAddress() const
{
    const uint16_t address = reader_.GetWord(0x06);

    if (address != 0)
        return StringPrintf("%04X0h", address);

    return std::string();
}

int SMBios::BiosTable::GetRuntimeSize() const
{
    const uint16_t address = reader_.GetWord(0x06);
    if (address == 0)
        return 0;

    const uint32_t code = (0x10000 - address) << 4;

    if (code & 0x000003FF)
        return code;

    return (code >> 10) * 1024;
}

uint64_t SMBios::BiosTable::GetCharacteristics() const
{
    BitSet<uint64_t> characteristics = reader_.GetQword(0x0A);
    if (characteristics.Test(3))
        return proto::DmiBios::CHARACTERISTIC_NONE;

    uint64_t ret = proto::DmiBios::CHARACTERISTIC_NONE;

    if (characteristics.Test(4)) ret |= proto::DmiBios::CHARACTERISTIC_ISA;
    if (characteristics.Test(5)) ret |= proto::DmiBios::CHARACTERISTIC_MCA;
    if (characteristics.Test(6)) ret |= proto::DmiBios::CHARACTERISTIC_EISA;
    if (characteristics.Test(7)) ret |= proto::DmiBios::CHARACTERISTIC_PCI;
    if (characteristics.Test(8)) ret |= proto::DmiBios::CHARACTERISTIC_PC_CARD;
    if (characteristics.Test(9)) ret |= proto::DmiBios::CHARACTERISTIC_PLUG_AND_PLAY;
    if (characteristics.Test(10)) ret |= proto::DmiBios::CHARACTERISTIC_APM;
    if (characteristics.Test(11)) ret |= proto::DmiBios::CHARACTERISTIC_BIOS_IS_UPGRADEABLE;
    if (characteristics.Test(12)) ret |= proto::DmiBios::CHARACTERISTIC_BIOS_SHADOWING;
    if (characteristics.Test(13)) ret |= proto::DmiBios::CHARACTERISTIC_VLB;
    if (characteristics.Test(14)) ret |= proto::DmiBios::CHARACTERISTIC_ESCD;
    if (characteristics.Test(15)) ret |= proto::DmiBios::CHARACTERISTIC_BOOT_FROM_CD;
    if (characteristics.Test(16)) ret |= proto::DmiBios::CHARACTERISTIC_SELECTABLE_BOOT;
    if (characteristics.Test(17)) ret |= proto::DmiBios::CHARACTERISTIC_BOOT_ROM_IS_SOCKETED;
    if (characteristics.Test(18)) ret |= proto::DmiBios::CHARACTERISTIC_BOOT_FROM_PC_CARD;
    if (characteristics.Test(19)) ret |= proto::DmiBios::CHARACTERISTIC_EDD;
    if (characteristics.Test(20)) ret |= proto::DmiBios::CHARACTERISTIC_JAPANESE_FLOPPY_FOR_NEC9800;
    if (characteristics.Test(21)) ret |= proto::DmiBios::CHARACTERISTIC_JAPANESE_FLOPPY_FOR_TOSHIBA;
    if (characteristics.Test(22)) ret |= proto::DmiBios::CHARACTERISTIC_525_360KB_FLOPPY;
    if (characteristics.Test(23)) ret |= proto::DmiBios::CHARACTERISTIC_525_12MB_FLOPPY;
    if (characteristics.Test(24)) ret |= proto::DmiBios::CHARACTERISTIC_35_720KB_FLOPPY;
    if (characteristics.Test(25)) ret |= proto::DmiBios::CHARACTERISTIC_35_288MB_FLOPPY;
    if (characteristics.Test(26)) ret |= proto::DmiBios::CHARACTERISTIC_PRINT_SCREEN;
    if (characteristics.Test(27)) ret |= proto::DmiBios::CHARACTERISTIC_8042_KEYBOARD;
    if (characteristics.Test(28)) ret |= proto::DmiBios::CHARACTERISTIC_SERIAL;
    if (characteristics.Test(29)) ret |= proto::DmiBios::CHARACTERISTIC_PRINTER;
    if (characteristics.Test(30)) ret |= proto::DmiBios::CHARACTERISTIC_CGA_VIDEO;
    if (characteristics.Test(31)) ret |= proto::DmiBios::CHARACTERISTIC_NEC_PC98;

    return ret;
}

uint32_t SMBios::BiosTable::GetCharacteristics1() const
{
    if (reader_.GetTableLength() < 0x13)
        return 0;

    BitSet<uint8_t> characteristics1 = reader_.GetByte(0x12);
    uint32_t ret = 0;

    if (characteristics1.Test(0)) ret |= proto::DmiBios::CHARACTERISTIC1_ACPI;
    if (characteristics1.Test(1)) ret |= proto::DmiBios::CHARACTERISTIC1_USB_LEGACY;
    if (characteristics1.Test(2)) ret |= proto::DmiBios::CHARACTERISTIC1_AGP;
    if (characteristics1.Test(3)) ret |= proto::DmiBios::CHARACTERISTIC1_I2O_BOOT;
    if (characteristics1.Test(4)) ret |= proto::DmiBios::CHARACTERISTIC1_LS120_BOOT;
    if (characteristics1.Test(5)) ret |= proto::DmiBios::CHARACTERISTIC1_ATAPI_ZIP_DRIVE_BOOT;
    if (characteristics1.Test(6)) ret |= proto::DmiBios::CHARACTERISTIC1_IEEE1394_BOOT;
    if (characteristics1.Test(7)) ret |= proto::DmiBios::CHARACTERISTIC1_SMART_BATTERY;

    return ret;
}

uint32_t SMBios::BiosTable::GetCharacteristics2() const
{
    if (reader_.GetTableLength() < 0x14)
        return 0;

    BitSet<uint8_t> characteristics2 = reader_.GetByte(0x13);
    uint32_t ret = 0;

    if (characteristics2.Test(0)) ret |= proto::DmiBios::CHARACTERISTIC2_BIOS_BOOT_SPECIFICATION;
    if (characteristics2.Test(1)) ret |= proto::DmiBios::CHARACTERISTIC2_KEY_INITIALIZED_NETWORK_BOOT;
    if (characteristics2.Test(2)) ret |= proto::DmiBios::CHARACTERISTIC2_TARGETED_CONTENT_DISTRIBUTION;

    return ret;
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

    const uint8_t features = reader_.GetByte(0x09);
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

proto::DmiBaseboard::BoardType SMBios::BaseboardTable::GetBoardType() const
{
    if (reader_.GetTableLength() < 0x0E)
        return proto::DmiBaseboard::BOARD_TYPE_UNKNOWN;

    switch (reader_.GetByte(0x0D))
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

proto::DmiChassis::Type SMBios::ChassisTable::GetType() const
{
    if (reader_.GetTableLength() < 0x09)
        return proto::DmiChassis::TYPE_UNKNOWN;

    switch (reader_.GetByte(0x05) & 0x7F)
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

proto::DmiChassis::Status SMBios::ChassisTable::GetOSLoadStatus() const
{
    if (reader_.GetTableLength() < 0x0D)
        return proto::DmiChassis::STATUS_UNKNOWN;

    switch (reader_.GetByte(0x09))
    {
        case 0x01: return proto::DmiChassis::STATUS_OTHER;
        case 0x03: return proto::DmiChassis::STATUS_SAFE;
        case 0x04: return proto::DmiChassis::STATUS_WARNING;
        case 0x05: return proto::DmiChassis::STATUS_CRITICAL;
        case 0x06: return proto::DmiChassis::STATUS_NON_RECOVERABLE;
        default: return proto::DmiChassis::STATUS_UNKNOWN;
    }
}

proto::DmiChassis::Status SMBios::ChassisTable::GetPowerSourceStatus() const
{
    if (reader_.GetTableLength() < 0x0D)
        return proto::DmiChassis::STATUS_UNKNOWN;

    switch (reader_.GetByte(0x0A))
    {
        case 0x01: return proto::DmiChassis::STATUS_OTHER;
        case 0x03: return proto::DmiChassis::STATUS_SAFE;
        case 0x04: return proto::DmiChassis::STATUS_WARNING;
        case 0x05: return proto::DmiChassis::STATUS_CRITICAL;
        case 0x06: return proto::DmiChassis::STATUS_NON_RECOVERABLE;
        default: return proto::DmiChassis::STATUS_UNKNOWN;
    }
}

proto::DmiChassis::Status SMBios::ChassisTable::GetTemperatureStatus() const
{
    if (reader_.GetTableLength() < 0x0D)
        return proto::DmiChassis::STATUS_UNKNOWN;

    switch (reader_.GetByte(0x0B))
    {
        case 0x01: return proto::DmiChassis::STATUS_OTHER;
        case 0x03: return proto::DmiChassis::STATUS_SAFE;
        case 0x04: return proto::DmiChassis::STATUS_WARNING;
        case 0x05: return proto::DmiChassis::STATUS_CRITICAL;
        case 0x06: return proto::DmiChassis::STATUS_NON_RECOVERABLE;
        default: return proto::DmiChassis::STATUS_UNKNOWN;
    }
}

proto::DmiChassis::SecurityStatus SMBios::ChassisTable::GetSecurityStatus() const
{
    if (reader_.GetTableLength() < 0x0D)
        return proto::DmiChassis::SECURITY_STATUS_UNKNOWN;

    switch (reader_.GetByte(0x0C))
    {
        case 0x01: return proto::DmiChassis::SECURITY_STATUS_OTHER;
        case 0x03: return proto::DmiChassis::SECURITY_STATUS_NONE;
        case 0x04: return proto::DmiChassis::SECURITY_STATUS_EXTERNAL_INTERFACE_LOCKED_OUT;
        case 0x05: return proto::DmiChassis::SECURITY_STATUS_EXTERNAL_INTERFACE_ENABLED;
        default: return proto::DmiChassis::SECURITY_STATUS_UNKNOWN;
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
        { 0x2C, "Core M" },
        { 0x2D, "Core m3" },
        { 0x2E, "Core m5" },
        { 0x2F, "Core m7" },
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
        { 0x3F, "FX" },
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
        { 0x4A, "AMD Z-Series" },
        { 0x4B, "AMD R-Series" },
        { 0x4C, "Opteron 4300" },
        { 0x4D, "Opteron 6300" },
        { 0x4E, "Opteron 3300" },
        { 0x4F, "FirePro" },
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
        { 0x66, "Athlon X4" },
        { 0x67, "Opteron X1000" },
        { 0x68, "Opteron X2000" },
        { 0x69, "Opteron A-Series" },
        { 0x6A, "Opteron X3000" },
        { 0x6B, "Zen" },
        // 0x6C - 0x6F Available for assignment.
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
        // 0xE1 - 0xE3 Available for assignment.
        { 0xE4, "Opteron 3000" },
        { 0xE5, "Sempron II" },
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

proto::DmiProcessors::Type SMBios::ProcessorTable::GetType() const
{
    if (reader_.GetTableLength() < 0x1A)
        return proto::DmiProcessors::TYPE_UNKNOWN;

    switch (reader_.GetByte(0x05))
    {
        case 0x01: return proto::DmiProcessors::TYPE_OTHER;
        case 0x02: return proto::DmiProcessors::TYPE_UNKNOWN;
        case 0x03: return proto::DmiProcessors::TYPE_CENTRAL_PROCESSOR;
        case 0x04: return proto::DmiProcessors::TYPE_MATH_PROCESSOR;
        case 0x05: return proto::DmiProcessors::TYPE_DSP_PROCESSOR;
        case 0x06: return proto::DmiProcessors::TYPE_VIDEO_PROCESSOR;
        default: return proto::DmiProcessors::TYPE_UNKNOWN;
    }
}

proto::DmiProcessors::Status SMBios::ProcessorTable::GetStatus() const
{
    if (reader_.GetTableLength() < 0x1A)
        return proto::DmiProcessors::STATUS_UNKNOWN;

    switch (BitSet<uint8_t>(reader_.GetByte(0x18)).Range(0, 2))
    {
        case 0x00: return proto::DmiProcessors::STATUS_UNKNOWN;
        case 0x01: return proto::DmiProcessors::STATUS_ENABLED;
        case 0x02: return proto::DmiProcessors::STATUS_DISABLED_BY_USER;
        case 0x03: return proto::DmiProcessors::STATUS_DISABLED_BY_BIOS;
        case 0x04: return proto::DmiProcessors::STATUS_IDLE;
        case 0x07: return proto::DmiProcessors::STATUS_OTHER;
        default: return proto::DmiProcessors::STATUS_UNKNOWN;
    }
}

std::string SMBios::ProcessorTable::GetSocket() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    return reader_.GetString(0x04);
}

proto::DmiProcessors::Upgrade SMBios::ProcessorTable::GetUpgrade() const
{
    if (reader_.GetTableLength() < 0x1A)
        return proto::DmiProcessors::UPGRADE_UNKNOWN;

    switch (reader_.GetByte(0x19))
    {
        case 0x01: return proto::DmiProcessors::UPGRADE_OTHER;
        case 0x02: return proto::DmiProcessors::UPGRADE_UNKNOWN;
        case 0x03: return proto::DmiProcessors::UPGRADE_DAUGHTER_BOARD;
        case 0x04: return proto::DmiProcessors::UPGRADE_ZIF_SOCKET;
        case 0x05: return proto::DmiProcessors::UPGRADE_REPLACEABLE_PIGGY_BACK;
        case 0x06: return proto::DmiProcessors::UPGRADE_NONE;
        case 0x07: return proto::DmiProcessors::UPGRADE_LIF_SOCKET;
        case 0x08: return proto::DmiProcessors::UPGRADE_SLOT_1;
        case 0x09: return proto::DmiProcessors::UPGRADE_SLOT_2;
        case 0x0A: return proto::DmiProcessors::UPGRADE_370_PIN_SOCKET;
        case 0x0B: return proto::DmiProcessors::UPGRADE_SLOT_A;
        case 0x0C: return proto::DmiProcessors::UPGRADE_SLOT_M;
        case 0x0D: return proto::DmiProcessors::UPGRADE_SOCKET_423;
        case 0x0E: return proto::DmiProcessors::UPGRADE_SOCKET_462;
        case 0x0F: return proto::DmiProcessors::UPGRADE_SOCKET_478;
        case 0x10: return proto::DmiProcessors::UPGRADE_SOCKET_754;
        case 0x11: return proto::DmiProcessors::UPGRADE_SOCKET_940;
        case 0x12: return proto::DmiProcessors::UPGRADE_SOCKET_939;
        case 0x13: return proto::DmiProcessors::UPGRADE_SOCKET_MPGA604;
        case 0x14: return proto::DmiProcessors::UPGRADE_SOCKET_LGA771;
        case 0x15: return proto::DmiProcessors::UPGRADE_SOCKET_LGA775;
        case 0x16: return proto::DmiProcessors::UPGRADE_SOCKET_S1;
        case 0x17: return proto::DmiProcessors::UPGRADE_SOCKET_AM2;
        case 0x18: return proto::DmiProcessors::UPGRADE_SOCKET_F;
        case 0x19: return proto::DmiProcessors::UPGRADE_SOCKET_LGA1366;
        case 0x1A: return proto::DmiProcessors::UPGRADE_SOCKET_G34;
        case 0x1B: return proto::DmiProcessors::UPGRADE_SOCKET_AM3;
        case 0x1C: return proto::DmiProcessors::UPGRADE_SOCKET_C32;
        case 0x1D: return proto::DmiProcessors::UPGRADE_SOCKET_LGA1156;
        case 0x1E: return proto::DmiProcessors::UPGRADE_SOCKET_LGA1567;
        case 0x1F: return proto::DmiProcessors::UPGRADE_SOCKET_PGA988A;
        case 0x20: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1288;
        case 0x21: return proto::DmiProcessors::UPGRADE_SOCKET_RPGA988B;
        case 0x22: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1023;
        case 0x23: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1224;
        case 0x24: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1155;
        case 0x25: return proto::DmiProcessors::UPGRADE_SOCKET_LGA1356;
        case 0x26: return proto::DmiProcessors::UPGRADE_SOCKET_LGA2011;
        case 0x27: return proto::DmiProcessors::UPGRADE_SOCKET_FS1;
        case 0x28: return proto::DmiProcessors::UPGRADE_SOCKET_FS2;
        case 0x29: return proto::DmiProcessors::UPGRADE_SOCKET_FM1;
        case 0x2A: return proto::DmiProcessors::UPGRADE_SOCKET_FM2;
        case 0x2B: return proto::DmiProcessors::UPGRADE_SOCKET_LGA2011_3;
        case 0x2C: return proto::DmiProcessors::UPGRADE_SOCKET_LGA1356_3;
        case 0x2D: return proto::DmiProcessors::UPGRADE_SOCKET_LGA1150;
        case 0x2E: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1168;
        case 0x2F: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1234;
        case 0x30: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1364;
        case 0x31: return proto::DmiProcessors::UPGRADE_SOCKET_AM4;
        case 0x32: return proto::DmiProcessors::UPGRADE_SOCKET_LGA1151;
        case 0x33: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1356;
        case 0x34: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1440;
        case 0x35: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1515;
        case 0x36: return proto::DmiProcessors::UPGRADE_SOCKET_LGA3647_1;
        case 0x37: return proto::DmiProcessors::UPGRADE_SOCKET_SP3;
        case 0x38: return proto::DmiProcessors::UPGRADE_SOCKET_SP3_R2;
        default: return proto::DmiProcessors::UPGRADE_UNKNOWN;
    }
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

uint32_t SMBios::ProcessorTable::GetCharacteristics() const
{
    if (reader_.GetTableLength() < 0x28)
        return 0;

    BitSet<uint16_t> bitfield(reader_.GetWord(0x26));
    uint32_t characteristics = 0;

    if (bitfield.Test(2))
        characteristics |= proto::DmiProcessors::CHARACTERISTIC_64BIT_CAPABLE;

    if (bitfield.Test(3))
        characteristics |= proto::DmiProcessors::CHARACTERISTIC_MULTI_CORE;

    if (bitfield.Test(4))
        characteristics |= proto::DmiProcessors::CHARACTERISTIC_HARDWARE_THREAD;

    if (bitfield.Test(5))
        characteristics |= proto::DmiProcessors::CHARACTERISTIC_EXECUTE_PROTECTION;

    if (bitfield.Test(6))
        characteristics |= proto::DmiProcessors::CHARACTERISTIC_ENHANCED_VIRTUALIZATION;

    if (bitfield.Test(7))
        characteristics |= proto::DmiProcessors::CHARACTERISTIC_POWER_CONTROL;

    return characteristics;
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

proto::DmiCaches::Location SMBios::CacheTable::GetLocation() const
{
    switch (BitSet<uint16_t>(reader_.GetWord(0x05)).Range(5, 6))
    {
        case 0x00: return proto::DmiCaches::LOCATION_INTERNAL;
        case 0x01: return proto::DmiCaches::LOCATION_EXTERNAL;
        case 0x02: return proto::DmiCaches::LOCATION_RESERVED;
        default: return proto::DmiCaches::LOCATION_UNKNOWN;
    }
}

proto::DmiCaches::Status SMBios::CacheTable::GetStatus() const
{
    if (BitSet<uint16_t>(reader_.GetWord(0x05) & 0x0080).Test(7))
        return proto::DmiCaches::STATUS_ENABLED;

    return proto::DmiCaches::STATUS_DISABLED;
}

proto::DmiCaches::Mode SMBios::CacheTable::GetMode() const
{
    switch (BitSet<uint16_t>(reader_.GetWord(0x05)).Range(8, 9))
    {
        case 0x00: return proto::DmiCaches::MODE_WRITE_THRU;
        case 0x01: return proto::DmiCaches::MODE_WRITE_BACK;
        case 0x02: return proto::DmiCaches::MODE_WRITE_WITH_MEMORY_ADDRESS;
        default: return proto::DmiCaches::MODE_UNKNOWN;
    }
}

int SMBios::CacheTable::GetLevel() const
{
    return BitSet<uint16_t>(reader_.GetWord(0x05)).Range(0, 2) + 1;
}

int SMBios::CacheTable::GetMaximumSize() const
{
    return BitSet<uint16_t>(reader_.GetWord(0x07)).Range(0, 14);
}

int SMBios::CacheTable::GetCurrentSize() const
{
    return BitSet<uint16_t>(reader_.GetWord(0x09)).Range(0, 14);
}

uint32_t SMBios::CacheTable::GetSupportedSRAMTypes() const
{
    BitSet<uint16_t> bitfield(reader_.GetWord(0x0B));

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

proto::DmiCaches::SRAMType SMBios::CacheTable::GetCurrentSRAMType() const
{
    BitSet<uint16_t> type(reader_.GetWord(0x0D));

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

int SMBios::CacheTable::GetSpeed() const
{
    if (reader_.GetTableLength() < 0x13)
        return 0;

    return reader_.GetByte(0x0F);
}

proto::DmiCaches::ErrorCorrectionType SMBios::CacheTable::GetErrorCorrectionType() const
{
    if (reader_.GetTableLength() < 0x13)
        return proto::DmiCaches::ERROR_CORRECTION_TYPE_UNKNOWN;

    switch (reader_.GetByte(0x10))
    {
        case 0x01: return proto::DmiCaches::ERROR_CORRECTION_TYPE_OTHER;
        case 0x03: return proto::DmiCaches::ERROR_CORRECTION_TYPE_NONE;
        case 0x04: return proto::DmiCaches::ERROR_CORRECTION_TYPE_PARITY;
        case 0x05: return proto::DmiCaches::ERROR_CORRECTION_TYPE_SINGLE_BIT_ECC;
        case 0x06: return proto::DmiCaches::ERROR_CORRECTION_TYPE_MULTI_BIT_ECC;
        default: return proto::DmiCaches::ERROR_CORRECTION_TYPE_UNKNOWN;
    }
}

proto::DmiCaches::Type SMBios::CacheTable::GetType() const
{
    if (reader_.GetTableLength() < 0x13)
        return proto::DmiCaches::TYPE_UNKNOWN;

    switch (reader_.GetByte(0x11))
    {
        case 0x01: return proto::DmiCaches::TYPE_OTHER;
        case 0x03: return proto::DmiCaches::TYPE_INSTRUCTION;
        case 0x04: return proto::DmiCaches::TYPE_DATA;
        case 0x05: return proto::DmiCaches::TYPE_UNIFIED;
        default: return proto::DmiCaches::TYPE_UNKNOWN;
    }
}

proto::DmiCaches::Associativity SMBios::CacheTable::GetAssociativity() const
{
    if (reader_.GetTableLength() < 0x13)
        return proto::DmiCaches::ASSOCIATIVITY_UNKNOWN;

    switch (reader_.GetByte(0x12))
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
        { 0x21, "IEEE 1394" },
        { 0x22, "SAS/SATA Plug Receptacle" },
        { 0xA0, "PC-98" },
        { 0xA1, "PC-98 Hireso" },
        { 0xA2, "PC-H98" },
        { 0xA3, "PC-98 Note" },
        { 0xA4, "PC-98 Full" },
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

proto::DmiSystemSlots::Type SMBios::SystemSlotTable::GetType() const
{
    if (reader_.GetTableLength() < 0x0C)
        return proto::DmiSystemSlots::TYPE_UNKNOWN;

    switch (reader_.GetByte(0x05))
    {
        case 0x01: return proto::DmiSystemSlots::TYPE_OTHER;
        case 0x02: return proto::DmiSystemSlots::TYPE_UNKNOWN;
        case 0x03: return proto::DmiSystemSlots::TYPE_ISA;
        case 0x04: return proto::DmiSystemSlots::TYPE_MCA;
        case 0x05: return proto::DmiSystemSlots::TYPE_EISA;
        case 0x06: return proto::DmiSystemSlots::TYPE_PCI;
        case 0x07: return proto::DmiSystemSlots::TYPE_PC_CARD;
        case 0x08: return proto::DmiSystemSlots::TYPE_VLB;
        case 0x09: return proto::DmiSystemSlots::TYPE_PROPRIETARY;
        case 0x0A: return proto::DmiSystemSlots::TYPE_PROCESSOR_CARD;
        case 0x0B: return proto::DmiSystemSlots::TYPE_PROPRIETARY_MEMORY_CARD;
        case 0x0C: return proto::DmiSystemSlots::TYPE_IO_RISER_CARD;
        case 0x0D: return proto::DmiSystemSlots::TYPE_NUBUS;
        case 0x0E: return proto::DmiSystemSlots::TYPE_PCI_66;
        case 0x0F: return proto::DmiSystemSlots::TYPE_AGP;
        case 0x10: return proto::DmiSystemSlots::TYPE_AGP_2X;
        case 0x11: return proto::DmiSystemSlots::TYPE_AGP_4X;
        case 0x12: return proto::DmiSystemSlots::TYPE_PCI_X;
        case 0x13: return proto::DmiSystemSlots::TYPE_AGP_8X;
        case 0x14: return proto::DmiSystemSlots::TYPE_M2_SOCKET_1DP;
        case 0x15: return proto::DmiSystemSlots::TYPE_M2_SOCKET_1SD;
        case 0x16: return proto::DmiSystemSlots::TYPE_M2_SOCKET_2;
        case 0x17: return proto::DmiSystemSlots::TYPE_M2_SOCKET_3;
        case 0x18: return proto::DmiSystemSlots::TYPE_MXM_TYPE_I;
        case 0x19: return proto::DmiSystemSlots::TYPE_MXM_TYPE_II;
        case 0x1A: return proto::DmiSystemSlots::TYPE_MXM_TYPE_III;
        case 0x1B: return proto::DmiSystemSlots::TYPE_MXM_TYPE_III_HE;
        case 0x1C: return proto::DmiSystemSlots::TYPE_MXM_TYPE_IV;
        case 0x1D: return proto::DmiSystemSlots::TYPE_MXM_30_TYPE_A;
        case 0x1E: return proto::DmiSystemSlots::TYPE_MXM_30_TYPE_B;
        case 0x1F: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_SFF_8639;
        case 0x20: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_SFF_8639;
        case 0x21: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_MINI_52PIN_WITH_BOTTOM_SIDE;
        case 0x22: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_MINI_52PIN;
        case 0x23: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_MINI_76PIN;

        case 0xA0: return proto::DmiSystemSlots::TYPE_PC98_C20;
        case 0xA1: return proto::DmiSystemSlots::TYPE_PC98_C24;
        case 0xA2: return proto::DmiSystemSlots::TYPE_PC98_E;
        case 0xA3: return proto::DmiSystemSlots::TYPE_PC98_LOCAL_BUS;
        case 0xA4: return proto::DmiSystemSlots::TYPE_PC98_CARD;
        case 0xA5: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS;
        case 0xA6: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X1;
        case 0xA7: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X2;
        case 0xA8: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X4;
        case 0xA9: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X8;
        case 0xAA: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X16;
        case 0xAB: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2;
        case 0xAC: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X1;
        case 0xAD: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X2;
        case 0xAE: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X4;
        case 0xAF: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X8;
        case 0xB0: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X16;
        case 0xB1: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3;
        case 0xB2: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X1;
        case 0xB3: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X2;
        case 0xB4: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X4;
        case 0xB5: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X8;
        case 0xB6: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X16;

        default: return proto::DmiSystemSlots::TYPE_UNKNOWN;
    }
}

proto::DmiSystemSlots::Usage SMBios::SystemSlotTable::GetUsage() const
{
    if (reader_.GetTableLength() < 0x0C)
        return proto::DmiSystemSlots::USAGE_UNKNOWN;

    switch (reader_.GetByte(0x07))
    {
        case 0x01: return proto::DmiSystemSlots::USAGE_OTHER;
        case 0x02: return proto::DmiSystemSlots::USAGE_UNKNOWN;
        case 0x03: return proto::DmiSystemSlots::USAGE_AVAILABLE;
        case 0x04: return proto::DmiSystemSlots::USAGE_IN_USE;
        default: return proto::DmiSystemSlots::USAGE_UNKNOWN;
    }
}

proto::DmiSystemSlots::BusWidth SMBios::SystemSlotTable::GetBusWidth() const
{
    if (reader_.GetTableLength() < 0x0C)
        return proto::DmiSystemSlots::BUS_WIDTH_UNKNOWN;

    switch (reader_.GetByte(0x06))
    {
        case 0x01: return proto::DmiSystemSlots::BUS_WIDTH_OTHER;
        case 0x02: return proto::DmiSystemSlots::BUS_WIDTH_UNKNOWN;
        case 0x03: return proto::DmiSystemSlots::BUS_WIDTH_8_BIT;
        case 0x04: return proto::DmiSystemSlots::BUS_WIDTH_16_BIT;
        case 0x05: return proto::DmiSystemSlots::BUS_WIDTH_32_BIT;
        case 0x06: return proto::DmiSystemSlots::BUS_WIDTH_64_BIT;
        case 0x07: return proto::DmiSystemSlots::BUS_WIDTH_128_BIT;
        case 0x08: return proto::DmiSystemSlots::BUS_WIDTH_X1;
        case 0x09: return proto::DmiSystemSlots::BUS_WIDTH_X2;
        case 0x0A: return proto::DmiSystemSlots::BUS_WIDTH_X4;
        case 0x0B: return proto::DmiSystemSlots::BUS_WIDTH_X8;
        case 0x0C: return proto::DmiSystemSlots::BUS_WIDTH_X12;
        case 0x0D: return proto::DmiSystemSlots::BUS_WIDTH_X16;
        case 0x0E: return proto::DmiSystemSlots::BUS_WIDTH_X32;
        default: return proto::DmiSystemSlots::BUS_WIDTH_OTHER;
    }
}

proto::DmiSystemSlots::Length SMBios::SystemSlotTable::GetLength() const
{
    if (reader_.GetTableLength() < 0x0C)
        return proto::DmiSystemSlots::LENGTH_UNKNOWN;

    switch (reader_.GetByte(0x08))
    {
        case 0x01: return proto::DmiSystemSlots::LENGTH_OTHER;
        case 0x02: return proto::DmiSystemSlots::LENGTH_UNKNOWN;
        case 0x03: return proto::DmiSystemSlots::LENGTH_SHORT;
        case 0x04: return proto::DmiSystemSlots::LENGTH_LONG;
        default: return proto::DmiSystemSlots::LENGTH_UNKNOWN;
    }
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

    std::string output;
    TrimWhitespaceASCII(string, TRIM_ALL, output);
    return output;
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

//
// PortableBatteryTable
//

SMBios::PortableBatteryTable::PortableBatteryTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::PortableBatteryTable::GetLocation() const
{
    if (reader_.GetTableLength() < 0x10)
        return std::string();

    return reader_.GetString(0x04);
}

std::string SMBios::PortableBatteryTable::GetManufacturer() const
{
    if (reader_.GetTableLength() < 0x10)
        return std::string();

    return reader_.GetString(0x05);
}

std::string SMBios::PortableBatteryTable::GetManufactureDate() const
{
    if (reader_.GetTableLength() < 0x10)
        return std::string();

    return reader_.GetString(0x06);
}

std::string SMBios::PortableBatteryTable::GetSerialNumber() const
{
    if (reader_.GetTableLength() < 0x10)
        return std::string();

    return reader_.GetString(0x07);
}

std::string SMBios::PortableBatteryTable::GetDeviceName() const
{
    if (reader_.GetTableLength() < 0x10)
        return std::string();

    return reader_.GetString(0x08);
}

std::string SMBios::PortableBatteryTable::GetChemistry() const
{
    if (reader_.GetTableLength() < 0x10)
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

    const uint8_t value = reader_.GetByte(0x09);

    if (value >= 0x01 && value <= 0x09 && names[value - 0x01])
        return names[value - 0x01];

    return std::string();
}

int SMBios::PortableBatteryTable::GetDesignCapacity() const
{
    const uint8_t length = reader_.GetTableLength();
    if (length < 0x10)
        return 0;

    if (length < 0x16)
        return reader_.GetWord(0x0A);

    return reader_.GetWord(0x0A) + reader_.GetByte(0x15);
}

int SMBios::PortableBatteryTable::GetDesignVoltage() const
{
    if (reader_.GetTableLength() < 0x10)
        return 0;

    return reader_.GetWord(0x0C);
}

std::string SMBios::PortableBatteryTable::GetSBDSVersionNumber() const
{
    if (reader_.GetTableLength() < 0x10)
        return std::string();

    return reader_.GetString(0x0E);
}

int SMBios::PortableBatteryTable::GetMaxErrorInBatteryData() const
{
    if (reader_.GetTableLength() < 0x10)
        return 0;

    return reader_.GetByte(0x0F);
}

std::string SMBios::PortableBatteryTable::GetSBDSSerialNumber() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    return StringPrintf("%04X", reader_.GetWord(0x10));
}

std::string SMBios::PortableBatteryTable::GetSBDSManufactureDate() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    const BitSet<uint16_t> date(reader_.GetWord(0x12));

    return StringPrintf("%02u/%02u/%u",
                        date.Range(0, 4), // Day.
                        date.Range(5, 8), // Month.
                        1980U + date.Range(9, 15)); // Year.
}

std::string SMBios::PortableBatteryTable::GetSBDSDeviceChemistry() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    return reader_.GetString(0x14);
}

} // namespace aspia
