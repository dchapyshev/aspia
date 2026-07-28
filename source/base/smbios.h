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

#include <QtTypes>

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
    SMBIOS_TABLE_TYPE_BIOS                 = 0x00,
    SMBIOS_TABLE_TYPE_SYSTEM               = 0x01,
    SMBIOS_TABLE_TYPE_BASEBOARD            = 0x02,
    SMBIOS_TABLE_TYPE_CHASSIS              = 0x03,
    SMBIOS_TABLE_TYPE_PROCESSOR            = 0x04,
    SMBIOS_TABLE_TYPE_CACHE                = 0x07,
    SMBIOS_TABLE_TYPE_PORT_CONNECTOR       = 0x08,
    SMBIOS_TABLE_TYPE_SYSTEM_SLOT          = 0x09,
    SMBIOS_TABLE_TYPE_ONBOARD_DEVICE       = 0x0A,
    SMBIOS_TABLE_TYPE_OEM_STRINGS          = 0x0B,
    SMBIOS_TABLE_TYPE_CONFIGURATION_OPTION = 0x0C,
    SMBIOS_TABLE_TYPE_MEMORY_ARRAY         = 0x10,
    SMBIOS_TABLE_TYPE_MEMORY_DEVICE        = 0x11,
    SMBIOS_TABLE_TYPE_MEMORY_ERROR         = 0x12,
    SMBIOS_TABLE_TYPE_MEMORY_ARRAY_ADDRESS = 0x13,
    SMBIOS_TABLE_TYPE_MEMORY_DEVICE_ADDR   = 0x14,
    SMBIOS_TABLE_TYPE_POINTING_DEVICE      = 0x15,
    SMBIOS_TABLE_TYPE_PORTABLE_BATTERY     = 0x16,
    SMBIOS_TABLE_TYPE_VOLTAGE_PROBE        = 0x1A,
    SMBIOS_TABLE_TYPE_COOLING_DEVICE       = 0x1B,
    SMBIOS_TABLE_TYPE_TEMPERATURE_PROBE    = 0x1C,
    SMBIOS_TABLE_TYPE_CURRENT_PROBE        = 0x1D,
    SMBIOS_TABLE_TYPE_SYSTEM_BOOT          = 0x20,
    SMBIOS_TABLE_TYPE_POWER_SUPPLY         = 0x27,
    SMBIOS_TABLE_TYPE_ONBOARD_DEVICE_EXT   = 0x29,
    SMBIOS_TABLE_TYPE_TPM_DEVICE           = 0x2B,
    SMBIOS_TABLE_TYPE_END_OF_TABLE         = 0x7F
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

struct SmbiosSystemTable : public SmbiosTable
{
    // 2.0+
    quint8 manufacturer;  // 04h
    quint8 product_name;  // 05h
    quint8 version;       // 06h
    quint8 serial_number; // 07h

    // 2.1+
    quint8 uuid[16];      // 08h-17h
    quint8 wakeup_type;   // 18h

    // 2.4+
    quint8 sku_number;    // 19h
    quint8 family;        // 1Ah
};

struct SmbiosBaseboardTable : public SmbiosTable
{
    quint8 manufacturer;    // 04h
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

struct SmbiosChassisTable : public SmbiosTable
{
    // 2.0+
    quint8 manufacturer;       // 04h
    quint8 type;               // 05h, bit 7 is the chassis lock presence
    quint8 version;            // 06h
    quint8 serial_number;      // 07h
    quint8 asset_tag;          // 08h

    // 2.1+
    quint8 boot_up_state;      // 09h
    quint8 power_supply_state; // 0Ah
    quint8 thermal_state;      // 0Bh
    quint8 security_status;    // 0Ch

    // 2.3+
    quint32 oem_defined;       // 0Dh-10h
    quint8 height;             // 11h
    quint8 power_cords;        // 12h
    quint8 element_count;      // 13h
    quint8 element_length;     // 14h
    // element_count * element_length bytes of contained elements
    // 2.7+: the SKU number (BYTE) follows the contained elements
};

struct SmbiosProcessorTable : public SmbiosTable
{
    // 2.0+
    quint8 socket_designation; // 04h
    quint8 type;               // 05h
    quint8 family;             // 06h
    quint8 manufacturer;       // 07h
    quint64 id;                // 08h-0Fh
    quint8 version;            // 10h
    quint8 voltage;            // 11h
    quint16 external_clock;    // 12h-13h
    quint16 max_speed;         // 14h-15h
    quint16 current_speed;     // 16h-17h
    quint8 status;             // 18h
    quint8 upgrade;            // 19h

    // 2.1+
    quint16 l1_cache_handle;   // 1Ah-1Bh
    quint16 l2_cache_handle;   // 1Ch-1Dh
    quint16 l3_cache_handle;   // 1Eh-1Fh

    // 2.3+
    quint8 serial_number;      // 20h
    quint8 asset_tag;          // 21h
    quint8 part_number;        // 22h

    // 2.5+
    quint8 core_count;         // 23h
    quint8 core_enabled;       // 24h
    quint8 thread_count;       // 25h
    quint16 characteristics;   // 26h-27h

    // 2.6+
    quint16 family2;           // 28h-29h

    // 3.0+
    quint16 core_count2;       // 2Ah-2Bh
    quint16 core_enabled2;     // 2Ch-2Dh
    quint16 thread_count2;     // 2Eh-2Fh

    // 3.6+
    quint16 thread_enabled;    // 30h-31h

    // 3.8+
    quint8 socket_type;        // 32h
};

struct SmbiosCacheTable : public SmbiosTable
{
    // 2.0+
    quint8 socket_designation;    // 04h
    quint16 configuration;        // 05h-06h
    quint16 max_size;             // 07h-08h
    quint16 current_size;         // 09h-0Ah
    quint16 supported_sram_type;  // 0Bh-0Ch
    quint16 current_sram_type;    // 0Dh-0Eh

    // 2.1+
    quint8 speed;                 // 0Fh
    quint8 error_correction_type; // 10h
    quint8 type;                  // 11h
    quint8 associativity;         // 12h

    // 3.1+
    quint32 max_size2;            // 13h-16h
    quint32 current_size2;        // 17h-1Ah
};

struct SmbiosPortConnectorTable : public SmbiosTable
{
    quint8 internal_designator; // 04h
    quint8 internal_connector;  // 05h
    quint8 external_designator; // 06h
    quint8 external_connector;  // 07h
    quint8 type;                // 08h
};

struct SmbiosSystemSlotTable : public SmbiosTable
{
    // 2.0+
    quint8 designation;      // 04h
    quint8 type;             // 05h
    quint8 data_bus_width;   // 06h
    quint8 usage;            // 07h
    quint8 slot_length;      // 08h, the length of the slot, not of the table
    quint16 id;              // 09h-0Ah
    quint8 characteristics1; // 0Bh

    // 2.1+
    quint8 characteristics2; // 0Ch

    // 2.6+
    quint16 segment_group;   // 0Dh-0Eh
    quint8 bus_number;       // 0Fh
    quint8 device_function;  // 10h
};

struct SmbiosOnBoardDeviceTable : public SmbiosTable
{
    // The table carries (length - 4) / 2 devices, each one a pair of the type byte and the
    // description string number. The fields below are the first pair.
    quint8 device_type; // 04h, bit 7 tells whether the device is enabled
    quint8 description; // 05h
};

// The layout of both the OEM strings (Type 11) and the system configuration options (Type 12):
// the strings themselves live in the string area of the table.
struct SmbiosStringListTable : public SmbiosTable
{
    quint8 count; // 04h
};

struct SmbiosMemoryArrayTable : public SmbiosTable
{
    // 2.1+
    quint8 location;           // 04h
    quint8 use;                // 05h
    quint8 error_correction;   // 06h
    quint32 max_capacity;      // 07h-0Ah, in kilobytes
    quint16 error_info_handle; // 0Bh-0Ch
    quint16 device_count;      // 0Dh-0Eh

    // 2.7+
    quint64 ext_max_capacity;  // 0Fh-16h, in bytes
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
    quint16 speed;                  // 15h-16h, in MT/s
    quint8 manufacturer;            // 17h
    quint8 serial_number;           // 18h
    quint8 asset_tag;               // 19h
    quint8 part_number;             // 1Ah

    // 2.6+
    quint8 attributes;              // 1Bh, bits 3:0 carry the rank

    // 2.7+
    quint32 ext_size;               // 1Ch-1Fh, in megabytes
    quint16 configured_speed;       // 20h-21h, in MT/s

    // 2.8+
    quint16 min_voltage;            // 22h-23h, in millivolts
    quint16 max_voltage;            // 24h-25h
    quint16 configured_voltage;     // 26h-27h

    // 3.2+
    quint8 technology;              // 28h
    quint16 operating_mode;         // 29h-2Ah
    quint8 firmware_version;        // 2Bh
    quint16 module_manufacturer_id; // 2Ch-2Dh
    quint16 module_product_id;      // 2Eh-2Fh
    quint16 controller_vendor_id;   // 30h-31h
    quint16 controller_product_id;  // 32h-33h
    quint64 non_volatile_size;      // 34h-3Bh, in bytes
    quint64 volatile_size;          // 3Ch-43h, in bytes
    quint64 cache_size;             // 44h-4Bh, in bytes
    quint64 logical_size;           // 4Ch-53h, in bytes

    // 3.3+
    quint32 ext_speed;              // 54h-57h, in MT/s
    quint32 ext_configured_speed;   // 58h-5Bh, in MT/s

    // 3.7+
    quint16 pmic0_manufacturer_id;  // 5Ch-5Dh
    quint16 pmic0_revision;         // 5Eh-5Fh
    quint16 rcd_manufacturer_id;    // 60h-61h
    quint16 rcd_revision;           // 62h-63h
};

struct SmbiosMemoryErrorTable : public SmbiosTable
{
    quint8 error_type;       // 04h
    quint8 granularity;      // 05h
    quint8 operation;        // 06h
    quint32 vendor_syndrome; // 07h-0Ah
    quint32 array_address;   // 0Bh-0Eh
    quint32 device_address;  // 0Fh-12h
    quint32 resolution;      // 13h-16h
};

struct SmbiosMemoryArrayAddressTable : public SmbiosTable
{
    // 2.1+
    quint32 start_address;     // 04h-07h, in kilobytes
    quint32 end_address;       // 08h-0Bh, the last kilobyte of the range
    quint16 array_handle;      // 0Ch-0Dh
    quint8 partition_width;    // 0Eh

    // 2.7+
    quint64 ext_start_address; // 0Fh-16h, in bytes
    quint64 ext_end_address;   // 17h-1Eh, the last byte of the range
};

struct SmbiosMemoryDeviceAddressTable : public SmbiosTable
{
    // 2.1+
    quint32 start_address;        // 04h-07h, in kilobytes
    quint32 end_address;          // 08h-0Bh, the last kilobyte of the range
    quint16 device_handle;        // 0Ch-0Dh
    quint16 array_address_handle; // 0Eh-0Fh
    quint8 row_position;          // 10h
    quint8 interleave_position;   // 11h
    quint8 interleave_depth;      // 12h

    // 2.7+
    quint64 ext_start_address;    // 13h-1Ah, in bytes
    quint64 ext_end_address;      // 1Bh-22h, the last byte of the range
};

struct SmbiosPointingDeviceTable : public SmbiosTable
{
    quint8 type;           // 04h
    quint8 interface_type; // 05h
    quint8 button_count;   // 06h
};

struct SmbiosPortableBatteryTable : public SmbiosTable
{
    // 2.1+
    quint8 location;               // 04h
    quint8 manufacturer;           // 05h
    quint8 manufacture_date;       // 06h
    quint8 serial_number;          // 07h
    quint8 device_name;            // 08h
    quint8 device_chemistry;       // 09h
    quint16 design_capacity;       // 0Ah-0Bh, in milliwatt-hours
    quint16 design_voltage;        // 0Ch-0Dh, in millivolts
    quint8 sbds_version;           // 0Eh
    quint8 max_error;              // 0Fh

    // 2.2+
    quint16 sbds_serial_number;    // 10h-11h
    quint16 sbds_manufacture_date; // 12h-13h
    quint8 sbds_device_chemistry;  // 14h
    quint8 capacity_multiplier;    // 15h
    quint32 oem_specific;          // 16h-19h
};

// The layout of the voltage probe (Type 26), the temperature probe (Type 28) and the electrical
// current probe (Type 29): the tables differ only in the unit of the values they report.
struct SmbiosProbeTable : public SmbiosTable
{
    quint8 description;    // 04h
    quint8 location;       // 05h, bits 4:0 the location, bits 7:5 the status
    quint16 max_value;     // 06h-07h
    quint16 min_value;     // 08h-09h
    quint16 resolution;    // 0Ah-0Bh, in tenths of the unit of the values
    quint16 tolerance;     // 0Ch-0Dh
    quint16 accuracy;      // 0Eh-0Fh, in hundredths of a percent
    quint32 oem_defined;   // 10h-13h
    quint16 nominal_value; // 14h-15h
};

struct SmbiosCoolingDeviceTable : public SmbiosTable
{
    quint16 probe_handle;  // 04h-05h, the temperature probe of the device
    quint8 device_type;    // 06h, bits 4:0 the type, bits 7:5 the status
    quint8 unit_group;     // 07h
    quint32 oem_defined;   // 08h-0Bh

    // 2.7+
    quint16 nominal_speed; // 0Ch-0Dh, in revolutions per minute
    quint8 description;    // 0Eh
};

struct SmbiosSystemBootTable : public SmbiosTable
{
    quint8 reserved[6]; // 04h-09h
    quint8 status;      // 0Ah, the first byte of the boot status field

    // 0Bh-13h, the rest of the boot status field: data whose meaning depends on the status.
};

struct SmbiosPowerSupplyTable : public SmbiosTable
{
    quint8 unit_group;             // 04h
    quint8 location;               // 05h
    quint8 device_name;            // 06h
    quint8 manufacturer;           // 07h
    quint8 serial_number;          // 08h
    quint8 asset_tag;              // 09h
    quint8 model_part_number;      // 0Ah
    quint8 revision_level;         // 0Bh
    quint16 max_power_capacity;    // 0Ch-0Dh, in milliwatts
    quint16 characteristics;       // 0Eh-0Fh
    quint16 voltage_probe_handle;  // 10h-11h
    quint16 cooling_device_handle; // 12h-13h
    quint16 current_probe_handle;  // 14h-15h
};

struct SmbiosOnBoardDeviceExtTable : public SmbiosTable
{
    quint8 designation;     // 04h
    quint8 device_type;     // 05h, bit 7 tells whether the device is enabled
    quint8 type_instance;   // 06h
    quint16 segment_group;  // 07h-08h
    quint8 bus_number;      // 09h
    quint8 device_function; // 0Ah
};

struct SmbiosTpmDeviceTable : public SmbiosTable
{
    quint8 vendor_id[4];        // 04h-07h
    quint8 major_version;       // 08h
    quint8 minor_version;       // 09h
    quint32 firmware_version1;  // 0Ah-0Dh
    quint32 firmware_version2;  // 0Eh-11h
    quint8 description;         // 12h
    quint64 characteristics;    // 13h-1Ah
    quint32 oem_defined;        // 1Bh-1Eh
};

#pragma pack(pop)

#endif // BASE_SMBIOS_H
