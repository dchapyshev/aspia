//
// PROJECT:         Aspia
// FILE:            base/devices/physical_drive_util.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DEVICES__PHYSICAL_DRIVE_UTIL_H
#define _ASPIA_BASE__DEVICES__PHYSICAL_DRIVE_UTIL_H

#include "base/devices/device.h"
#include "base/devices/physical_drive_smart.h"

#include <cstdint>
#include <memory>
#include <winioctl.h>

namespace aspia {

#pragma pack (push, drive_identify_data, 1)
struct DriveIdentifyData
{
    uint16_t general_configuration;          // 0
    uint16_t number_of_cylinders;            // 1
    uint16_t spec_config;                    // 2
    uint16_t number_of_heads;                // 3
    uint16_t unformatted_bytes_per_track;    // 4
    uint16_t unformatted_bytes_per_sector;   // 5
    uint16_t sectors_per_track;              // 6
    uint16_t vendor_unique1[3];              // 7-9
    char serial_number[20];                  // 10-19
    uint16_t buffer_type;                    // 20
    uint16_t buffer_size;                    // 21
    uint16_t ecc_size;                       // 22
    char firmware_revision[8];               // 23-26
    char model_number[40];                   // 27-46
    uint8_t maximum_block_transfer;          // 47
    uint8_t vendor_unique2;
    uint16_t double_word_io;                 // 48
    uint16_t capabilities1;                  // 49
    uint16_t capabilities2;                  // 50
    uint8_t vendor_unique3;                  // 51
    uint8_t pio_cycle_timing_mode;
    uint8_t vendor_unique4;                  // 52
    uint8_t dma_cycle_timing_mode;
    uint16_t bs;                             // 53
    uint16_t number_of_current_cylinders;    // 54
    uint16_t number_of_current_heads;        // 55
    uint16_t current_sectors_per_track;      // 56
    uint32_t current_sector_capacity;        // 57, 58
    uint16_t current_multi_sector_setting;   // 59
    uint32_t user_addressable_sectors;       // 60, 61
    uint16_t single_word_dma;                // 62
    uint16_t multi_word_dma;                 // 63
    uint8_t pio_mode;                        // 64
    uint8_t reserved1;
    uint16_t minimum_mwx_fer_cycle_time;     // 65
    uint16_t recommended_mwx_fer_cycle_time; // 66
    uint16_t minimum_pio_cycle_time;         // 67
    uint16_t minimum_pio_cycle_time_iordy;   // 68
    uint16_t reserved2[2];                   // 69, 70
    uint16_t release_time_overlapped;        // 71
    uint16_t release_time_service_command;   // 72
    uint16_t major_revision;                 // 73
    uint16_t minor_revision;                 // 74
    uint16_t reserved3;                      // 75
    uint16_t sata_capabilities;              // 76
    uint16_t reserved4;                      // 77
    uint16_t sata_features_suported;         // 78
    uint16_t sata_features_enabled;          // 79
    uint16_t major_version;                  // 80
    uint16_t minor_version;                  // 81
    uint16_t command_set_support1;           // 82
    uint16_t command_set_support2;           // 83
    uint16_t command_set_support3;           // 84
    uint16_t command_set_enabled1;           // 85
    uint16_t command_set_enabled2;           // 86
    uint16_t command_set_default;            // 87
    uint16_t ultra_dma_mode;                 // 88
    uint16_t reserved5;                      // 89
    uint16_t reserved6;                      // 90
    uint16_t reserved7;                      // 91
    uint16_t reserved8;                      // 92
    uint16_t reserved9;                      // 93
    uint16_t reserved10;                     // 94
    uint16_t reserved11;                     // 95
    uint16_t reserved12;                     // 96
    uint16_t reserved13;                     // 97
    uint32_t reserved14;                     // 98, 99
    uint64_t reserved15;                     // 100-103
    uint16_t reserved16;                     // 104
    uint16_t reserved17;                     // 105
    uint16_t reserved18;                     // 106
    uint16_t reserved19;                     // 107
    uint16_t reserved20;                     // 108
    uint16_t reserved21;                     // 109
    uint16_t reserved22;                     // 110
    uint16_t reserved23;                     // 111
    uint16_t reserved24[4];                  // 112-115
    uint16_t reserved25;                     // 116
    uint32_t reserved26;                     // 117, 118
    uint16_t reserved27[8];                  // 119-126
    uint16_t special_functions_enabled;      // 127
    uint16_t reserved28;                     // 128
    uint16_t reserved29[31];                 // 129-159
    uint16_t reserved30;                     // 160
    uint16_t reserved31[7];                  // 161-167
    uint16_t reserved32;                     // 168
    uint16_t data_set_management;            // 169
    uint16_t reserved33[4];                  // 170-173
    uint16_t reserved34[2];                  // 174, 175
    char reserved35[60];                     // 176-205
    uint16_t reserved36;                     // 206
    uint16_t reserved37[2];                  // 207, 208
    uint16_t reserved38;                     // 209
    uint32_t reserved39;                     // 210, 211
    uint32_t reserved40;                     // 212, 213
    uint16_t nv_cache_capabilities;          // 214
    uint32_t nv_cache_logical_blocks_size;   // 215, 216
    uint16_t rotation_rate;                  // 217
    uint16_t reserved41;                     // 218
    uint16_t nv_cache_options1;              // 219
    uint16_t nv_cache_options2;              // 220
    uint16_t reserved42;                     // 221
    uint16_t reserved43;                     // 222
    uint16_t reserved44;                     // 223
    uint16_t reserved45[10];                 // 224-233
    uint16_t reserved46;                     // 234
    uint16_t reserved47;                     // 235
    uint16_t reserved48[19];                 // 236-254
    uint16_t reserved49;                     // 255
};
#pragma pack (pop, drive_identify_data)

//
// Generic Functions
//

bool OpenDrive(Device& device, uint8_t device_number);
STORAGE_BUS_TYPE GetDriveBusType(Device& device);
bool GetDriveNumber(Device& device, uint8_t& device_number);
bool GetDriveGeometry(Device& device, DISK_GEOMETRY& geometry);

//
// Physical Drive Functions (PD)
//

std::unique_ptr<DriveIdentifyData> GetDriveIdentifyDataPD(Device& device, uint8_t device_number);
bool EnableSmartPD(Device& device, uint8_t device_number);
bool GetSmartAttributesPD(Device& device, uint8_t device_number, SmartAttributeData& data);
bool GetSmartThresholdsPD(Device& device, uint8_t device_number, SmartThresholdData& data);

//
// SCSI-ATA Translation Functions (SAT)
//

std::unique_ptr<DriveIdentifyData> GetDriveIdentifyDataSAT(Device& device, uint8_t device_number);
bool EnableSmartSAT(Device& device, uint8_t device_number);
bool GetSmartAttributesSAT(Device& device, uint8_t device_number, SmartAttributeData& data);
bool GetSmartThresholdsSAT(Device& device, uint8_t device_number, SmartThresholdData& data);

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__PHYSICAL_DRIVE_UTIL_H
