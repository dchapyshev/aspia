//
// PROJECT:         Aspia
// FILE:            base/devices/device_spti.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DEVICES__DEVICE_SPTI_H
#define _ASPIA_BASE__DEVICES__DEVICE_SPTI_H

#include "base/devices/device.h"

namespace aspia {

class DeviceSPTI : public Device
{
public:
    DeviceSPTI() = default;
    virtual ~DeviceSPTI() = default;

    enum Operation : uint8_t
    {
        OP_INQUIRY           = 0x12,
        OP_MODE_SENSE_6      = 0x1A,
        OP_GET_CONFIGURATION = 0x46,
        OP_MODE_SENSE_10     = 0x5A,
        OP_REPORT_KEY        = 0xA4
    };

#pragma pack(push, inquiry_data, 1)
    typedef struct
    {
        uint8_t device_type : 5;
        uint8_t device_type_qualifier : 3;
        uint8_t device_type_modifier : 7;
        uint8_t removable_media : 1;
        uint8_t versions;
        uint8_t response_data_format : 4;
        uint8_t hi_support : 1;
        uint8_t norm_aca : 1;
        uint8_t terminate_task : 1;
        uint8_t aerc : 1;
        uint8_t additional_length;
        uint8_t reserved;
        uint8_t addr16 : 1;
        uint8_t addr32 : 1;
        uint8_t ack_req_q: 1;
        uint8_t medium_changer : 1;
        uint8_t multi_port : 1;
        uint8_t reserved_bit2 : 1;
        uint8_t enclosure_services : 1;
        uint8_t reserved_bit3 : 1;
        uint8_t soft_reset : 1;
        uint8_t command_queue : 1;
        uint8_t transfer_disable : 1;
        uint8_t linked_commands : 1;
        uint8_t synchronous : 1;
        uint8_t wide16bit : 1;
        uint8_t wide32bit : 1;
        uint8_t relative_addressing : 1;
        uint8_t vendor_id[8];
        uint8_t product_id[16];
        uint8_t product_revision_level[4];
        uint8_t vendor_specific[20];
        uint8_t reserved3[40];
    } InquiryData;
#pragma pack(pop, inquiry_data)

    enum FeatureCode : uint16_t
    {
        FEATURE_CODE_PROFILE_LIST                   = 0x0000,
        FEATURE_CODE_CORE                           = 0x0001,
        FEATURE_CODE_MORPHING                       = 0x0002,
        FEATURE_CODE_REMOVABLE_MEDIUM               = 0x0003,
        FEATURE_CODE_WRITE_PROTECT                  = 0x0004,
        FEATURE_CODE_RANDOM_READABLE                = 0x0010,
        FEATURE_CODE_MULTI_READ                     = 0x001D,
        FEATURE_CODE_CD_READ                        = 0x001E,
        FEATURE_CODE_DVD_READ                       = 0x001F,
        FEATURE_CODE_RANDOM_WRITABLE                = 0x0020,
        FEATURE_CODE_INCREMENTAL_STREAMING_WRITABLE = 0x0021,
        FEATURE_CODE_SECTOR_ERASABLE                = 0x0022,
        FEATURE_CODE_FORMATTABLE                    = 0x0023,
        FEATURE_CODE_DEFECT_MANAGEMENT              = 0x0024,
        FEATURE_CODE_WRITE_ONCE                     = 0x0025,
        FEATURE_CODE_RESTRICTED_OVERWRITE           = 0x0026,
        FEATURE_CODE_CD_RW_CAV_WRITE                = 0x0027,
        FEATURE_CODE_MRW                            = 0x0028,
        FEATURE_CODE_ENHANCED_DEFECT_REPORTING      = 0x0029,
        FEATURE_CODE_DVD_PLUS_RW                    = 0x002A,
        FEATURE_CODE_DVD_PLUS_R                     = 0x002B,
        FEATURE_CODE_RIGID_RESTRICTED_OVERWRITE     = 0x002C,
        FEATURE_CODE_CD_TRACK_AT_ONCE               = 0x002D,
        FEATURE_CODE_CD_MASTERING                   = 0x002E,
        FEATURE_CODE_DVD_R_RW_WRITE                 = 0x002F,
        FEATURE_CODE_DDCD_READ                      = 0x0030,
        FEATURE_CODE_DDCD_R_WRITE                   = 0x0031,
        FEATURE_CODE_DDCD_RW_WRITE                  = 0x0032,
        FEATURE_CODE_LAYER_JUMP_RECORDING           = 0x0033,
        FEATURE_CODE_CD_RW_MEDIA_WRITE              = 0x0037,
        FEATURE_CODE_BD_R_POW                       = 0x0038,
        FEATURE_CODE_DVD_PLUS_RW_DUAL_LAYER         = 0x003A,
        FEATURE_CODE_DVD_PLUS_R_DUAL_LAYER          = 0x003B,
        FEATURE_CODE_BD_READ                        = 0x0040,
        FEATURE_CODE_BD_WRITE                       = 0x0041,
        FEATURE_CODE_HD_DVD_READ                    = 0x0050,
        FEATURE_CODE_HD_DVD_WRITE                   = 0x0051,
        FEATURE_CODE_HYBRID_DISK                    = 0x0080,
        FEATURE_CODE_POWER_MANAGEMENT               = 0x0100,
        FEATURE_CODE_SMART                          = 0x0101,
        FEATURE_CODE_EMBEDDED_CHANGER               = 0x0102,
        FEATURE_CODE_CD_AUDIO_ANALOG_PLAY           = 0x0103,
        FEATURE_CODE_MICROCODE_UPGRADE              = 0x0104,
        FEATURE_CODE_TIMEOUT                        = 0x0105,
        FEATURE_CODE_DVD_CSS                        = 0x0106,
        FEATURE_CODE_REAL_TIME_STREAMING            = 0x0107,
        FEATURE_CODE_LOGICAL_UNIT_SERIAL_NUMBER     = 0x0108,
        FEATURE_CODE_DISK_CONTROL_BLOCKS            = 0x010A,
        FEATURE_CODE_DVD_CPRM                       = 0x010B,
        FEATURE_CODE_FIRMWARE_INFORMATION           = 0x010C,
        FEATURE_CODE_AACS                           = 0x010D,
        FEATURE_CODE_VCPS                           = 0x0110
    };

    enum PhysicalInterfaceStandard : uint32_t
    {
        PHYSICAL_INTERFACE_STANDARD_UNSPECIFIED    = 0x00000000,
        PHYSICAL_INTERFACE_STANDARD_SCSI_FAMILY    = 0x00000001,
        PHYSICAL_INTERFACE_STANDARD_ATAPI          = 0x00000002,
        PHYSICAL_INTERFACE_STANDARD_IEEE_1394_1995 = 0x00000003,
        PHYSICAL_INTERFACE_STANDARD_IEEE_1394A     = 0x00000004,
        PHYSICAL_INTERFACE_STANDARD_FIBRE_CHANNEL  = 0x00000005,
        PHYSICAL_INTERFACE_STANDARD_IEEE_1394B     = 0x00000006,
        PHYSICAL_INTERFACE_STANDARD_SERIAL_ATAPI   = 0x00000007,
        PHYSICAL_INTERFACE_STANDARD_USB            = 0x00000008
    };

    enum Profile : uint16_t
    {
        PROFILE_CD_ROM                                = 0x0008,
        PROFILE_CD_R                                  = 0x0009,
        PROFILE_CD_RW                                 = 0x000A,
        PROFILE_DVD_ROM                               = 0x0010,
        PROFILE_DVD_R_SEQUENTIAL_RECORDING            = 0x0011,
        PROFILE_DVD_RAM                               = 0x0012,
        PROFILE_DVD_RW_RESTRICTED_OVERWRITE           = 0x0013,
        PROFILE_DVD_RW_SEQUENTIAL_RECORDING           = 0x0014,
        PROFILE_DVD_R_DUAL_LAYER_SEQUENTIAL_RECORDING = 0x0015,
        PROFILE_DVD_R_DUAL_LAYER_JUMP_RECORDING       = 0x0016,
        PROFILE_DVD_RW_DUAL_LAYER                     = 0x0017,
        PROFILE_DVD_DOWNLOAD_DISK_RECORDING           = 0x0018,
        PROFILE_DVD_PLUS_RW                           = 0x001A,
        PROFILE_DVD_PLUS_R                            = 0x001B,
        PROFILE_DVD_PLUS_RW_DUAL_LAYER                = 0x002A,
        PROFILE_DVD_PLUS_R_DUAL_LAYER                 = 0x002B,
        PROFILE_BD_ROM                                = 0x0040,
        PROFILE_BD_R                                  = 0x0042,
        PROFILE_BD_RE                                 = 0x0043,
        PROFILE_HD_DVD_ROM                            = 0x0050,
        PROFILE_HD_DVD_R                              = 0x0051,
        PROFILE_HD_DVD_RAM                            = 0x0052
    };

    typedef struct
    {
        uint32_t data_length;
        uint8_t reserved1;
        uint8_t reserved2;
        uint16_t current_profile;
    } FeatureHeader;

    static_assert(sizeof(FeatureHeader) == 8);

#pragma pack(push, feature, 1)
    typedef struct
    {
        FeatureHeader header;

        uint16_t feature_code;
        uint8_t current : 1;
        uint8_t persistent : 1;
        uint8_t version : 4;
        uint8_t reserved1 : 2;
        uint8_t additional_length;

        union
        {
            uint8_t additional_data[102];

            // Feature code = 0x0001
            struct CoreFeature
            {
                PhysicalInterfaceStandard physical_interface_standard;
                uint8_t dbe : 1;
                uint8_t reserved1 : 7;
                uint8_t reserved2;
                uint8_t reserved3;
                uint8_t reserved4;
            } core;

            // Feature code = 0x001E
            struct CDReadFeature
            {
                uint8_t cd_text : 1;
                uint8_t c2_flags : 1;
                uint8_t reserved1 : 5;
                uint8_t dap : 1;

                uint8_t reserved2;
                uint8_t reserved3;
                uint8_t reserved4;
            } cd_read;

            // Feature code = 0x1F (MMC-6)
            struct DVDReadFeature
            {
                uint8_t multi110 : 1;
                uint8_t reserved1 : 7;
                uint8_t reserved2;
                uint8_t dual_r : 1;
                uint8_t dual_rw : 1;
                uint8_t reserved3 : 6;
            } dvd_read;

            // Feature code = 0x002A
            struct DVDPlusRW
            {
                uint8_t write : 1;
                uint8_t reserved1 : 7;
                uint8_t close_only : 1;
                uint8_t quick_start : 1;
                uint8_t reserved2 : 6;
                uint8_t reserved3[2];
            } dvd_plus_rw;

            // Feature code = 0x002B
            struct DVDPlusR
            {
                uint8_t write : 1;
                uint8_t reserved1 : 7;
                uint8_t reserved2[3];
            } dvd_plus_r;

            // Feature code = 0x002F
            struct DVDRRWWrite
            {
                uint8_t reserved1 : 1;
                uint8_t dvd_rw : 1;
                uint8_t test_write : 1;
                uint8_t rdl : 1;
                uint8_t reserved2 : 2;
                uint8_t buf : 1;
                uint8_t reserved3 : 1;
                uint8_t reserved4[3];
            } dvd_r_rw_write;

            // Feature code = 0x003A
            struct DVDPlusRWDualLayer
            {
                uint8_t write : 1;
                uint8_t reserved1 : 7;
                uint8_t close_only : 1;
                uint8_t quick_start : 1;
                uint8_t reserved2 : 6;
                uint8_t reserved3[2];
            } dvd_plus_rw_dual_layer;

            // Feature code = 0x003B
            struct DVDPlusRDualLayer
            {
                uint8_t write : 1;
                uint8_t reserved1 : 7;
                uint8_t reserved2[3];
            } dvd_plus_r_dual_layer;

            // Feature code = 0x0040 (MMC-5)
            struct BDReadFeature
            {
                uint8_t reserved[4];
                uint64_t re;
                uint64_t r;
                uint64_t rom;
            } bd_read;

            // Feature code = 0x0041 (MMC-5)
            struct BDWriteFeature
            {
                uint8_t reserved[4];
                uint64_t re;
                uint64_t r;
            } bd_write;

            // Feature code = 0x0050 (MMC-5)
            struct HDDVDRead
            {
                uint8_t hd_dvd_r : 1;
                uint8_t reserved1 : 7;
                uint8_t reserved2;
                uint8_t hd_dvd_ram : 1;
                uint8_t reserved3 : 7;
                uint8_t reserved4;
            } hd_dvd_read;

            // Feature code = 0x0051 (MMC-5)
            struct HDDVDWrite
            {
                uint8_t hd_dvd_r : 1;
                uint8_t reserved1 : 7;
                uint8_t reserved2;
                uint8_t hd_dvd_ram : 1;
                uint8_t reserved3 : 7;
                uint8_t reserved4;
            } hd_dvd_write;
        } table;
    } Feature;
#pragma pack(pop, feature)

#pragma pack(push, report_key_data, 1)
    typedef struct
    {
        uint8_t reserved1[4];
        uint8_t user_changes: 3;
        uint8_t vendor_resets: 3;
        uint8_t type_code: 2;
        uint8_t region_mask;
        uint8_t rpc_scheme;
        uint8_t reserved2;
    } ReportKeyData;
#pragma pack(pop, report_key_data)

#pragma pack(push, mode_sense_parameter_header, 1)
    typedef struct
    {
        uint16_t mode_data_length;  // Byte 0, 1.
        uint8_t reserved[4];        // Byte 2-5.
        uint16_t block_desc_length; // Byte 6, 7.
    } ModeSenseParameterHeader;
#pragma pack(pop, mode_sense_parameter_header)

    static_assert(sizeof(ModeSenseParameterHeader) == 8);

#pragma pack(push, mode_sense_page_2a, 1)
    typedef struct
    {
        uint8_t page_code : 6;                  // Byte 0.
        uint8_t reserved1 : 1;                  //
        uint8_t ps : 1;                         //
        uint8_t page_length;                    // Byte 1. 0x14 = MMC, 0x18 = MMC-2, >=0x1C = MMC-3
        uint8_t cd_r_read : 1;                  // Byte 2. CD-R media reading.
        uint8_t cd_rw_read : 1;                 //         CD-RW media reading.
        uint8_t method2 : 1;                    //         Fixed packet method2.
        uint8_t dvd_rom_read : 1;               //         DVD-ROM media reading.
        uint8_t dvd_r_read : 1;                 //         DVD-R media reading.
        uint8_t dvd_ram_read : 1;               //         DVD-RAM media reading.
        uint8_t reserved2 : 2;                  //         Reserved.
        uint8_t cd_r_write : 1;                 // Byte 3. CD-R media writing.
        uint8_t cd_rw_write : 1;                //         CD-RW media writing.
        uint8_t test_write : 1;                 //         Emulation write.
        uint8_t reserved3 : 1;                  //         Reserved.
        uint8_t dvd_r_write : 1;                //         DVD-R media writing.
        uint8_t dvd_ram_write : 1;              //         DVD-RAM media writing.
        uint8_t reserved4 : 2;                  //         Reserved.
        uint8_t audio_play : 1;                 // Byte 4. Audio play operation.
        uint8_t composite : 1;                  //         Composite A/V stream.
        uint8_t digital_port_2 : 1;             //         Digital output on port 2.
        uint8_t digital_port_1 : 1;             //         Digital output on port 1.
        uint8_t mode_2_form_1 : 1;              //         Mode-2 form 1 media.
        uint8_t mode_2_form_2 : 1;              //         Mode-2 form 2 media.
        uint8_t multi_session : 1;              //         Multi-session media.
        uint8_t reserved5 : 1;                  //         Reserved.
        uint8_t cdda_supported : 1;             // Byte 5.
        uint8_t cdda_accurate : 1;              //
        uint8_t rw_supported : 1;               //         R-W sub channel information.
        uint8_t rw_deinterleaved_corrected : 1; //
        uint8_t c2_pointers : 1;                //         Supports C2 error pointers.
        uint8_t isrc : 1;                       //         ISRC information.
        uint8_t upc : 1;                        //         Media catalog number.
        uint8_t read_bar_code : 1;              //         Reading bar codes.
        uint8_t lock : 1;                       // Byte 6. PREVENT/ALLOW may lock media.
        uint8_t lock_state : 1;                 //         0 = unlocked, 1 = locked.
        uint8_t prevent_jumper : 1;             //         State of jumper.
        uint8_t eject : 1;                      //         Ejects disc.
        uint8_t reserved6 : 1;                  //         Reserved.
        uint8_t loading_type : 3;               //         Loading mechanism type.
        uint8_t sep_vol_levels_per_channel : 1; // Byte 7. Separate volume levels per channel.
        uint8_t sep_channel_mute_supported : 1; //         Separate channel mute control.
        uint8_t disk_present_rep : 1;           //         Changer supports disk present rep.
        uint8_t sw_slot_selection : 1;          //         Load empty slot in changer.
        uint8_t side_change_capable : 1;        //         Side change capable.
        uint8_t pw_in_lead_in : 1;              //         Reads raw P-W sucode from lead in.
        uint8_t reserved7 : 2;                  //         Reserved.
        uint16_t max_read_speed_supported;      // Byte 8, 9. Max. read speed in KB/s.
        uint16_t num_volume_levels_supported;   // Byte 10, 11. Number of supported volume levels.
        uint16_t buffer_size;                   // Byte 12, 13. Buffer size for the data in KB.
        uint16_t current_read_speed;            // Byte 14, 15. Current read speed in KB/s.
        uint8_t reserved8;                      // Byte 16. Reserved.
        uint8_t reserved9 : 1;                  // Byte 17. Reserved.
        uint8_t bck : 1;                        //         Data valid on falling edge of BCK
        uint8_t rck : 1;                        //
        uint8_t lsbf : 1;                       //
        uint8_t length : 2;                     //         0=32BCKs 1=16BCKs 2=24BCKs 3=24I2c
        uint8_t reserved10 : 2;                 //         Reserved.
        uint16_t max_write_speed_supported;     // Byte 18, 19. Max. write speed supported in KB/s.
        uint16_t cur_write_speed;               // Byte 20, 21. Current write speed in KB/s.
    } ModeSensePage2A;
#pragma pack(pop, mode_sense_page_2a)

    static_assert(sizeof(ModeSensePage2A) == 22);

    enum ModeSensePage : uint8_t
    {
        // 0x00 - Vendor-specific (does not require page format)
        MODE_SENSE_PAGE_02 = 0x01, // Read/Write error recovery page.
        // 0x02 - 0x04 - Reserved.
        MODE_SENSE_PAGE_05 = 0x05, // Write Parameter page.
        // 0x06 - Reserved.
        MODE_SENSE_PAGE_07 = 0x07, // Verify error recovery page.
        // 0x08 - 0x0A - Reserved.
        MODE_SENSE_PAGE_0B = 0x0B, // Medium types supported page.
        // 0x0C - Reserved.
        MODE_SENSE_PAGE_0D = 0x0D, // CD Device Parameters Page.
        MODE_SENSE_PAGE_0E = 0x0E, // CD audio control page.
        // 0x0F - 0x19 - Reserved.
        MODE_SENSE_PAGE_1A = 0x1A, // Power Condition Page.
        // 0x1B - Reserved.
        MODE_SENSE_PAGE_1C = 0x1C, //  Fault/Failure Reporting Page.
        MODE_SENSE_PAGE_1D = 0x1D, // Time-out & Protect Page.
        // 0x1E - 0x1F - Reserved.
        // 0x20 - 0x29 - Vendor Specific.
        MODE_SENSE_PAGE_2A = 0x2A, // C/DVD Capabilities & Mechanical Status Page.
        // 0x2B - 0x3E - Vendor Specific.
        MODE_SENSE_PAGE_3F = 0x3F // Return all pages (valid only for the Mode Sense command)
    };

    typedef struct
    {
        ModeSenseParameterHeader header;

        union ModeSensePage
        {
            uint8_t raw[504];
            ModeSensePage2A page_2A;
        } page;
    } ModeSenseData;

    bool GetInquiryData(InquiryData* inquiry_data);
    bool GetConfiguration(FeatureCode feature_code, Feature* feature);
    bool GetReportKeyData(ReportKeyData* report_key_data);
    bool GetModeSenseData(uint8_t page, ModeSenseData* mode_sense_data);
};

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__DEVICE_SPTI_H
