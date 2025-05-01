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

#ifndef BASE_SMBIOS_H
#define BASE_SMBIOS_H

#include <QtGlobal>

namespace base {

const size_t kSmbiosMaxDataSize = 0xFA00; // 64kB

struct SmbiosDump
{
    uint8_t used_20_calling_method;
    uint8_t smbios_major_version;
    uint8_t smbios_minor_version;
    uint8_t dmi_revision;
    uint32_t length;
    uint8_t smbios_table_data[kSmbiosMaxDataSize];
};

enum SmbiosTableType : uint8_t
{
    SMBIOS_TABLE_TYPE_BIOS          = 0x00,
    SMBIOS_TABLE_TYPE_BASEBOARD     = 0x02,
    SMBIOS_TABLE_TYPE_MEMORY_DEVICE = 0x11,
    SMBIOS_TABLE_TYPE_END_OF_TABLE  = 0x7F
};

#pragma pack(push, 1)

struct SmbiosTable
{
    uint8_t type;    // 00h
    uint8_t length;  // 01h
    quint16 handle; // 02h-03h
};

struct SmbiosBiosTable : public SmbiosTable
{
    // 2.0+
    uint8_t vendor;             // 04h
    uint8_t version;            // 05h
    quint16 address_segment;   // 06h-07h
    uint8_t release_date;       // 08h
    uint8_t rom_size;           // 09h
    quint64 characters;        // 0Ah-11h

    // 2.4+
    uint8_t ext_characters1;    // 12h
    uint8_t ext_characters2;    // 13h
    uint8_t major_release;      // 14h
    uint8_t minor_release;      // 15h
    uint8_t ctrl_major_release; // 16h
    uint8_t ctrl_minor_release; // 17h

    // 3.1+
    quint16 ext_rom_size;      // 18h
};

struct SmbiosBaseboardTable : public SmbiosTable
{
    uint8_t manufactorer;    // 04h
    uint8_t product;         // 05h
    uint8_t version;         // 06h
    uint8_t serial_number;   // 07h
    uint8_t asset_tag;       // 08h
    uint8_t feature_flags;   // 09h
    uint8_t location;        // 0Ah
    quint16 chassis_handle; // 0Bh-0Ch
    uint8_t board_type;      // 0Dh
    uint8_t obj_handles_num; // 0Eh
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
    uint8_t form_factor;             // 0Eh
    uint8_t device_set;              // 0Fh
    uint8_t device_location;         // 10h
    uint8_t bank_locator;            // 11h
    uint8_t memory_type;             // 12h
    quint16 type_detail;            // 13h-14h

    // 2.3+
    quint16 speed;                  // 15h-16h
    uint8_t manufacturer;            // 17h
    uint8_t serial_number;           // 18h
    uint8_t asset_tag;               // 19h
    uint8_t part_number;             // 1Ah

    // 2.6+
    uint8_t attributes;              // 1Bh

    // 2.7+
    uint32_t ext_size;               // 1Ch-1Fh
    quint16 configured_clock_speed; // 20h-21h

    // 2.8+
    quint16 min_voltage;            // 22h-23h
    quint16 max_voltage;            // 24h-25h
    quint16 configured_voltage;     // 26h-27h

};

#pragma pack(pop)

} // namespace base

#endif // BASE_SMBIOS_H
