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

#include <QDate>

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

//--------------------------------------------------------------------------------------------------
// Converts a 16-bit cache size field to bytes. Bit 15 selects the granularity.
quint64 cacheSize(quint16 size)
{
    if (size & 0x8000)
        return static_cast<quint64>(size & 0x7FFF) * 64ULL * 1024ULL;

    return static_cast<quint64>(size) * 1024ULL;
}

//--------------------------------------------------------------------------------------------------
// Converts a 32-bit cache size field (3.1+) to bytes. Bit 31 selects the granularity.
quint64 cacheSize2(quint32 size)
{
    if (size & 0x80000000)
        return static_cast<quint64>(size & 0x7FFFFFFF) * 64ULL * 1024ULL;

    return static_cast<quint64>(size) * 1024ULL;
}

//--------------------------------------------------------------------------------------------------
// The internal and the external connector of a port share the same value list.
QString connectorType(quint8 type)
{
    static const char* kType[] =
    {
        "None", // 0x00
        "Centronics",
        "Mini Centronics",
        "Proprietary",
        "DB-25 Pin Male",
        "DB-25 Pin Female",
        "DB-15 Pin Male",
        "DB-15 Pin Female",
        "DB-9 Pin Male",
        "DB-9 Pin Female",
        "RJ-11",
        "RJ-45",
        "50-pin MiniSCSI",
        "Mini-DIN",
        "Micro-DIN",
        "PS/2",
        "Infrared",
        "HP-HIL",
        "Access Bus (USB)",
        "SSA SCSI",
        "Circular DIN-8 Male",
        "Circular DIN-8 Female",
        "On Board IDE",
        "On Board Floppy",
        "9-pin Dual Inline (pin 10 cut)",
        "25-pin Dual Inline (pin 26 cut)",
        "50-pin Dual Inline",
        "68-pin Dual Inline",
        "On Board Sound Input From CD-ROM",
        "Mini-Centronics Type-14",
        "Mini-Centronics Type-26",
        "Mini-jack (headphones)",
        "BNC",
        "1394",
        "SAS/SATA Plug Receptacle",
        "USB Type-C Receptacle" // 0x23
    };

    static const char* kPc98Type[] =
    {
        "PC-98", // 0xA0
        "PC-98Hireso",
        "PC-H98",
        "PC-98Note",
        "PC-98Full" // 0xA4
    };

    if (type <= 0x23)
        return kType[type];

    if (type >= 0xA0 && type <= 0xA4)
        return kPc98Type[type - 0xA0];

    if (type == 0xFF)
        return "Other";

    return QString();
}

//--------------------------------------------------------------------------------------------------
// The legacy and the extended on board devices tables share the same device type list.
QString onBoardDeviceType(quint8 type)
{
    static const char* kType[] =
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
        "SAS Controller",
        "Wireless LAN",
        "Bluetooth",
        "WWAN",
        "eMMC",
        "NVMe Controller",
        "UFS Controller" // 0x10
    };

    // Bit 7 carries the enabled flag, the type itself is in the remaining bits.
    const quint8 device_type = type & 0x7F;

    if (device_type >= 0x01 && device_type <= 0x10)
        return kType[device_type - 0x01];

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
quint32 SmbiosBios::address() const
{
    // The field holds the segment of the address in real mode addressing.
    return static_cast<quint32>(table_->address_segment) << 4;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosBios::romSize() const
{
    // The field counts blocks of 64K, one less than there really are.
    if (table_->rom_size != 0xFF)
        return (static_cast<quint64>(table_->rom_size) + 1) * 64 * 1024;

    // FFh means the size does not fit the byte and is reported by the extended field of SMBIOS
    // 3.1. Firmware of an earlier version leaves the real size unknown.
    if (table_->length < 0x1A)
        return 0;

    const quint64 size = table_->ext_rom_size & 0x3FFF;

    // The two upper bits carry the unit of the size.
    switch (table_->ext_rom_size >> 14)
    {
        case 0x00:
            return size * 1024 * 1024;

        case 0x01:
            return size * 1024 * 1024 * 1024;

        default:
            return 0;
    }
}

//--------------------------------------------------------------------------------------------------
QString SmbiosBios::revision() const
{
    // The pair of bytes appeared in SMBIOS 2.4, FFh in either of them means the release is not
    // reported.
    if (table_->length < 0x16 || table_->major_release == 0xFF ||
        table_->minor_release == 0xFF)
    {
        return QString();
    }

    return QString("%1.%2").arg(table_->major_release).arg(table_->minor_release);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosBios::firmwareRevision() const
{
    // FFh means the system has no embedded controller.
    if (table_->length < 0x18 || table_->ctrl_major_release == 0xFF ||
        table_->ctrl_minor_release == 0xFF)
    {
        return QString();
    }

    return QString("%1.%2").arg(table_->ctrl_major_release).arg(table_->ctrl_minor_release);
}

//--------------------------------------------------------------------------------------------------
QStringList SmbiosBios::characteristics() const
{
    // The names start at bit 4: the bits below it do not name a feature.
    static const char* kCharacteristics[] =
    {
        "ISA is supported", // Bit 4
        "MCA is supported",
        "EISA is supported",
        "PCI is supported",
        "PC Card (PCMCIA) is supported",
        "Plug and Play is supported",
        "APM is supported",
        "BIOS is upgradeable",
        "BIOS shadowing is allowed",
        "VL-VESA is supported",
        "ESCD support is available",
        "Boot from CD is supported",
        "Selectable boot is supported",
        "BIOS ROM is socketed",
        "Boot from PC Card (PCMCIA) is supported",
        "EDD specification is supported",
        "Japanese floppy for NEC 9800 1.2 MB is supported (int 13h)",
        "Japanese floppy for Toshiba 1.2 MB is supported (int 13h)",
        "5.25\"/360 kB floppy services are supported (int 13h)",
        "5.25\"/1.2 MB floppy services are supported (int 13h)",
        "3.5\"/720 kB floppy services are supported (int 13h)",
        "3.5\"/2.88 MB floppy services are supported (int 13h)",
        "Print screen service is supported (int 5h)",
        "8042 keyboard services are supported (int 9h)",
        "Serial services are supported (int 14h)",
        "Printer services are supported (int 17h)",
        "CGA/mono video services are supported (int 10h)",
        "NEC PC-98" // Bit 31
    };

    // Bits of the first extension byte (2.4+).
    static const char* kExtCharacteristics1[] =
    {
        "ACPI is supported", // Bit 0
        "USB legacy is supported",
        "AGP is supported",
        "I2O boot is supported",
        "LS-120 SuperDisk boot is supported",
        "ATAPI ZIP drive boot is supported",
        "IEEE 1394 boot is supported",
        "Smart battery is supported" // Bit 7
    };

    // Bits of the second extension byte (2.4+), the last two of them added by SMBIOS 3.5.
    static const char* kExtCharacteristics2[] =
    {
        "BIOS boot specification is supported", // Bit 0
        "Function key-initiated network boot is supported",
        "Targeted content distribution is supported",
        "UEFI specification is supported",
        "The system is a virtual machine",
        "Manufacturing mode is supported",
        "Manufacturing mode is enabled" // Bit 6
    };

    QStringList result;

    // Bit 3 tells that the firmware fills none of the bits above it.
    if (!(table_->characters & 0x08))
    {
        for (size_t i = 0; i < std::size(kCharacteristics); ++i)
        {
            if (table_->characters & (1ULL << (i + 4)))
                result << kCharacteristics[i];
        }
    }

    if (table_->length >= 0x13)
    {
        for (size_t i = 0; i < std::size(kExtCharacteristics1); ++i)
        {
            if (table_->ext_characters1 & (1 << i))
                result << kExtCharacteristics1[i];
        }
    }

    if (table_->length >= 0x14)
    {
        for (size_t i = 0; i < std::size(kExtCharacteristics2); ++i)
        {
            if (table_->ext_characters2 & (1 << i))
                result << kExtCharacteristics2[i];
        }
    }

    return result;
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
    return smbiosString(table_, table_->manufacturer);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosBaseboard::product() const
{
    return smbiosString(table_, table_->product);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosBaseboard::version() const
{
    return smbiosString(table_, table_->version);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosBaseboard::serialNumber() const
{
    return smbiosString(table_, table_->serial_number);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosBaseboard::assetTag() const
{
    if (table_->length < 0x09)
        return QString();

    return smbiosString(table_, table_->asset_tag);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosBaseboard::location() const
{
    if (table_->length < 0x0B)
        return QString();

    return smbiosString(table_, table_->location);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosBaseboard::type() const
{
    static const char* kType[] =
    {
        "Unknown", // 0x01
        "Other",
        "Server Blade",
        "Connectivity Switch",
        "System Management Module",
        "Processor Module",
        "I/O Module",
        "Memory Module",
        "Daughter Board",
        "Motherboard",
        "Processor/Memory Module",
        "Processor/IO Module",
        "Interconnect Board" // 0x0D
    };

    if (table_->length < 0x0E)
        return QString();

    if (table_->board_type >= 0x01 && table_->board_type <= 0x0D)
        return kType[table_->board_type - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
bool SmbiosBaseboard::isHostingBoard() const
{
    return (featureFlags() & 0x01) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosBaseboard::requiresDaughterBoard() const
{
    return (featureFlags() & 0x02) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosBaseboard::isRemovable() const
{
    return (featureFlags() & 0x04) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosBaseboard::isReplaceable() const
{
    return (featureFlags() & 0x08) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosBaseboard::isHotSwappable() const
{
    return (featureFlags() & 0x10) != 0;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosBaseboard::featureFlags() const
{
    if (table_->length < 0x0A)
        return 0;

    return table_->feature_flags;
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
        { 0x0271, "Multi-Core Loongson 3D 5xxx Series" },
        { 0x0300, "Intel Core 3" },
        { 0x0301, "Intel Core 5" },
        { 0x0302, "Intel Core 7" },
        { 0x0303, "Intel Core 9" },
        { 0x0304, "Intel Core Ultra 3" },
        { 0x0305, "Intel Core Ultra 5" },
        { 0x0306, "Intel Core Ultra 7" },
        { 0x0307, "Intel Core Ultra 9" }
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
        "Socket SP6",
        "Socket BGA883",
        "Socket BGA1190",
        "Socket BGA4129",
        "Socket LGA4710",
        "Socket LGA7529",
        "Socket BGA1964",
        "Socket BGA1792",
        "Socket BGA2049",
        "Socket BGA2551",
        "Socket LGA1851",
        "Socket BGA2114",
        "Socket BGA2833" // 0x57
    };

    if (table_->upgrade >= 0x01 && table_->upgrade <= 0x57)
        return kUpgrade[table_->upgrade - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosProcessor::socketType() const
{
    // The socket named as a string, for the sockets the enumerated upgrade value has no code
    // for. The field appeared in SMBIOS 3.8.
    if (table_->length < 0x33)
        return QString();

    return smbiosString(table_, table_->socket_type);
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
quint16 SmbiosProcessor::l1CacheHandle() const
{
    // The cache handles appeared in SMBIOS 2.1. FFFFh means the cache is not provided.
    if (table_->length < 0x1C)
        return 0xFFFF;

    return table_->l1_cache_handle;
}

//--------------------------------------------------------------------------------------------------
quint16 SmbiosProcessor::l2CacheHandle() const
{
    if (table_->length < 0x1E)
        return 0xFFFF;

    return table_->l2_cache_handle;
}

//--------------------------------------------------------------------------------------------------
quint16 SmbiosProcessor::l3CacheHandle() const
{
    if (table_->length < 0x20)
        return 0xFFFF;

    return table_->l3_cache_handle;
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
SmbiosCache::SmbiosCache(const SmbiosTable* table)
    : table_(static_cast<const SmbiosCacheTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosCache::isValid() const
{
    // 0Fh is the minimum length of the cache information table since SMBIOS 2.0.
    return table_->length >= 0x0F;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosCache::isEnabled() const
{
    // Bit 7 of the configuration tells whether the cache is enabled at boot time.
    return (table_->configuration & 0x0080) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosCache::isSocketed() const
{
    return (table_->configuration & 0x0008) != 0;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosCache::designation() const
{
    return smbiosString(table_, table_->socket_designation);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosCache::location() const
{
    static const char* kLocation[] =
    {
        "Internal", // 0b00
        "External",
        "Reserved",
        "Unknown" // 0b11
    };

    // Bits 6:5 of the configuration carry the location relative to the CPU module.
    return kLocation[(table_->configuration >> 5) & 0x03];
}

//--------------------------------------------------------------------------------------------------
QString SmbiosCache::mode() const
{
    static const char* kMode[] =
    {
        "Write Through", // 0b00
        "Write Back",
        "Varies With Memory Address",
        "Unknown" // 0b11
    };

    // Bits 9:8 of the configuration carry the operational mode.
    return kMode[(table_->configuration >> 8) & 0x03];
}

//--------------------------------------------------------------------------------------------------
QString SmbiosCache::type() const
{
    static const char* kType[] =
    {
        "Other", // 0x01
        "Unknown",
        "Instruction",
        "Data",
        "Unified" // 0x05
    };

    // The field appeared in SMBIOS 2.1.
    if (table_->length < 0x12)
        return QString();

    if (table_->type >= 0x01 && table_->type <= 0x05)
        return kType[table_->type - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosCache::errorCorrectionType() const
{
    static const char* kType[] =
    {
        "Other", // 0x01
        "Unknown",
        "None",
        "Parity",
        "Single-bit ECC",
        "Multi-bit ECC" // 0x06
    };

    // The field appeared in SMBIOS 2.1.
    if (table_->length < 0x11)
        return QString();

    if (table_->error_correction_type >= 0x01 && table_->error_correction_type <= 0x06)
        return kType[table_->error_correction_type - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosCache::associativity() const
{
    static const char* kAssociativity[] =
    {
        "Other", // 0x01
        "Unknown",
        "Direct Mapped",
        "2-way Set-Associative",
        "4-way Set-Associative",
        "Fully Associative",
        "8-way Set-Associative",
        "16-way Set-Associative",
        "12-way Set-Associative",
        "24-way Set-Associative",
        "32-way Set-Associative",
        "48-way Set-Associative",
        "64-way Set-Associative",
        "20-way Set-Associative" // 0x0E
    };

    // The field appeared in SMBIOS 2.1.
    if (table_->length < 0x13)
        return QString();

    if (table_->associativity >= 0x01 && table_->associativity <= 0x0E)
        return kAssociativity[table_->associativity - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosCache::currentSramType() const
{
    static const char* kType[] =
    {
        "Other", // bit 0
        "Unknown",
        "Non-Burst",
        "Burst",
        "Pipeline Burst",
        "Synchronous",
        "Asynchronous" // bit 6
    };

    // The field is a bit mask, but a cache runs on one type at a time.
    for (size_t i = 0; i < std::size(kType); ++i)
    {
        if (table_->current_sram_type & (1 << i))
            return kType[i];
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosCache::level() const
{
    // Bits 2:0 of the configuration carry the level with L1 encoded as zero.
    return (table_->configuration & 0x07) + 1;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosCache::maxSize() const
{
    // FFFFh means the size does not fit the field and is stored in the wider one (3.1+).
    if (table_->max_size == 0xFFFF)
    {
        if (table_->length < 0x17)
            return 0;

        return cacheSize2(table_->max_size2);
    }

    return cacheSize(table_->max_size);
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosCache::currentSize() const
{
    // FFFFh means the size does not fit the field and is stored in the wider one (3.1+).
    if (table_->current_size == 0xFFFF)
    {
        if (table_->length < 0x1B)
            return 0;

        return cacheSize2(table_->current_size2);
    }

    return cacheSize(table_->current_size);
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosCache::speed() const
{
    // The speed is measured in nanoseconds. The field appeared in SMBIOS 2.1, zero means the
    // speed is unknown.
    if (table_->length < 0x10)
        return 0;

    return table_->speed;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosCache::supportsNonBurst() const
{
    return (table_->supported_sram_type & 0x0004) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosCache::supportsBurst() const
{
    return (table_->supported_sram_type & 0x0008) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosCache::supportsPipelineBurst() const
{
    return (table_->supported_sram_type & 0x0010) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosCache::supportsSynchronous() const
{
    return (table_->supported_sram_type & 0x0020) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosCache::supportsAsynchronous() const
{
    return (table_->supported_sram_type & 0x0040) != 0;
}

//--------------------------------------------------------------------------------------------------
SmbiosPortConnector::SmbiosPortConnector(const SmbiosTable* table)
    : table_(static_cast<const SmbiosPortConnectorTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosPortConnector::isValid() const
{
    // 09h is the length of the port connector table in every SMBIOS version.
    return table_->length >= 0x09;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPortConnector::internalDesignator() const
{
    return smbiosString(table_, table_->internal_designator);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPortConnector::internalConnectorType() const
{
    return connectorType(table_->internal_connector);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPortConnector::externalDesignator() const
{
    return smbiosString(table_, table_->external_designator);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPortConnector::externalConnectorType() const
{
    return connectorType(table_->external_connector);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPortConnector::type() const
{
    static const char* kType[] =
    {
        "None", // 0x00
        "Parallel Port XT/AT Compatible",
        "Parallel Port PS/2",
        "Parallel Port ECP",
        "Parallel Port EPP",
        "Parallel Port ECP/EPP",
        "Serial Port XT/AT Compatible",
        "Serial Port 16450 Compatible",
        "Serial Port 16550 Compatible",
        "Serial Port 16550A Compatible",
        "SCSI Port",
        "MIDI Port",
        "Joy Stick Port",
        "Keyboard Port",
        "Mouse Port",
        "SSA SCSI",
        "USB",
        "FireWire (IEEE P1394)",
        "PCMCIA Type I",
        "PCMCIA Type II",
        "PCMCIA Type III",
        "Cardbus",
        "Access Bus Port",
        "SCSI II",
        "SCSI Wide",
        "PC-98",
        "PC-98-Hireso",
        "PC-H98",
        "Video Port",
        "Audio Port",
        "Modem Port",
        "Network Port",
        "SATA",
        "SAS",
        "MFDP (Multi-Function Display Port)",
        "Thunderbolt" // 0x23
    };

    static const char* k8251Type[] =
    {
        "8251 Compatible", // 0xA0
        "8251 FIFO Compatible" // 0xA1
    };

    if (table_->type <= 0x23)
        return kType[table_->type];

    if (table_->type >= 0xA0 && table_->type <= 0xA1)
        return k8251Type[table_->type - 0xA0];

    if (table_->type == 0xFF)
        return "Other";

    return QString();
}

//--------------------------------------------------------------------------------------------------
SmbiosSystemSlot::SmbiosSystemSlot(const SmbiosTable* table)
    : table_(static_cast<const SmbiosSystemSlotTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosSystemSlot::isValid() const
{
    // 0Ch is the minimum length of the system slot table since SMBIOS 2.0.
    return table_->length >= 0x0C;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosSystemSlot::designation() const
{
    return smbiosString(table_, table_->designation);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosSystemSlot::type() const
{
    static const char* kType[] =
    {
        "Other", // 0x01
        "Unknown",
        "ISA",
        "MCA",
        "EISA",
        "PCI",
        "PC Card (PCMCIA)",
        "VL-VESA",
        "Proprietary",
        "Processor Card Slot",
        "Proprietary Memory Card Slot",
        "I/O Riser Card Slot",
        "NuBus",
        "PCI - 66MHz Capable",
        "AGP",
        "AGP 2X",
        "AGP 4X",
        "PCI-X",
        "AGP 8X",
        "M.2 Socket 1-DP",
        "M.2 Socket 1-SD",
        "M.2 Socket 2",
        "M.2 Socket 3",
        "MXM Type I",
        "MXM Type II",
        "MXM Type III",
        "MXM Type III-HE",
        "MXM Type IV",
        "MXM 3.0 Type A",
        "MXM 3.0 Type B",
        "PCI Express Gen 2 SFF-8639 (U.2)",
        "PCI Express Gen 3 SFF-8639 (U.2)",
        "PCI Express Mini 52-pin With Bottom-side Keep-outs",
        "PCI Express Mini 52-pin Without Bottom-side Keep-outs",
        "PCI Express Mini 76-pin",
        "PCI Express Gen 4 SFF-8639 (U.2)",
        "PCI Express Gen 5 SFF-8639 (U.2)",
        "OCP NIC 3.0 Small Form Factor (SFF)",
        "OCP NIC 3.0 Large Form Factor (LFF)",
        "OCP NIC Prior to 3.0" // 0x28
    };

    // The PC-98 slots and the PCI Express ones up to the third generation live in their own
    // range of values.
    static const char* kLegacyType[] =
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
        "PCI Express Gen 2",
        "PCI Express Gen 2 x1",
        "PCI Express Gen 2 x2",
        "PCI Express Gen 2 x4",
        "PCI Express Gen 2 x8",
        "PCI Express Gen 2 x16",
        "PCI Express Gen 3",
        "PCI Express Gen 3 x1",
        "PCI Express Gen 3 x2",
        "PCI Express Gen 3 x4",
        "PCI Express Gen 3 x8",
        "PCI Express Gen 3 x16" // 0xB6
    };

    // The generations above the third continue behind a gap at B7h.
    static const char* kExpressType[] =
    {
        "PCI Express Gen 4", // 0xB8
        "PCI Express Gen 4 x1",
        "PCI Express Gen 4 x2",
        "PCI Express Gen 4 x4",
        "PCI Express Gen 4 x8",
        "PCI Express Gen 4 x16",
        "PCI Express Gen 5",
        "PCI Express Gen 5 x1",
        "PCI Express Gen 5 x2",
        "PCI Express Gen 5 x4",
        "PCI Express Gen 5 x8",
        "PCI Express Gen 5 x16",
        "PCI Express Gen 6 and Beyond",
        "EDSFF E1 Form Factor",
        "EDSFF E3 Form Factor" // 0xC6
    };

    if (table_->type >= 0x01 && table_->type <= 0x28)
        return kType[table_->type - 0x01];

    if (table_->type == 0x30)
        return "CXL Flexbus 1.0";

    if (table_->type >= 0xA0 && table_->type <= 0xB6)
        return kLegacyType[table_->type - 0xA0];

    if (table_->type >= 0xB8 && table_->type <= 0xC6)
        return kExpressType[table_->type - 0xB8];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosSystemSlot::dataBusWidth() const
{
    static const char* kWidth[] =
    {
        "Other", // 0x01
        "Unknown",
        "8 bit",
        "16 bit",
        "32 bit",
        "64 bit",
        "128 bit",
        "x1",
        "x2",
        "x4",
        "x8",
        "x12",
        "x16",
        "x32" // 0x0E
    };

    if (table_->data_bus_width >= 0x01 && table_->data_bus_width <= 0x0E)
        return kWidth[table_->data_bus_width - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosSystemSlot::usage() const
{
    static const char* kUsage[] =
    {
        "Other", // 0x01
        "Unknown",
        "Available",
        "In Use",
        "Unavailable" // 0x05
    };

    if (table_->usage >= 0x01 && table_->usage <= 0x05)
        return kUsage[table_->usage - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosSystemSlot::length() const
{
    static const char* kLength[] =
    {
        "Other", // 0x01
        "Unknown",
        "Short Length",
        "Long Length",
        "2.5\" Drive Form Factor",
        "3.5\" Drive Form Factor" // 0x06
    };

    if (table_->slot_length >= 0x01 && table_->slot_length <= 0x06)
        return kLength[table_->slot_length - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
quint16 SmbiosSystemSlot::id() const
{
    return table_->id;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosSystemSlot::hasBusAddress() const
{
    // The bus address appeared in SMBIOS 2.6. Slots outside a PCI bus report it as all ones.
    if (table_->length < 0x11)
        return false;

    return table_->segment_group != 0xFFFF || table_->bus_number != 0xFF ||
           table_->device_function != 0xFF;
}

//--------------------------------------------------------------------------------------------------
quint16 SmbiosSystemSlot::segmentGroupNumber() const
{
    if (!hasBusAddress())
        return 0;

    return table_->segment_group;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosSystemSlot::busNumber() const
{
    if (!hasBusAddress())
        return 0;

    return table_->bus_number;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosSystemSlot::deviceNumber() const
{
    if (!hasBusAddress())
        return 0;

    // Bits 7:3 of the field carry the device number.
    return (table_->device_function >> 3) & 0x1F;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosSystemSlot::functionNumber() const
{
    if (!hasBusAddress())
        return 0;

    // Bits 2:0 of the field carry the function number.
    return table_->device_function & 0x07;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosSystemSlot::provides5Volts() const
{
    return (characteristics1() & 0x02) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosSystemSlot::provides3Volts() const
{
    return (characteristics1() & 0x04) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosSystemSlot::isShared() const
{
    return (characteristics1() & 0x08) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosSystemSlot::supportsPme() const
{
    return (characteristics2() & 0x01) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosSystemSlot::supportsHotPlug() const
{
    return (characteristics2() & 0x02) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosSystemSlot::supportsSmbus() const
{
    return (characteristics2() & 0x04) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosSystemSlot::supportsBifurcation() const
{
    return (characteristics2() & 0x08) != 0;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosSystemSlot::characteristics1() const
{
    // Bit 0 marks the characteristics as unknown, which makes the remaining bits meaningless.
    if (table_->characteristics1 & 0x01)
        return 0;

    return table_->characteristics1;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosSystemSlot::characteristics2() const
{
    // The field appeared in SMBIOS 2.1 and shares the 'unknown' marker with the first one.
    if (table_->length < 0x0D || (table_->characteristics1 & 0x01))
        return 0;

    return table_->characteristics2;
}

//--------------------------------------------------------------------------------------------------
SmbiosOnBoardDevices::SmbiosOnBoardDevices(const SmbiosTable* table)
    : table_(static_cast<const SmbiosOnBoardDeviceTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosOnBoardDevices::isValid() const
{
    // 06h is the length of the table with a single device, the smallest one that makes sense.
    return table_->length >= 0x06;
}

//--------------------------------------------------------------------------------------------------
int SmbiosOnBoardDevices::count() const
{
    if (!isValid())
        return 0;

    // Every device takes a pair of bytes behind the table header.
    return (table_->length - static_cast<int>(sizeof(SmbiosTable))) / 2;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosOnBoardDevices::description(int index) const
{
    const quint8* device_data = device(index);
    if (!device_data)
        return QString();

    return smbiosString(table_, device_data[1]);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosOnBoardDevices::type(int index) const
{
    const quint8* device_data = device(index);
    if (!device_data)
        return QString();

    return onBoardDeviceType(device_data[0]);
}

//--------------------------------------------------------------------------------------------------
bool SmbiosOnBoardDevices::isEnabled(int index) const
{
    const quint8* device_data = device(index);
    if (!device_data)
        return false;

    return (device_data[0] & 0x80) != 0;
}

//--------------------------------------------------------------------------------------------------
const quint8* SmbiosOnBoardDevices::device(int index) const
{
    if (index < 0 || index >= count())
        return nullptr;

    return reinterpret_cast<const quint8*>(table_) + sizeof(SmbiosTable) + index * 2;
}

//--------------------------------------------------------------------------------------------------
SmbiosStringList::SmbiosStringList(const SmbiosTable* table)
    : table_(static_cast<const SmbiosStringListTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosStringList::isValid() const
{
    // 05h is the length of both tables in every SMBIOS version.
    return table_->length >= 0x05;
}

//--------------------------------------------------------------------------------------------------
int SmbiosStringList::count() const
{
    return table_->count;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosStringList::string(int index) const
{
    if (index < 0 || index >= count())
        return QString();

    // The string area numbers its strings starting at one. Firmware that declares more strings
    // than it stores leaves the last ones empty.
    return smbiosString(table_, static_cast<quint8>(index + 1));
}

//--------------------------------------------------------------------------------------------------
SmbiosMemoryArray::SmbiosMemoryArray(const SmbiosTable* table)
    : table_(static_cast<const SmbiosMemoryArrayTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosMemoryArray::isValid() const
{
    // 0Fh is the minimum length of the physical memory array table since SMBIOS 2.1.
    return table_->length >= 0x0F;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosMemoryArray::location() const
{
    static const char* kLocation[] =
    {
        "Other", // 0x01
        "Unknown",
        "System Board Or Motherboard",
        "ISA Add-on Card",
        "EISA Add-on Card",
        "PCI Add-on Card",
        "MCA Add-on Card",
        "PCMCIA Add-on Card",
        "Proprietary Add-on Card",
        "NuBus" // 0x0A
    };

    static const char* kPc98Location[] =
    {
        "PC-98/C20 Add-on Card", // 0xA0
        "PC-98/C24 Add-on Card",
        "PC-98/E Add-on Card",
        "PC-98/Local Bus Add-on Card",
        "CXL Add-on Card" // 0xA4
    };

    if (table_->location >= 0x01 && table_->location <= 0x0A)
        return kLocation[table_->location - 0x01];

    if (table_->location >= 0xA0 && table_->location <= 0xA4)
        return kPc98Location[table_->location - 0xA0];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosMemoryArray::use() const
{
    static const char* kUse[] =
    {
        "Other", // 0x01
        "Unknown",
        "System Memory",
        "Video Memory",
        "Flash Memory",
        "Non-volatile RAM",
        "Cache Memory" // 0x07
    };

    if (table_->use >= 0x01 && table_->use <= 0x07)
        return kUse[table_->use - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosMemoryArray::errorCorrection() const
{
    static const char* kCorrection[] =
    {
        "Other", // 0x01
        "Unknown",
        "None",
        "Parity",
        "Single-bit ECC",
        "Multi-bit ECC",
        "CRC" // 0x07
    };

    if (table_->error_correction >= 0x01 && table_->error_correction <= 0x07)
        return kCorrection[table_->error_correction - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryArray::maxCapacity() const
{
    // 80000000h means the capacity does not fit the field and is stored in the extended one
    // (2.7+), which is measured in bytes instead of kilobytes.
    if (table_->max_capacity == 0x80000000)
    {
        if (table_->length < 0x17)
            return 0;

        return table_->ext_max_capacity;
    }

    return static_cast<quint64>(table_->max_capacity) * 1024ULL;
}

//--------------------------------------------------------------------------------------------------
quint16 SmbiosMemoryArray::deviceCount() const
{
    return table_->device_count;
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
QString SmbiosMemoryDevice::bankLocator() const
{
    return smbiosString(table_, table_->bank_locator);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosMemoryDevice::manufacturer() const
{
    if (table_->length < 0x18)
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
QString SmbiosMemoryDevice::serialNumber() const
{
    if (table_->length < 0x19)
        return QString();

    return smbiosString(table_, table_->serial_number);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosMemoryDevice::assetTag() const
{
    if (table_->length < 0x1A)
        return QString();

    return smbiosString(table_, table_->asset_tag);
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryDevice::size() const
{
    if (table_->module_size == 0x7FFF)
    {
        // The actual size is stored in the extended field (2.7+), which counts megabytes with
        // bit 31 reserved. A table too short to carry the field cannot tell the size.
        if (table_->length < 0x20)
            return 0;

        return static_cast<quint64>(table_->ext_size & 0x7FFFFFFFUL) * 1024ULL * 1024ULL;
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
        "HBM3 (High Bandwidth Memory Generation 3)",
        "MRDIMM" // 0x25
    };

    if (table_->memory_type >= 0x01 && table_->memory_type <= 0x25)
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
        "FB-DIMM",
        "Die",
        "CAMM",
        "CUDIMM",
        "CSODIMM" // 0x13
    };

    if (table_->form_factor >= 0x01 && table_->form_factor <= 0x13)
        return kFormFactor[table_->form_factor - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosMemoryDevice::technology() const
{
    static const char* kTechnology[] =
    {
        "Other", // 0x01
        "Unknown",
        "DRAM",
        "NVDIMM-N",
        "NVDIMM-F",
        "NVDIMM-P",
        "Intel Optane Persistent Memory",
        "MRDIMM" // 0x08
    };

    // The field appeared in SMBIOS 3.2.
    if (table_->length < 0x29)
        return QString();

    if (table_->technology >= 0x01 && table_->technology <= 0x08)
        return kTechnology[table_->technology - 0x01];

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
QString SmbiosMemoryDevice::firmwareVersion() const
{
    // The field appeared in SMBIOS 3.2.
    if (table_->length < 0x2C)
        return QString();

    return smbiosString(table_, table_->firmware_version);
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosMemoryDevice::speed() const
{
    // The field appeared in SMBIOS 2.3. Zero means the speed is unknown.
    if (table_->length < 0x17)
        return 0;

    // FFFFh means the speed is 65535 MT/s or more and the real value sits in the extended
    // field (3.3+).
    if (table_->speed == 0xFFFF)
    {
        if (table_->length < 0x58)
            return 0;

        return table_->ext_speed & 0x7FFFFFFFUL;
    }

    return table_->speed;
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosMemoryDevice::configuredSpeed() const
{
    // The speed the device runs at, which may be lower than the one it supports. The field
    // appeared in SMBIOS 2.7, zero means it is unknown.
    if (table_->length < 0x22)
        return 0;

    // FFFFh means the speed is 65535 MT/s or more and the real value sits in the extended
    // field (3.3+).
    if (table_->configured_speed == 0xFFFF)
    {
        if (table_->length < 0x5C)
            return 0;

        return table_->ext_configured_speed & 0x7FFFFFFFUL;
    }

    return table_->configured_speed;
}

//--------------------------------------------------------------------------------------------------
quint16 SmbiosMemoryDevice::totalWidth() const
{
    // The width in bits, including the bits used for error correction. FFFFh means it is
    // unknown.
    if (table_->total_width == 0xFFFF)
        return 0;

    return table_->total_width;
}

//--------------------------------------------------------------------------------------------------
quint16 SmbiosMemoryDevice::dataWidth() const
{
    // The width in bits without the ones used for error correction. FFFFh means it is unknown.
    if (table_->data_width == 0xFFFF)
        return 0;

    return table_->data_width;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosMemoryDevice::rank() const
{
    // Bits 3:0 of the attributes carry the rank (2.6+). Zero means it is unknown.
    if (table_->length < 0x1C)
        return 0;

    return table_->attributes & 0x0F;
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosMemoryDevice::minVoltage() const
{
    // The voltages are measured in millivolts and appeared in SMBIOS 2.8. Zero means the value
    // is unknown.
    if (table_->length < 0x24)
        return 0;

    return table_->min_voltage;
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosMemoryDevice::maxVoltage() const
{
    if (table_->length < 0x26)
        return 0;

    return table_->max_voltage;
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosMemoryDevice::configuredVoltage() const
{
    if (table_->length < 0x28)
        return 0;

    return table_->configured_voltage;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryDevice::nonVolatileSize() const
{
    // The sizes of the regions appeared in SMBIOS 3.2. All ones mean the size is unknown, zero
    // means the device has no region of the kind.
    if (table_->length < 0x3C || table_->non_volatile_size == 0xFFFFFFFFFFFFFFFFULL)
        return 0;

    return table_->non_volatile_size;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryDevice::volatileSize() const
{
    if (table_->length < 0x44 || table_->volatile_size == 0xFFFFFFFFFFFFFFFFULL)
        return 0;

    return table_->volatile_size;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryDevice::cacheSize() const
{
    if (table_->length < 0x4C || table_->cache_size == 0xFFFFFFFFFFFFFFFFULL)
        return 0;

    return table_->cache_size;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryDevice::logicalSize() const
{
    if (table_->length < 0x54 || table_->logical_size == 0xFFFFFFFFFFFFFFFFULL)
        return 0;

    return table_->logical_size;
}

//--------------------------------------------------------------------------------------------------
QStringList SmbiosMemoryDevice::typeDetail() const
{
    // The names start at bit 1: bit 0 does not name a property.
    static const char* kDetail[] =
    {
        "Other", // Bit 1
        "Unknown",
        "Fast-paged",
        "Static Column",
        "Pseudo-static",
        "RAMBUS",
        "Synchronous",
        "CMOS",
        "EDO",
        "Window DRAM",
        "Cache DRAM",
        "Non-volatile",
        "Registered (Buffered)",
        "Unbuffered (Unregistered)",
        "LRDIMM" // Bit 15
    };

    QStringList result;

    for (size_t i = 0; i < std::size(kDetail); ++i)
    {
        if (table_->type_detail & (1 << (i + 1)))
            result << kDetail[i];
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
quint16 SmbiosMemoryDevice::arrayHandle() const
{
    // The handle of the physical memory array the device belongs to.
    return table_->memory_array_handle;
}

//--------------------------------------------------------------------------------------------------
SmbiosMemoryError::SmbiosMemoryError(const SmbiosTable* table)
    : table_(static_cast<const SmbiosMemoryErrorTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosMemoryError::isValid() const
{
    // 17h is the length of the memory error information table in every SMBIOS version.
    return table_->length >= 0x17;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosMemoryError::type() const
{
    static const char* kType[] =
    {
        "Other", // 0x01
        "Unknown",
        "OK",
        "Bad Read",
        "Parity Error",
        "Single-bit Error",
        "Double-bit Error",
        "Multi-bit Error",
        "Nibble Error",
        "Checksum Error",
        "CRC Error",
        "Corrected Single-bit Error",
        "Corrected Error",
        "Uncorrectable Error" // 0x0E
    };

    if (table_->error_type >= 0x01 && table_->error_type <= 0x0E)
        return kType[table_->error_type - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosMemoryError::granularity() const
{
    static const char* kGranularity[] =
    {
        "Other", // 0x01
        "Unknown",
        "Device Level",
        "Memory Partition Level" // 0x04
    };

    if (table_->granularity >= 0x01 && table_->granularity <= 0x04)
        return kGranularity[table_->granularity - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosMemoryError::operation() const
{
    static const char* kOperation[] =
    {
        "Other", // 0x01
        "Unknown",
        "Read",
        "Write",
        "Partial Write" // 0x05
    };

    if (table_->operation >= 0x01 && table_->operation <= 0x05)
        return kOperation[table_->operation - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosMemoryError::vendorSyndrome() const
{
    // Zero means the vendor-specific data is not filled in.
    return table_->vendor_syndrome;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryError::arrayErrorAddress() const
{
    // 80000000h means the address is unknown.
    if (table_->array_address == 0x80000000)
        return 0;

    return table_->array_address;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryError::deviceErrorAddress() const
{
    if (table_->device_address == 0x80000000)
        return 0;

    return table_->device_address;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryError::errorResolution() const
{
    // The range the error address is known within, in bytes. 80000000h means it is unknown.
    if (table_->resolution == 0x80000000)
        return 0;

    return table_->resolution;
}

//--------------------------------------------------------------------------------------------------
SmbiosMemoryArrayAddress::SmbiosMemoryArrayAddress(const SmbiosTable* table)
    : table_(static_cast<const SmbiosMemoryArrayAddressTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosMemoryArrayAddress::isValid() const
{
    // 0Fh is the minimum length of the memory array mapped address table since SMBIOS 2.1.
    return table_->length >= 0x0F;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryArrayAddress::startAddress() const
{
    if (isExtended())
        return table_->ext_start_address;

    if (table_->start_address == 0xFFFFFFFF)
        return 0;

    return static_cast<quint64>(table_->start_address) * 1024ULL;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryArrayAddress::endAddress() const
{
    if (isExtended())
        return table_->ext_end_address;

    if (table_->start_address == 0xFFFFFFFF)
        return 0;

    // The field holds the last kilobyte of the range, so the last byte of it belongs to the
    // kilobyte behind.
    return (static_cast<quint64>(table_->end_address) + 1ULL) * 1024ULL - 1ULL;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryArrayAddress::size() const
{
    const quint64 start = startAddress();
    const quint64 end = endAddress();

    // Equal addresses mean the range is not filled in rather than a single byte of memory.
    if (end <= start)
        return 0;

    return end - start + 1ULL;
}

//--------------------------------------------------------------------------------------------------
quint16 SmbiosMemoryArrayAddress::arrayHandle() const
{
    return table_->array_handle;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosMemoryArrayAddress::partitionWidth() const
{
    // The number of memory devices forming a single row. FFh means it is unknown.
    if (table_->partition_width == 0xFF)
        return 0;

    return table_->partition_width;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosMemoryArrayAddress::isExtended() const
{
    // FFFFFFFFh in the starting address means both addresses come from the extended fields
    // (2.7+), which are measured in bytes instead of kilobytes.
    return table_->start_address == 0xFFFFFFFF && table_->length >= 0x1F;
}

//--------------------------------------------------------------------------------------------------
SmbiosMemoryDeviceAddress::SmbiosMemoryDeviceAddress(const SmbiosTable* table)
    : table_(static_cast<const SmbiosMemoryDeviceAddressTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosMemoryDeviceAddress::isValid() const
{
    // 13h is the minimum length of the memory device mapped address table since SMBIOS 2.1.
    return table_->length >= 0x13;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryDeviceAddress::startAddress() const
{
    if (isExtended())
        return table_->ext_start_address;

    if (table_->start_address == 0xFFFFFFFF)
        return 0;

    return static_cast<quint64>(table_->start_address) * 1024ULL;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryDeviceAddress::endAddress() const
{
    if (isExtended())
        return table_->ext_end_address;

    if (table_->start_address == 0xFFFFFFFF)
        return 0;

    // The field holds the last kilobyte of the range, so the last byte of it belongs to the
    // kilobyte behind.
    return (static_cast<quint64>(table_->end_address) + 1ULL) * 1024ULL - 1ULL;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosMemoryDeviceAddress::size() const
{
    const quint64 start = startAddress();
    const quint64 end = endAddress();

    // Equal addresses mean the range is not filled in rather than a single byte of memory.
    if (end <= start)
        return 0;

    return end - start + 1ULL;
}

//--------------------------------------------------------------------------------------------------
quint16 SmbiosMemoryDeviceAddress::deviceHandle() const
{
    return table_->device_handle;
}

//--------------------------------------------------------------------------------------------------
quint16 SmbiosMemoryDeviceAddress::arrayAddressHandle() const
{
    return table_->array_address_handle;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosMemoryDeviceAddress::rowPosition() const
{
    // The position of the device in a row of the partition. FFh means it is unknown and zero is
    // reserved by the specification.
    if (table_->row_position == 0xFF)
        return 0;

    return table_->row_position;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosMemoryDeviceAddress::interleavePosition() const
{
    // Zero tells the device is not interleaved, FFh that the position is unknown.
    if (table_->interleave_position == 0xFF)
        return 0;

    return table_->interleave_position;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosMemoryDeviceAddress::interleaveDepth() const
{
    if (table_->interleave_depth == 0xFF)
        return 0;

    return table_->interleave_depth;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosMemoryDeviceAddress::isExtended() const
{
    // FFFFFFFFh in the starting address means both addresses come from the extended fields
    // (2.7+), which are measured in bytes instead of kilobytes.
    return table_->start_address == 0xFFFFFFFF && table_->length >= 0x23;
}

//--------------------------------------------------------------------------------------------------
SmbiosPointingDevice::SmbiosPointingDevice(const SmbiosTable* table)
    : table_(static_cast<const SmbiosPointingDeviceTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosPointingDevice::isValid() const
{
    // 07h is the length of the built-in pointing device table in every SMBIOS version.
    return table_->length >= 0x07;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPointingDevice::type() const
{
    static const char* kType[] =
    {
        "Other", // 0x01
        "Unknown",
        "Mouse",
        "Track Ball",
        "Track Point",
        "Glide Point",
        "Touch Pad",
        "Touch Screen",
        "Optical Sensor" // 0x09
    };

    if (table_->type >= 0x01 && table_->type <= 0x09)
        return kType[table_->type - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPointingDevice::interfaceType() const
{
    static const char* kInterface[] =
    {
        "Other", // 0x01
        "Unknown",
        "Serial",
        "PS/2",
        "Infrared",
        "HP-HIL",
        "Bus Mouse",
        "ADB (Apple Desktop Bus)" // 0x08
    };

    static const char* kBusInterface[] =
    {
        "Bus Mouse DB-9", // 0xA0
        "Bus Mouse Micro-DIN",
        "USB",
        "I2C",
        "SPI" // 0xA4
    };

    if (table_->interface_type >= 0x01 && table_->interface_type <= 0x08)
        return kInterface[table_->interface_type - 0x01];

    if (table_->interface_type >= 0xA0 && table_->interface_type <= 0xA4)
        return kBusInterface[table_->interface_type - 0xA0];

    return QString();
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosPointingDevice::buttonCount() const
{
    return table_->button_count;
}

//--------------------------------------------------------------------------------------------------
SmbiosPortableBattery::SmbiosPortableBattery(const SmbiosTable* table)
    : table_(static_cast<const SmbiosPortableBatteryTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosPortableBattery::isValid() const
{
    // 10h is the minimum length of the portable battery table since SMBIOS 2.1.
    return table_->length >= 0x10;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPortableBattery::location() const
{
    return smbiosString(table_, table_->location);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPortableBattery::manufacturer() const
{
    return smbiosString(table_, table_->manufacturer);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPortableBattery::manufactureDate() const
{
    QString result = smbiosString(table_, table_->manufacture_date);
    if (!result.isEmpty())
        return result;

    // Batteries following the Smart Battery Data Specification leave the string empty and pack
    // the date into a word (2.2+): bits 15:9 hold the year biased by 1980, bits 8:5 the month
    // and bits 4:0 the day.
    if (table_->length < 0x14 || !table_->sbds_manufacture_date)
        return QString();

    const int year = 1980 + (table_->sbds_manufacture_date >> 9);
    const int month = (table_->sbds_manufacture_date >> 5) & 0x0F;
    const int day = table_->sbds_manufacture_date & 0x1F;

    return QDate(year, month, day).toString(Qt::ISODate);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPortableBattery::serialNumber() const
{
    QString result = smbiosString(table_, table_->serial_number);
    if (!result.isEmpty())
        return result;

    // Batteries following the Smart Battery Data Specification report the serial number as a
    // word instead of a string (2.2+).
    if (table_->length < 0x12 || !table_->sbds_serial_number)
        return QString();

    return QString("%1").arg(table_->sbds_serial_number, 4, 16, QChar('0')).toUpper();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPortableBattery::deviceName() const
{
    return smbiosString(table_, table_->device_name);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPortableBattery::chemistry() const
{
    static const char* kChemistry[] =
    {
        "Other", // 0x01
        "Unknown",
        "Lead Acid",
        "Nickel Cadmium",
        "Nickel Metal Hydride",
        "Lithium-ion",
        "Zinc Air",
        "Lithium Polymer" // 0x08
    };

    // Batteries following the Smart Battery Data Specification report the chemistry as a string
    // and leave the enumerated field unknown (2.2+).
    if (table_->device_chemistry == 0x02 && table_->length >= 0x15)
    {
        QString result = smbiosString(table_, table_->sbds_device_chemistry);
        if (!result.isEmpty())
            return result;
    }

    if (table_->device_chemistry >= 0x01 && table_->device_chemistry <= 0x08)
        return kChemistry[table_->device_chemistry - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPortableBattery::sbdsVersion() const
{
    return smbiosString(table_, table_->sbds_version);
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosPortableBattery::designCapacity() const
{
    // The capacity is measured in milliwatt-hours. Zero means it is unknown.
    if (!table_->design_capacity)
        return 0;

    // Since SMBIOS 2.2 the stored value is scaled down by the multiplier.
    if (table_->length >= 0x16 && table_->capacity_multiplier)
        return static_cast<quint32>(table_->design_capacity) * table_->capacity_multiplier;

    return table_->design_capacity;
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosPortableBattery::designVoltage() const
{
    // The voltage is measured in millivolts. Zero means it is unknown.
    return table_->design_voltage;
}

//--------------------------------------------------------------------------------------------------
int SmbiosPortableBattery::maxError() const
{
    if (table_->max_error == 0xFF)
        return -1;

    return table_->max_error;
}

//--------------------------------------------------------------------------------------------------
SmbiosSystemBoot::SmbiosSystemBoot(const SmbiosTable* table)
    : table_(static_cast<const SmbiosSystemBootTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosSystemBoot::isValid() const
{
    // 0Bh is enough to carry the first byte of the boot status, which is the only one the meaning
    // of which does not depend on the status itself.
    return table_->length >= 0x0B;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosSystemBoot::status() const
{
    static const char* kStatus[] =
    {
        "No Errors Detected", // 0x00
        "No Bootable Media",
        "Operating System Failed To Load",
        "Firmware-detected Hardware Failure",
        "Operating System-detected Hardware Failure",
        "User-requested Boot",
        "System Security Violation",
        "Previously-requested Image",
        "System Watchdog Timer Expired" // 0x08
    };

    if (table_->status <= 0x08)
        return kStatus[table_->status];

    if (table_->status >= 0x80 && table_->status <= 0xBF)
        return "OEM-specific";

    if (table_->status >= 0xC0)
        return "Product-specific";

    return QString();
}

//--------------------------------------------------------------------------------------------------
SmbiosPowerSupply::SmbiosPowerSupply(const SmbiosTable* table)
    : table_(static_cast<const SmbiosPowerSupplyTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosPowerSupply::isValid() const
{
    // 16h is the length of the system power supply table in every SMBIOS version.
    return table_->length >= 0x16;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosPowerSupply::isPresent() const
{
    return (table_->characteristics & 0x0002) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosPowerSupply::isUnplugged() const
{
    return (table_->characteristics & 0x0004) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosPowerSupply::isHotReplaceable() const
{
    return (table_->characteristics & 0x0001) != 0;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosPowerSupply::unitGroup() const
{
    // Supplies sharing the group form a redundant set, a value of 1 means the supply is on its
    // own.
    return table_->unit_group;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPowerSupply::location() const
{
    return smbiosString(table_, table_->location);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPowerSupply::deviceName() const
{
    return smbiosString(table_, table_->device_name);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPowerSupply::manufacturer() const
{
    return smbiosString(table_, table_->manufacturer);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPowerSupply::serialNumber() const
{
    return smbiosString(table_, table_->serial_number);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPowerSupply::assetTag() const
{
    return smbiosString(table_, table_->asset_tag);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPowerSupply::modelPartNumber() const
{
    return smbiosString(table_, table_->model_part_number);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPowerSupply::revisionLevel() const
{
    return smbiosString(table_, table_->revision_level);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPowerSupply::type() const
{
    static const char* kType[] =
    {
        "Other", // 0x01
        "Unknown",
        "Linear",
        "Switching",
        "Battery",
        "UPS",
        "Converter",
        "Regulator" // 0x08
    };

    // Bits 13:10 of the characteristics carry the type.
    const quint16 type = (table_->characteristics >> 10) & 0x0F;

    if (type >= 0x01 && type <= 0x08)
        return kType[type - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPowerSupply::status() const
{
    static const char* kStatus[] =
    {
        "Other", // 0x01
        "Unknown",
        "OK",
        "Non-critical",
        "Critical" // 0x05
    };

    // Bits 9:7 of the characteristics carry the status.
    const quint16 status = (table_->characteristics >> 7) & 0x07;

    if (status >= 0x01 && status <= 0x05)
        return kStatus[status - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosPowerSupply::inputVoltageRangeSwitching() const
{
    static const char* kSwitching[] =
    {
        "Other", // 0x01
        "Unknown",
        "Manual",
        "Auto-switch",
        "Wide Range",
        "Not Applicable" // 0x06
    };

    // Bits 6:3 of the characteristics carry the switching capability.
    const quint16 switching = (table_->characteristics >> 3) & 0x0F;

    if (switching >= 0x01 && switching <= 0x06)
        return kSwitching[switching - 0x01];

    return QString();
}

//--------------------------------------------------------------------------------------------------
quint32 SmbiosPowerSupply::maxPowerCapacity() const
{
    // 8000h means the capacity is unknown. Note that the field is measured in milliwatts, which
    // leaves no room for the output of a modern supply - such firmware reports it as unknown.
    if (table_->max_power_capacity == 0x8000)
        return 0;

    return table_->max_power_capacity;
}

//--------------------------------------------------------------------------------------------------
SmbiosOnBoardDeviceExt::SmbiosOnBoardDeviceExt(const SmbiosTable* table)
    : table_(static_cast<const SmbiosOnBoardDeviceExtTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosOnBoardDeviceExt::isValid() const
{
    // 0Bh is the length of the extended on board device table in every SMBIOS version.
    return table_->length >= 0x0B;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosOnBoardDeviceExt::isEnabled() const
{
    return (table_->device_type & 0x80) != 0;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosOnBoardDeviceExt::designation() const
{
    return smbiosString(table_, table_->designation);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosOnBoardDeviceExt::type() const
{
    return onBoardDeviceType(table_->device_type);
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosOnBoardDeviceExt::typeInstance() const
{
    // The number of the device among the ones of the same type on the board.
    return table_->type_instance;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosOnBoardDeviceExt::hasBusAddress() const
{
    // Devices outside a PCI bus report the address as all ones.
    return table_->segment_group != 0xFFFF || table_->bus_number != 0xFF ||
           table_->device_function != 0xFF;
}

//--------------------------------------------------------------------------------------------------
quint16 SmbiosOnBoardDeviceExt::segmentGroupNumber() const
{
    if (!hasBusAddress())
        return 0;

    return table_->segment_group;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosOnBoardDeviceExt::busNumber() const
{
    if (!hasBusAddress())
        return 0;

    return table_->bus_number;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosOnBoardDeviceExt::deviceNumber() const
{
    if (!hasBusAddress())
        return 0;

    // Bits 7:3 of the field carry the device number.
    return (table_->device_function >> 3) & 0x1F;
}

//--------------------------------------------------------------------------------------------------
quint8 SmbiosOnBoardDeviceExt::functionNumber() const
{
    if (!hasBusAddress())
        return 0;

    // Bits 2:0 of the field carry the function number.
    return table_->device_function & 0x07;
}

//--------------------------------------------------------------------------------------------------
SmbiosTpmDevice::SmbiosTpmDevice(const SmbiosTable* table)
    : table_(static_cast<const SmbiosTpmDeviceTable*>(table))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SmbiosTpmDevice::isValid() const
{
    // 1Fh is the length of the TPM device table in every SMBIOS version.
    return table_->length >= 0x1F;
}

//--------------------------------------------------------------------------------------------------
QString SmbiosTpmDevice::vendorId() const
{
    // The vendor id assigned by the Trusted Computing Group is four bytes of ASCII padded with
    // zeros. Firmware sometimes puts a numeric id there instead, so unprintable bytes are
    // dropped rather than shown as garbage.
    QString result;

    for (size_t i = 0; i < sizeof(table_->vendor_id); ++i)
    {
        const quint8 symbol = table_->vendor_id[i];
        if (!symbol)
            break;

        if (symbol >= 32 && symbol < 127)
            result += QChar::fromLatin1(static_cast<char>(symbol));
    }

    return result.trimmed();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosTpmDevice::specVersion() const
{
    if (!table_->major_version)
        return QString();

    return QString("%1.%2").arg(table_->major_version).arg(table_->minor_version);
}

//--------------------------------------------------------------------------------------------------
QString SmbiosTpmDevice::firmwareVersion() const
{
    // The layout of the field depends on the specification the device follows.
    if (table_->major_version == 0x01)
    {
        // TPM 1.2 keeps the numbers in the second and the third byte of the field.
        return QString("%1.%2").arg((table_->firmware_version1 >> 8) & 0xFF)
                               .arg((table_->firmware_version1 >> 16) & 0xFF);
    }

    if (table_->major_version == 0x02)
    {
        // TPM 2.0 splits the field in halves.
        return QString("%1.%2").arg(table_->firmware_version1 >> 16)
                               .arg(table_->firmware_version1 & 0xFFFF);
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString SmbiosTpmDevice::description() const
{
    return smbiosString(table_, table_->description);
}

//--------------------------------------------------------------------------------------------------
bool SmbiosTpmDevice::isFamilyConfigurableByFirmware() const
{
    return (characteristics() & 0x0008) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosTpmDevice::isFamilyConfigurableBySoftware() const
{
    return (characteristics() & 0x0010) != 0;
}

//--------------------------------------------------------------------------------------------------
bool SmbiosTpmDevice::isFamilyConfigurableByOem() const
{
    return (characteristics() & 0x0020) != 0;
}

//--------------------------------------------------------------------------------------------------
quint64 SmbiosTpmDevice::characteristics() const
{
    // Bit 2 marks the characteristics as not supported, which makes the remaining bits
    // meaningless.
    if (table_->characteristics & 0x0004)
        return 0;

    return table_->characteristics;
}
