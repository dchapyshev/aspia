//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/smbios_parser.h"

#include <cstring>

namespace base {

//--------------------------------------------------------------------------------------------------
SmbiosTableEnumerator::SmbiosTableEnumerator(const QByteArray& smbios_dump)
{
    if (smbios_dump.isEmpty())
        return;

    if (smbios_dump.size() > sizeof(SmbiosDump))
        return;

    memset(&smbios_, 0, sizeof(SmbiosDump));
    memcpy(&smbios_, smbios_dump.data(), smbios_dump.size());

    if (smbios_.length > kSmbiosMaxDataSize)
        return;

    start_ = &smbios_.smbios_table_data[0];
    end_ = start_ + smbios_.length;
    pos_ = start_;
}

//--------------------------------------------------------------------------------------------------
SmbiosTableEnumerator::~SmbiosTableEnumerator() = default;

//--------------------------------------------------------------------------------------------------
const SmbiosTable* SmbiosTableEnumerator::table() const
{
    return reinterpret_cast<const SmbiosTable*>(pos_);
}

//--------------------------------------------------------------------------------------------------
bool SmbiosTableEnumerator::isAtEnd() const
{
    return pos_ + sizeof(SmbiosTable) >= end_;
}

//--------------------------------------------------------------------------------------------------
void SmbiosTableEnumerator::advance()
{
    SmbiosTable* header = reinterpret_cast<SmbiosTable*>(pos_);

    if (header->length < sizeof(SmbiosTable) || header->type == SMBIOS_TABLE_TYPE_END_OF_TABLE)
    {
        // If a short entry is found (less than 4 bytes), not only it is invalid, but we
        // cannot reliably locate the next entry. Better stop at this point, and let the
        // user know his / her table is broken.
        pos_ = end_;
        return;
    }

    pos_ += header->length;

    while (static_cast<uint32_t>(pos_ - start_ + 1) < smbios_.length && (pos_[0] || pos_[1]))
        ++pos_;

    // Points to the next table thas after two null bytes at the end of the strings.
    pos_ += 2;
}

//--------------------------------------------------------------------------------------------------
uint8_t SmbiosTableEnumerator::majorVersion() const
{
    return smbios_.smbios_major_version;
}

//--------------------------------------------------------------------------------------------------
uint8_t SmbiosTableEnumerator::minorVersion() const
{
    return smbios_.smbios_minor_version;
}

//--------------------------------------------------------------------------------------------------
uint32_t SmbiosTableEnumerator::length() const
{
    return smbios_.length;
}

//--------------------------------------------------------------------------------------------------
QString smbiosString(const SmbiosTable* table, uint8_t number)
{
    if (!number)
        return QString();

    const char* string = reinterpret_cast<const char*>(table) + table->length;

    while (number > 1 && *string)
    {
        string += strlen(string) + 1;
        --number;
    }

    return QString::fromLatin1(string).trimmed();
}

//--------------------------------------------------------------------------------------------------
SmbiosBios::SmbiosBios(const SmbiosTable* table)
    : table_(static_cast<const SmbiosBiosTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QString SmbiosBios::vendor() const
{
    return smbiosString(table_, table_->vendor);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosBios::version() const
{
    return smbiosString(table_, table_->version);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosBios::releaseDate() const
{
    return smbiosString(table_, table_->release_date);
}

//--------------------------------------------------------------------------------------------------
SmbiosBaseboard::SmbiosBaseboard(const SmbiosTable* table)
    : table_(static_cast<const SmbiosBaseboardTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosBaseboard::isValid() const
{
    return table_->length >= 0x08;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosBaseboard::manufacturer() const
{
    return smbiosString(table_, table_->manufactorer);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosBaseboard::product() const
{
    return smbiosString(table_, table_->product);
}

//--------------------------------------------------------------------------------------------------
SmbiosMemoryDevice::SmbiosMemoryDevice(const SmbiosTable* table)
    : table_(static_cast<const SmbiosMemoryDeviceTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosMemoryDevice::isValid() const
{
    return table_->length >= 0x15;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosMemoryDevice::isPresent() const
{
    return size() != 0;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosMemoryDevice::location() const
{
    return smbiosString(table_, table_->device_location);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosMemoryDevice::manufacturer() const
{
    if (table_->length < 0x1B)
        return QString();

    static const char* kBlackList[] = { "0000" };

    QString result = smbiosString(table_, table_->manufacturer);

    for (size_t i = 0; i < std::size(kBlackList); ++i)
    {
        if (result == kBlackList[i])
            return QString();
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryDevice::size() const
{
    if (table_->length >= 0x20 && table_->module_size == 0x7FFF)
    {
        uint32_t ext_size = table_->ext_size & 0x7FFFFFFFUL;

        if (ext_size & 0x3FFUL)
        {
            // Size in MB. Convert to bytes and return.
            return static_cast<quint64>(ext_size) * 1024ULL * 1024ULL;
        }
        else if (ext_size & 0xFFC00UL)
        {
            // Size in GB. Convert to bytes and return.
            return static_cast<quint64>(ext_size >> 10) * 1024ULL * 1024ULL * 1024ULL;
        }
        else
        {
            // Size in TB. Convert to bytes and return.
            return static_cast<quint64>(ext_size >> 20) * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
        }
    }
    else
    {
        if (table_->module_size == 0xFFFF)
        {
            // No installed memory device in the socket.
            return 0;
        }

        if (table_->module_size & 0x8000)
        {
            // Size in kB. Convert to bytes and return.
            return static_cast<quint64>(table_->module_size & 0x7FFF) * 1024ULL;
        }
        else
        {
            // Size in MB. Convert to bytes and return.
            return static_cast<quint64>(table_->module_size) * 1024ULL * 1024ULL;
        }
    }
}

//--------------------------------------------------------------------------------------------------
QString SmbiosMemoryDevice::type() const
{
    static const char* kType[] =
    {
        "Other", // 0x01
        "Unknown",
        "DRAM",
        "EDRAM",
        "VRAM",
        "SRAM",
        "RAM",
        "ROM",
        "Flash",
        "EEPROM",
        "FEPROM",
        "EPROM",
        "CDRAM",
        "3DRAM",
        "SDRAM",
        "SGRAM",
        "RDRAM",
        "DDR",
        "DDR2",
        "DDR2 FB-DIMM",
        "Reserved",
        "Reserved",
        "Reserved",
        "DDR3",
        "FBD2",
        "DDR4",
        "LPDDR",
        "LPDDR2",
        "LPDDR3",
        "LPDDR4",
        "Logical non-volatile device",
        "HBM (High Bandwidth Memory)",
        "HBM2 (High Bandwidth Memory Generation 2)",
        "DDR5",
        "LPDDR5",
        "HBM3 (High Bandwidth Memory Generation 3)" // 0x24
    };

    if (table_->memory_type >= 0x01 && table_->memory_type <= 0x24)
        return kType[table_->memory_type - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosMemoryDevice::formFactor() const
{
    static const char* kFormFactor[] =
    {
        "Other", // 0x01
        "Unknown",
        "SIMM",
        "SIP",
        "Chip",
        "DIP",
        "ZIP",
        "Proprietary Card",
        "DIMM",
        "TSOP",
        "Row Of Chips",
        "RIMM",
        "SODIMM",
        "SRIMM",
        "FB-DIMM" // 0x0F
    };

    if (table_->form_factor >= 0x01 && table_->form_factor <= 0x0F)
        return kFormFactor[table_->form_factor - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosMemoryDevice::partNumber() const
{
    if (table_->length < 0x1B)
        return QString();

    static const char* kBlackList[] = { "[Empty]" };

    QString result = smbiosString(table_, table_->part_number);

    for (size_t i = 0; i < std::size(kBlackList); ++i)
    {
        if (result == kBlackList[i])
            return QString();
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
uint32_t SmbiosMemoryDevice::speed() const
{
    if (table_->length < 0x17)
        return 0;

    return table_->speed;
}

} // namespace base
