//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <cstddef>
#include <cstring>

//--------------------------------------------------------------------------------------------------
SmbiosTableEnumerator::SmbiosTableEnumerator(const QByteArray& smbios_dump)
{
    // Zeroed even when the dump is rejected below, so that the version and length getters stay
    // deterministic and the slack after a short dump reads as zeros.
    memset(&smbios_, 0, sizeof(SmbiosDump));

    const qsizetype header_size = static_cast<qsizetype>(offsetof(SmbiosDump, smbios_table_data));

    if (smbios_dump.size() < header_size ||
        smbios_dump.size() > static_cast<qsizetype>(sizeof(SmbiosDump)))
        return;

    memcpy(&smbios_, smbios_dump.data(), smbios_dump.size());

    // The length field cannot be trusted: a corrupted dump may declare more data than it
    // actually carries. Clamp it so that the enumeration never extends past the real data.
    const quint32 available = static_cast<quint32>(smbios_dump.size() - header_size);
    if (smbios_.length > available)
        smbios_.length = available;

    start_ = &smbios_.smbios_table_data[0];
    end_ = start_ + smbios_.length;
    pos_ = start_;

    validate();
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
    return pos_ >= end_;
}

//--------------------------------------------------------------------------------------------------
void SmbiosTableEnumerator::advance()
{
    if (isAtEnd())
        return;

    pos_ = next_;
    validate();
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosTableEnumerator::majorVersion() const
{
    return smbios_.smbios_major_version;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosTableEnumerator::minorVersion() const
{
    return smbios_.smbios_minor_version;
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosTableEnumerator::length() const
{
    return smbios_.length;
}

//--------------------------------------------------------------------------------------------------
void SmbiosTableEnumerator::validate()
{
    // The table at |pos_| is exposed through table() only if it is fully contained in the
    // buffer: the header, the formatted area declared by header->length and the double null
    // byte terminating the string area. The string walk in smbiosString and the field accesses
    // in the table wrappers rely on this invariant to stay within bounds. A table that fails
    // the check also makes locating the next table unreliable, so enumeration stops at it.
    if (!pos_ || pos_ >= end_)
        return;

    if (end_ - pos_ < static_cast<qptrdiff>(sizeof(SmbiosTable)))
    {
        pos_ = end_;
        return;
    }

    const SmbiosTable* header = reinterpret_cast<const SmbiosTable*>(pos_);

    if (header->length < sizeof(SmbiosTable) || header->length > end_ - pos_ ||
        header->type == SMBIOS_TABLE_TYPE_END_OF_TABLE)
    {
        pos_ = end_;
        return;
    }

    quint8* p = pos_ + header->length;

    while (p + 1 < end_ && (p[0] || p[1]))
        ++p;

    if (p + 1 >= end_)
    {
        // No double null before the end of the buffer: the string area is truncated.
        pos_ = end_;
        return;
    }

    // The next table starts after the two null bytes at the end of the strings.
    next_ = p + 2;
}

//--------------------------------------------------------------------------------------------------
QString smbiosString(const SmbiosTable* table, quint8 number)
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
bool SmbiosBios::isValid() const
{
    // 12h is the minimum length of the BIOS information table since SMBIOS 2.0.
    return table_->length >= 0x12;
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
    // A module size of zero means an empty socket. 0xFFFF means the size is unknown, but the
    // device itself is present.
    return table_->module_size != 0;
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
    if (table_->module_size == 0x7FFF)
    {
        // The actual size is stored in the extended field (2.7+). A table too short to carry
        // the field cannot tell the size.
        if (table_->length < 0x20)
            return 0;

        quint32 ext_size = table_->ext_size & 0x7FFFFFFFUL;

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

    if (table_->module_size == 0xFFFF)
    {
        // The device is present, but its size is unknown (see isPresent).
        return 0;
    }

    if (table_->module_size & 0x8000)
    {
        // Size in kB. Convert to bytes and return.
        return static_cast<quint64>(table_->module_size & 0x7FFF) * 1024ULL;
    }

    // Size in MB. Convert to bytes and return.
    return static_cast<quint64>(table_->module_size) * 1024ULL * 1024ULL;
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
quint32 SmbiosMemoryDevice::speed() const
{
    if (table_->length < 0x17)
        return 0;

    return table_->speed;
}
