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

namespace {

//--------------------------------------------------------------------------------------------------
// The boot-up, the power supply and the thermal state of the chassis share the same value list.
QString chassisState(quint8 state)
{
    static const char* kState[] =
    {
        "Other", // 0x01
        "Unknown",
        "Safe",
        "Warning",
        "Critical",
        "Non-recoverable" // 0x06
    };

    if (state >= 0x01 && state <= 0x06)
        return kState[state - 0x01];

    return QString();
}

} // namespace

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
SmbiosSystem::SmbiosSystem(const SmbiosTable* table)
    : table_(static_cast<const SmbiosSystemTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosSystem::isValid() const
{
    // 08h is the minimum length of the system information table since SMBIOS 2.0.
    return table_->length >= 0x08;
}

//--------------------------------------------------------------------------------------------------
QByteArray SmbiosSystem::uuid() const
{
    // The UUID field appeared in SMBIOS 2.1.
    if (table_->length < 0x19)
        return QByteArray();

    // All 0x00 means the ID is not set; all 0xFF means it is set but unknown.
    bool all_zeros = true;
    bool all_ones = true;

    for (size_t i = 0; i < sizeof(table_->uuid); ++i)
    {
        if (table_->uuid[i] != 0x00)
            all_zeros = false;
        if (table_->uuid[i] != 0xFF)
            all_ones = false;
    }

    if (all_zeros || all_ones)
        return QByteArray();

    return QByteArray(reinterpret_cast<const char*>(table_->uuid), sizeof(table_->uuid));
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
SmbiosChassis::SmbiosChassis(const SmbiosTable* table)
    : table_(static_cast<const SmbiosChassisTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosChassis::isValid() const
{
    // 09h is the minimum length of the system enclosure table since SMBIOS 2.0.
    return table_->length >= 0x09;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosChassis::manufacturer() const
{
    return smbiosString(table_, table_->manufacturer);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosChassis::version() const
{
    return smbiosString(table_, table_->version);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosChassis::serialNumber() const
{
    return smbiosString(table_, table_->serial_number);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosChassis::assetTag() const
{
    return smbiosString(table_, table_->asset_tag);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosChassis::skuNumber() const
{
    // The SKU number (2.7+) is stored behind the list of contained elements, so its offset
    // depends on the number and the size of the elements declared by the table.
    if (table_->length < 0x15)
        return QString();

    const quint32 offset = 0x15 + static_cast<quint32>(table_->element_count) *
                                  static_cast<quint32>(table_->element_length);
    if (offset >= table_->length)
        return QString();

    return smbiosString(table_, *(reinterpret_cast<const quint8*>(table_) + offset));
}

//--------------------------------------------------------------------------------------------------
QString SmbiosChassis::type() const
{
    static const char* kType[] =
    {
        "Other", // 0x01
        "Unknown",
        "Desktop",
        "Low Profile Desktop",
        "Pizza Box",
        "Mini Tower",
        "Tower",
        "Portable",
        "Laptop",
        "Notebook",
        "Hand Held",
        "Docking Station",
        "All In One",
        "Sub Notebook",
        "Space-saving",
        "Lunch Box",
        "Main Server Chassis",
        "Expansion Chassis",
        "Sub Chassis",
        "Bus Expansion Chassis",
        "Peripheral Chassis",
        "RAID Chassis",
        "Rack Mount Chassis",
        "Sealed-case PC",
        "Multi-system Chassis",
        "Compact PCI",
        "Advanced TCA",
        "Blade",
        "Blade Enclosure",
        "Tablet",
        "Convertible",
        "Detachable",
        "IoT Gateway",
        "Embedded PC",
        "Mini PC",
        "Stick PC" // 0x24
    };

    // Bit 7 carries the chassis lock presence, the type itself is in the remaining bits.
    const quint8 type = table_->type & 0x7F;

    if (type >= 0x01 && type <= 0x24)
        return kType[type - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
bool SmbiosChassis::isLockPresent() const
{
    return (table_->type & 0x80) != 0;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosChassis::bootUpState() const
{
    if (table_->length < 0x0A)
        return QString();

    return chassisState(table_->boot_up_state);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosChassis::powerSupplyState() const
{
    if (table_->length < 0x0B)
        return QString();

    return chassisState(table_->power_supply_state);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosChassis::thermalState() const
{
    if (table_->length < 0x0C)
        return QString();

    return chassisState(table_->thermal_state);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosChassis::securityStatus() const
{
    static const char* kStatus[] =
    {
        "Other", // 0x01
        "Unknown",
        "None",
        "External Interface Locked Out",
        "External Interface Enabled" // 0x05
    };

    if (table_->length < 0x0D)
        return QString();

    if (table_->security_status >= 0x01 && table_->security_status <= 0x05)
        return kStatus[table_->security_status - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosChassis::height() const
{
    // The height is measured in 'U' units. Zero means the value is unspecified.
    if (table_->length < 0x12)
        return 0;

    return table_->height;
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosChassis::powerCords() const
{
    // Zero means the number of power cords is unspecified.
    if (table_->length < 0x13)
        return 0;

    return table_->power_cords;
}

//--------------------------------------------------------------------------------------------------
SmbiosProcessor::SmbiosProcessor(const SmbiosTable* table)
    : table_(static_cast<const SmbiosProcessorTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosProcessor::isValid() const
{
    // 1Ah is the minimum length of the processor information table since SMBIOS 2.0.
    return table_->length >= 0x1A;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosProcessor::isPopulated() const
{
    // Bit 6 of the status field tells whether the socket carries a processor.
    return (table_->status & 0x40) != 0;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosProcessor::manufacturer() const
{
    return smbiosString(table_, table_->manufacturer);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosProcessor::version() const
{
    return smbiosString(table_, table_->version);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosProcessor::serialNumber() const
{
    if (table_->length < 0x21)
        return QString();

    return smbiosString(table_, table_->serial_number);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosProcessor::assetTag() const
{
    if (table_->length < 0x22)
        return QString();

    return smbiosString(table_, table_->asset_tag);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosProcessor::partNumber() const
{
    if (table_->length < 0x23)
        return QString();

    return smbiosString(table_, table_->part_number);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosProcessor::socketDesignation() const
{
    return smbiosString(table_, table_->socket_designation);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosProcessor::type() const
{
    static const char* kType[] =
    {
        "Other", // 0x01
        "Unknown",
        "Central Processor",
        "Math Processor",
        "DSP Processor",
        "Video Processor" // 0x06
    };

    if (table_->type >= 0x01 && table_->type <= 0x06)
        return kType[table_->type - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosProcessor::family() const
{
    static const struct
    {
        quint16 value;
        const char* name;
    } kFamily[] =
    {
        { 0x0001, "Other" },
        { 0x0002, "Unknown" },
        { 0x0003, "8086" },
        { 0x0004, "80286" },
        { 0x0005, "Intel386" },
        { 0x0006, "Intel486" },
        { 0x0007, "8087" },
        { 0x0008, "80287" },
        { 0x0009, "80387" },
        { 0x000A, "80487" },
        { 0x000B, "Intel Pentium" },
        { 0x000C, "Pentium Pro" },
        { 0x000D, "Pentium II" },
        { 0x000E, "Pentium MMX" },
        { 0x000F, "Intel Celeron" },
        { 0x0010, "Pentium II Xeon" },
        { 0x0011, "Pentium III" },
        { 0x0012, "M1 Family" },
        { 0x0013, "M2 Family" },
        { 0x0014, "Intel Celeron M" },
        { 0x0015, "Intel Pentium 4 HT" },
        { 0x0016, "Intel Processor" },
        { 0x0018, "AMD Duron" },
        { 0x0019, "K5 Family" },
        { 0x001A, "K6 Family" },
        { 0x001B, "K6-2" },
        { 0x001C, "K6-3" },
        { 0x001D, "AMD Athlon" },
        { 0x001E, "AMD29000 Family" },
        { 0x001F, "K6-2+" },
        { 0x0020, "Power PC Family" },
        { 0x0021, "Power PC 601" },
        { 0x0022, "Power PC 603" },
        { 0x0023, "Power PC 603+" },
        { 0x0024, "Power PC 604" },
        { 0x0025, "Power PC 620" },
        { 0x0026, "Power PC x704" },
        { 0x0027, "Power PC 750" },
        { 0x0028, "Intel Core Duo" },
        { 0x0029, "Intel Core Duo Mobile" },
        { 0x002A, "Intel Core Solo Mobile" },
        { 0x002B, "Intel Atom" },
        { 0x002C, "Intel Core M" },
        { 0x002D, "Intel Core m3" },
        { 0x002E, "Intel Core m5" },
        { 0x002F, "Intel Core m7" },
        { 0x0030, "Alpha Family" },
        { 0x0031, "Alpha 21064" },
        { 0x0032, "Alpha 21066" },
        { 0x0033, "Alpha 21164" },
        { 0x0034, "Alpha 21164PC" },
        { 0x0035, "Alpha 21164a" },
        { 0x0036, "Alpha 21264" },
        { 0x0037, "Alpha 21364" },
        { 0x0038, "AMD Turion II Ultra Dual-Core Mobile M" },
        { 0x0039, "AMD Turion II Dual-Core Mobile M" },
        { 0x003A, "AMD Athlon II Dual-Core M" },
        { 0x003B, "AMD Opteron 6100 Series" },
        { 0x003C, "AMD Opteron 4100 Series" },
        { 0x003D, "AMD Opteron 6200 Series" },
        { 0x003E, "AMD Opteron 4200 Series" },
        { 0x003F, "AMD FX Series" },
        { 0x0040, "MIPS Family" },
        { 0x0041, "MIPS R4000" },
        { 0x0042, "MIPS R4200" },
        { 0x0043, "MIPS R4400" },
        { 0x0044, "MIPS R4600" },
        { 0x0045, "MIPS R10000" },
        { 0x0046, "AMD C-Series" },
        { 0x0047, "AMD E-Series" },
        { 0x0048, "AMD A-Series" },
        { 0x0049, "AMD G-Series" },
        { 0x004A, "AMD Z-Series" },
        { 0x004B, "AMD R-Series" },
        { 0x004C, "AMD Opteron 4300 Series" },
        { 0x004D, "AMD Opteron 6300 Series" },
        { 0x004E, "AMD Opteron 3300 Series" },
        { 0x004F, "AMD FirePro Series" },
        { 0x0050, "SPARC Family" },
        { 0x0051, "SuperSPARC" },
        { 0x0052, "microSPARC II" },
        { 0x0053, "microSPARC IIep" },
        { 0x0054, "UltraSPARC" },
        { 0x0055, "UltraSPARC II" },
        { 0x0056, "UltraSPARC IIi" },
        { 0x0057, "UltraSPARC III" },
        { 0x0058, "UltraSPARC IIIi" },
        { 0x0060, "68040 Family" },
        { 0x0061, "68xxx" },
        { 0x0062, "68000" },
        { 0x0063, "68010" },
        { 0x0064, "68020" },
        { 0x0065, "68030" },
        { 0x0066, "AMD Athlon X4 Quad-Core" },
        { 0x0067, "AMD Opteron X1000 Series" },
        { 0x0068, "AMD Opteron X2000 Series APU" },
        { 0x0069, "AMD Opteron A-Series" },
        { 0x006A, "AMD Opteron X3000 Series APU" },
        { 0x006B, "AMD Zen Family" },
        { 0x0070, "Hobbit Family" },
        { 0x0078, "Crusoe TM5000 Family" },
        { 0x0079, "Crusoe TM3000 Family" },
        { 0x007A, "Efficeon TM8000 Family" },
        { 0x0080, "Weitek" },
        { 0x0082, "Intel Itanium" },
        { 0x0083, "AMD Athlon 64" },
        { 0x0084, "AMD Opteron" },
        { 0x0085, "AMD Sempron" },
        { 0x0086, "AMD Turion 64 Mobile" },
        { 0x0087, "Dual-Core AMD Opteron" },
        { 0x0088, "AMD Athlon 64 X2 Dual-Core" },
        { 0x0089, "AMD Turion 64 X2 Mobile" },
        { 0x008A, "Quad-Core AMD Opteron" },
        { 0x008B, "Third-Generation AMD Opteron" },
        { 0x008C, "AMD Phenom FX Quad-Core" },
        { 0x008D, "AMD Phenom X4 Quad-Core" },
        { 0x008E, "AMD Phenom X2 Dual-Core" },
        { 0x008F, "AMD Athlon X2 Dual-Core" },
        { 0x0090, "PA-RISC Family" },
        { 0x0091, "PA-RISC 8500" },
        { 0x0092, "PA-RISC 8000" },
        { 0x0093, "PA-RISC 7300LC" },
        { 0x0094, "PA-RISC 7200" },
        { 0x0095, "PA-RISC 7100LC" },
        { 0x0096, "PA-RISC 7100" },
        { 0x00A0, "V30 Family" },
        { 0x00A1, "Quad-Core Intel Xeon 3200 Series" },
        { 0x00A2, "Dual-Core Intel Xeon 3000 Series" },
        { 0x00A3, "Quad-Core Intel Xeon 5300 Series" },
        { 0x00A4, "Dual-Core Intel Xeon 5100 Series" },
        { 0x00A5, "Dual-Core Intel Xeon 5000 Series" },
        { 0x00A6, "Dual-Core Intel Xeon LV" },
        { 0x00A7, "Dual-Core Intel Xeon ULV" },
        { 0x00A8, "Dual-Core Intel Xeon 7100 Series" },
        { 0x00A9, "Quad-Core Intel Xeon 5400 Series" },
        { 0x00AA, "Quad-Core Intel Xeon" },
        { 0x00AB, "Dual-Core Intel Xeon 5200 Series" },
        { 0x00AC, "Dual-Core Intel Xeon 7200 Series" },
        { 0x00AD, "Quad-Core Intel Xeon 7300 Series" },
        { 0x00AE, "Quad-Core Intel Xeon 7400 Series" },
        { 0x00AF, "Multi-Core Intel Xeon 7400 Series" },
        { 0x00B0, "Pentium III Xeon" },
        { 0x00B1, "Pentium III with SpeedStep" },
        { 0x00B2, "Pentium 4" },
        { 0x00B3, "Intel Xeon" },
        { 0x00B4, "AS400 Family" },
        { 0x00B5, "Intel Xeon MP" },
        { 0x00B6, "AMD Athlon XP" },
        { 0x00B7, "AMD Athlon MP" },
        { 0x00B8, "Intel Itanium 2" },
        { 0x00B9, "Intel Pentium M" },
        { 0x00BA, "Intel Celeron D" },
        { 0x00BB, "Intel Pentium D" },
        { 0x00BC, "Intel Pentium Extreme Edition" },
        { 0x00BD, "Intel Core Solo" },
        { 0x00BF, "Intel Core 2 Duo" },
        { 0x00C0, "Intel Core 2 Solo" },
        { 0x00C1, "Intel Core 2 Extreme" },
        { 0x00C2, "Intel Core 2 Quad" },
        { 0x00C3, "Intel Core 2 Extreme Mobile" },
        { 0x00C4, "Intel Core 2 Duo Mobile" },
        { 0x00C5, "Intel Core 2 Solo Mobile" },
        { 0x00C6, "Intel Core i7" },
        { 0x00C7, "Dual-Core Intel Celeron" },
        { 0x00C8, "IBM390 Family" },
        { 0x00C9, "G4" },
        { 0x00CA, "G5" },
        { 0x00CB, "ESA/390 G6" },
        { 0x00CC, "z/Architecture Base" },
        { 0x00CD, "Intel Core i5" },
        { 0x00CE, "Intel Core i3" },
        { 0x00CF, "Intel Core i9" },
        { 0x00D0, "Intel Xeon D" },
        { 0x00D2, "VIA C7-M" },
        { 0x00D3, "VIA C7-D" },
        { 0x00D4, "VIA C7" },
        { 0x00D5, "VIA Eden" },
        { 0x00D6, "Multi-Core Intel Xeon" },
        { 0x00D7, "Dual-Core Intel Xeon 3xxx Series" },
        { 0x00D8, "Quad-Core Intel Xeon 3xxx Series" },
        { 0x00D9, "VIA Nano" },
        { 0x00DA, "Dual-Core Intel Xeon 5xxx Series" },
        { 0x00DB, "Quad-Core Intel Xeon 5xxx Series" },
        { 0x00DD, "Dual-Core Intel Xeon 7xxx Series" },
        { 0x00DE, "Quad-Core Intel Xeon 7xxx Series" },
        { 0x00DF, "Multi-Core Intel Xeon 7xxx Series" },
        { 0x00E0, "Multi-Core Intel Xeon 3400 Series" },
        { 0x00E4, "AMD Opteron 3000 Series" },
        { 0x00E5, "AMD Sempron II" },
        { 0x00E6, "Embedded AMD Opteron Quad-Core" },
        { 0x00E7, "AMD Phenom Triple-Core" },
        { 0x00E8, "AMD Turion Ultra Dual-Core Mobile" },
        { 0x00E9, "AMD Turion Dual-Core Mobile" },
        { 0x00EA, "AMD Athlon Dual-Core" },
        { 0x00EB, "AMD Sempron SI" },
        { 0x00EC, "AMD Phenom II" },
        { 0x00ED, "AMD Athlon II" },
        { 0x00EE, "Six-Core AMD Opteron" },
        { 0x00EF, "AMD Sempron M" },
        { 0x00FA, "i860" },
        { 0x00FB, "i960" },
        { 0x0100, "ARMv7" },
        { 0x0101, "ARMv8" },
        { 0x0102, "ARMv9" },
        { 0x0104, "SH-3" },
        { 0x0105, "SH-4" },
        { 0x0118, "ARM" },
        { 0x0119, "StrongARM" },
        { 0x012C, "6x86" },
        { 0x012D, "MediaGX" },
        { 0x012E, "MII" },
        { 0x0140, "WinChip" },
        { 0x015E, "DSP" },
        { 0x01F4, "Video Processor" },
        { 0x0200, "RISC-V RV32" },
        { 0x0201, "RISC-V RV64" },
        { 0x0202, "RISC-V RV128" },
        { 0x0258, "LoongArch" },
        { 0x0259, "Loongson 1" },
        { 0x025A, "Loongson 2" },
        { 0x025B, "Loongson 3" },
        { 0x025C, "Loongson 2K" },
        { 0x025D, "Loongson 3A" },
        { 0x025E, "Loongson 3B" },
        { 0x025F, "Loongson 3C" },
        { 0x0260, "Loongson 3D" },
        { 0x0261, "Loongson 3E" },
        { 0x0262, "Dual-Core Loongson 2K 2xxx Series" },
        { 0x026C, "Quad-Core Loongson 3A 5xxx Series" },
        { 0x026D, "Multi-Core Loongson 3A 5xxx Series" },
        { 0x026E, "Quad-Core Loongson 3B 5xxx Series" },
        { 0x026F, "Multi-Core Loongson 3B 5xxx Series" },
        { 0x0270, "Multi-Core Loongson 3C 5xxx Series" },
        { 0x0271, "Multi-Core Loongson 3D 5xxx Series" }
    };

    quint16 family = table_->family;

    // FEh means the real value is stored in the wider 'family 2' field (2.6+).
    if (family == 0xFE)
    {
        if (table_->length < 0x2A)
            return QString();

        family = table_->family2;
    }

    for (size_t i = 0; i < std::size(kFamily); ++i)
    {
        if (kFamily[i].value == family)
            return kFamily[i].name;
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosProcessor::status() const
{
    static const char* kStatus[] =
    {
        "Unknown", // 0x00
        "Enabled",
        "Disabled By User",
        "Disabled By BIOS",
        "Idle" // 0x04
    };

    const quint8 status = table_->status & 0x07;

    if (status <= 0x04)
        return kStatus[status];

    if (status == 0x07)
        return "Other";

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosProcessor::upgrade() const
{
    static const char* kUpgrade[] =
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
        "Socket BGA1288",
        "Socket rPGA988B",
        "Socket BGA1023",
        "Socket BGA1224",
        "Socket LGA1155",
        "Socket LGA1356",
        "Socket LGA2011",
        "Socket FS1",
        "Socket FS2",
        "Socket FM1",
        "Socket FM2",
        "Socket LGA2011-3",
        "Socket LGA1356-3",
        "Socket LGA1150",
        "Socket BGA1168",
        "Socket BGA1234",
        "Socket BGA1364",
        "Socket AM4",
        "Socket LGA1151",
        "Socket BGA1356",
        "Socket BGA1440",
        "Socket BGA1515",
        "Socket LGA3647-1",
        "Socket SP3",
        "Socket SP3r2",
        "Socket LGA2066",
        "Socket BGA1392",
        "Socket BGA1510",
        "Socket BGA1528",
        "Socket LGA4189",
        "Socket LGA1200",
        "Socket LGA4677",
        "Socket LGA1700",
        "Socket BGA1744",
        "Socket BGA1781",
        "Socket BGA1211",
        "Socket BGA2422",
        "Socket LGA1211",
        "Socket LGA2422",
        "Socket LGA5773",
        "Socket BGA5773",
        "Socket AM5",
        "Socket SP5",
        "Socket SP6" // 0x4B
    };

    if (table_->upgrade >= 0x01 && table_->upgrade <= 0x4B)
        return kUpgrade[table_->upgrade - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosProcessor::id() const
{
    return table_->id;
}

//--------------------------------------------------------------------------------------------------
double SmbiosProcessor::voltage() const
{
    // With bit 7 set the remaining bits carry the current voltage in tenths of a volt. Otherwise
    // the field lists the voltages the socket supports, not the one the processor runs at.
    if (!(table_->voltage & 0x80))
        return 0.0;

    return static_cast<double>(table_->voltage & 0x7F) / 10.0;
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosProcessor::externalClock() const
{
    // Zero means the external clock is unknown.
    return table_->external_clock;
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosProcessor::maxSpeed() const
{
    // Zero means the maximum speed is unknown.
    return table_->max_speed;
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosProcessor::currentSpeed() const
{
    // Zero means the current speed is unknown.
    return table_->current_speed;
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosProcessor::coreCount() const
{
    if (table_->length < 0x24)
        return 0;

    // FFh means the real value is stored in the wider 'core count 2' field (3.0+).
    if (table_->core_count == 0xFF)
    {
        if (table_->length < 0x2C)
            return 0;

        return table_->core_count2 == 0xFFFF ? 0 : table_->core_count2;
    }

    return table_->core_count;
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosProcessor::coreEnabled() const
{
    if (table_->length < 0x25)
        return 0;

    // FFh means the real value is stored in the wider 'core enabled 2' field (3.0+).
    if (table_->core_enabled == 0xFF)
    {
        if (table_->length < 0x2E)
            return 0;

        return table_->core_enabled2 == 0xFFFF ? 0 : table_->core_enabled2;
    }

    return table_->core_enabled;
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosProcessor::threadCount() const
{
    if (table_->length < 0x26)
        return 0;

    // FFh means the real value is stored in the wider 'thread count 2' field (3.0+).
    if (table_->thread_count == 0xFF)
    {
        if (table_->length < 0x30)
            return 0;

        return table_->thread_count2 == 0xFFFF ? 0 : table_->thread_count2;
    }

    return table_->thread_count;
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosProcessor::threadEnabled() const
{
    // The field appeared in SMBIOS 3.6. Zero means the value is unknown.
    if (table_->length < 0x32)
        return 0;

    return table_->thread_enabled == 0xFFFF ? 0 : table_->thread_enabled;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosProcessor::is64Bit() const
{
    return (characteristics() & 0x0004) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosProcessor::isMultiCore() const
{
    return (characteristics() & 0x0008) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosProcessor::isHardwareThread() const
{
    return (characteristics() & 0x0010) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosProcessor::isExecuteProtection() const
{
    return (characteristics() & 0x0020) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosProcessor::isEnhancedVirtualization() const
{
    return (characteristics() & 0x0040) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosProcessor::isPowerPerformanceControl() const
{
    return (characteristics() & 0x0080) != 0;
}

//--------------------------------------------------------------------------------------------------
quint16 SmbiosProcessor::characteristics() const
{
    // The field appeared in SMBIOS 2.5. Bit 1 marks the characteristics as unknown, which makes
    // the remaining bits meaningless.
    if (table_->length < 0x28 || (table_->characteristics & 0x0002))
        return 0;

    return table_->characteristics;
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
