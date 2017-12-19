//
// PROJECT:         Aspia
// FILE:            base/devices/smbios.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios.h"
#include "base/strings/string_printf.h"
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

proto::DmiOnBoardDevices::Type SMBios::OnBoardDeviceTable::GetType(int index) const
{
    DCHECK(index < count_);

    switch (BitSet<uint8_t>(ptr_[2 * index]).Range(0, 6))
    {
        case 0x01: return proto::DmiOnBoardDevices::TYPE_OTHER;
        case 0x02: return proto::DmiOnBoardDevices::TYPE_UNKNOWN;
        case 0x03: return proto::DmiOnBoardDevices::TYPE_VIDEO;
        case 0x04: return proto::DmiOnBoardDevices::TYPE_SCSI_CONTROLLER;
        case 0x05: return proto::DmiOnBoardDevices::TYPE_ETHERNET;
        case 0x06: return proto::DmiOnBoardDevices::TYPE_TOKEN_RING;
        case 0x07: return proto::DmiOnBoardDevices::TYPE_SOUND;
        case 0x08: return proto::DmiOnBoardDevices::TYPE_PATA_CONTROLLER;
        case 0x09: return proto::DmiOnBoardDevices::TYPE_SATA_CONTROLLER;
        case 0x0A: return proto::DmiOnBoardDevices::TYPE_SAS_CONTROLLER;
        default: return proto::DmiOnBoardDevices::TYPE_UNKNOWN;
    }
}

bool SMBios::OnBoardDeviceTable::IsEnabled(int index) const
{
    DCHECK(index < count_);
    return BitSet<uint8_t>(ptr_[2 * index]).Test(7);
}

//
// PointingDeviceTable
//

SMBios::PointingDeviceTable::PointingDeviceTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

proto::DmiPointingDevices::Type SMBios::PointingDeviceTable::GetDeviceType() const
{
    if (reader_.GetTableLength() < 0x07)
        return proto::DmiPointingDevices::TYPE_UNKNOWN;

    switch (reader_.GetByte(0x04))
    {
        case 0x01: return proto::DmiPointingDevices::TYPE_OTHER;
        case 0x02: return proto::DmiPointingDevices::TYPE_UNKNOWN;
        case 0x03: return proto::DmiPointingDevices::TYPE_MOUSE;
        case 0x04: return proto::DmiPointingDevices::TYPE_TRACK_BALL;
        case 0x05: return proto::DmiPointingDevices::TYPE_TRACK_POINT;
        case 0x06: return proto::DmiPointingDevices::TYPE_GLIDE_POINT;
        case 0x07: return proto::DmiPointingDevices::TYPE_TOUCH_PAD;
        case 0x08: return proto::DmiPointingDevices::TYPE_TOUCH_SCREEN;
        case 0x09: return proto::DmiPointingDevices::TYPE_OPTICAL_SENSOR;
        default: return proto::DmiPointingDevices::TYPE_UNKNOWN;
    }
}

proto::DmiPointingDevices::Interface SMBios::PointingDeviceTable::GetInterface() const
{
    if (reader_.GetTableLength() < 0x07)
        return proto::DmiPointingDevices::INTERFACE_UNKNOWN;

    switch (reader_.GetByte(0x05))
    {
        case 0x01: return proto::DmiPointingDevices::INTERFACE_OTHER;
        case 0x02: return proto::DmiPointingDevices::INTERFACE_UNKNOWN;
        case 0x03: return proto::DmiPointingDevices::INTERFACE_SERIAL;
        case 0x04: return proto::DmiPointingDevices::INTERFACE_PS_2;
        case 0x05: return proto::DmiPointingDevices::INTERFACE_INFRARED;
        case 0x06: return proto::DmiPointingDevices::INTERFACE_HP_HIL;
        case 0x07: return proto::DmiPointingDevices::INTERFACE_BUS_MOUSE;
        case 0x08: return proto::DmiPointingDevices::INTERFACE_ADB;
        case 0xA0: return proto::DmiPointingDevices::INTERFACE_BUS_MOUSE_DB_9;
        case 0xA1: return proto::DmiPointingDevices::INTERFACE_BUS_MOUSE_MICRO_DIN;
        case 0xA2: return proto::DmiPointingDevices::INTERFACE_USB;
        default: return proto::DmiPointingDevices::INTERFACE_UNKNOWN;
    }
}

int SMBios::PointingDeviceTable::GetButtonCount() const
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

proto::DmiPortableBattery::Chemistry SMBios::PortableBatteryTable::GetChemistry() const
{
    if (reader_.GetTableLength() < 0x10)
        return proto::DmiPortableBattery::CHEMISTRY_UNKNOWN;

    switch (reader_.GetByte(0x09))
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
