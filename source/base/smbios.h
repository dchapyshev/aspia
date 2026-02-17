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

#ifndef BASE_SMBIOS_H
#define BASE_SMBIOS_H

#include <QtGlobal>

namespace base {

const size_t kSmbiosMaxDataSize = 0xFA00; // 64kB

struct SmbiosDump
{
    quint8 used_20_calling_method;
    quint8 smbios_major_version;
    quint8 smbios_minor_version;
    quint8 dmi_revision;
    quint32 length;
    quint8 smbios_table_data[kSmbiosMaxDataSize];
};

enum SmbiosTableType : quint8
{
    SMBIOS_TABLE_TYPE_BIOS          = 0x00,
    SMBIOS_TABLE_TYPE_BASEBOARD     = 0x02,
    SMBIOS_TABLE_TYPE_MEMORY_DEVICE = 0x11,
    SMBIOS_TABLE_TYPE_END_OF_TABLE  = 0x7F
};

#pragma pack(push, 1)

struct SmbiosTable
{
    quint8 type;    // 00h
    quint8 length;  // 01h
    quint16 handle; // 02h-03h
};

struct SmbiosBiosTable : public SmbiosTable
{
    // 2.0+
    quint8 vendor;             // 04h
    quint8 version;            // 05h
    quint16 address_segment;   // 06h-07h
    quint8 release_date;       // 08h
    quint8 rom_size;           // 09h
    quint64 characters;        // 0Ah-11h

    // 2.4+
    quint8 ext_characters1;    // 12h
    quint8 ext_characters2;    // 13h
    quint8 major_release;      // 14h
    quint8 minor_release;      // 15h
    quint8 ctrl_major_release; // 16h
    quint8 ctrl_minor_release; // 17h

    // 3.1+
    quint16 ext_rom_size;      // 18h
};

struct SmbiosBaseboardTable : public SmbiosTable
{
    quint8 manufactorer;    // 04h
    quint8 product;         // 05h
    quint8 version;         // 06h
    quint8 serial_number;   // 07h
    quint8 asset_tag;       // 08h
    quint8 feature_flags;   // 09h
    quint8 location;        // 0Ah
    quint16 chassis_handle; // 0Bh-0Ch
    quint8 board_type;      // 0Dh
    quint8 obj_handles_num; // 0Eh
    // obj_handles_num * WORDs
};

struct SmbiosMemoryDeviceTable : public SmbiosTable
{
    // 2.1+
    quint16 memory_array_handle;    // 04h-05h
    quint16 error_info_handle;      // 06h-07h
    quint16 total_width;            // 08h-09h
    quint16 data_width;             // 0Ah-0Bh
    quint16 module_size;            // 0Ch-0Dh
    quint8 form_factor;             // 0Eh
    quint8 device_set;              // 0Fh
    quint8 device_location;         // 10h
    quint8 bank_locator;            // 11h
    quint8 memory_type;             // 12h
    quint16 type_detail;            // 13h-14h

    // 2.3+
    quint16 speed;                  // 15h-16h
    quint8 manufacturer;            // 17h
    quint8 serial_number;           // 18h
    quint8 asset_tag;               // 19h
    quint8 part_number;             // 1Ah

    // 2.6+
    quint8 attributes;              // 1Bh

    // 2.7+
    quint32 ext_size;               // 1Ch-1Fh
    quint16 configured_clock_speed; // 20h-21h

    // 2.8+
    quint16 min_voltage;            // 22h-23h
    quint16 max_voltage;            // 24h-25h
    quint16 configured_voltage;     // 26h-27h

};

#pragma pack(pop)

} // namespace base

#endif // BASE_SMBIOS_H
