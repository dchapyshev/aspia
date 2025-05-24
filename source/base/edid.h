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

#ifndef BASE_EDID_H
#define BASE_EDID_H

#include <QtGlobal>

#include "base/macros_magic.h"

#include <memory>

namespace base {

class Edid
{
public:
    static std::unique_ptr<Edid> create(std::unique_ptr<quint8[]> data, size_t data_size);

    ~Edid() = default;

    enum InputSignalType
    {
        INPUT_SIGNAL_TYPE_ANALOG  = 0,
        INPUT_SIGNAL_TYPE_DIGITAL = 1
    };

    enum DataTypeTag : quint8
    {
        // 0Fh - 00h: Descriptor defined by manufacturer.
        // 10h: Dummy descriptor, used to indicate that the descriptor space is unused.
        // F9h - 11h: Currently undefined.

        // FAh: Descriptor contains additional Standard Timing Identifications.
        DATA_TYPE_TAG_STANDARD_TIMING_IDENTIFIERS = 0xFA,

        // FBh: Descriptor contains additional color point data.
        DATA_TYPE_TAG_COLOR_POINT = 0xFB,

        // FCh: Monitor name, stored as ASCII, code page # 437.
        DATA_TYPE_TAG_MONITOR_NAME_ASCII = 0xFC,

        // FDh: Monitor range limits, binary coded.
        DATA_TYPE_TAG_MINITOR_RANGE_LIMITS = 0xFD,

        // FEh: ASCII String - Stored as ASCII, code page # 437, ≤ 13 bytes.
        DATA_TYPE_TAG_ASCII_DATA_STRING = 0xFE,

        // FFh: Monitor Serial Number - Stored as ASCII, code page # 437, ≤ 13 bytes.
        DATA_TYPE_TAG_MONITOR_SERIAL_NUMBER_ASCII = 0xFF
    };

    struct MonitorDescriptor
    {
        // Flag = 0000h when block used as descriptor.
        quint16 flag;

        // Reserved = 00h when block used as descriptor.
        quint8 reserved1;

        // Data Type Tag (Binary coded).
        DataTypeTag data_type_tag;

        // 00h when block used as descriptor.
        quint8 reserved2;

        // Definition dependent on data type tag chosen.
        quint8 descriptor_data[13];
    };

    struct DetailedTimingDescriptor
    {
        quint16 pixel_clock;
        quint8 horizontal_active;
        quint8 horizontal_blanking;
        quint8 horizontal_active_blanking;
        quint8 vertical_active;
        quint8 vertical_blanking;
        quint8 vertical_active_blanking;
        quint8 horizontal_sync_offset;
        quint8 horizontal_sync_pulse_width;
        quint8 vertical_sync_offset;
        quint8 vertical_sync_pulse_width;

        // mm, lower 8 bits
        quint8 horizontal_image_size;

        // mm, lower 8 bits
        quint8 vertical_image_size;

        // Upper nibble : upper 4 bits of Horizontal Image Size
        // Lower nibble : upper 4 bits of Vertical Image Size
        quint8 horizontal_vertical_image_size;

        // Pixels
        quint8 horizontal_border;
        quint8 vertical_border;

        quint8 flags;
    };

    struct Data
    {
        // Header (8 bytes)
        quint64 header;                            // 00h-07h

        // Vendor / Product Identification (10 bytes)
        quint16 id_manufacturer_name;              // 08h-09h
        quint16 id_product_code;                   // 0Ah-0Bh
        quint32 id_serial_number;                  // 0Ch-0Fh
        quint8 week_of_manufacture;                // 10h
        quint8 year_of_manufacture;                // 11h

        // EDID Structure Version / Revision (2 bytes)
        quint8 structure_version;                  // 12h
        quint8 structure_revision;                 // 13h

        // Basic Display Parameters / Features (5 bytes)
        quint8 video_input_definition;             // 14h
        quint8 max_horizontal_image_size;          // 15h
        quint8 max_vertical_image_size;            // 16h
        quint8 gamma;                              // 17h
        quint8 feature_support;                    // 18h

        // Color Characteristics (10 bytes)
        quint8 red_green_low_bits;                 // 19h
        quint8 blue_white_low_bits;                // 1Ah
        quint8 red_x;                              // 1Bh
        quint8 red_y;                              // 1Ch
        quint8 green_x;                            // 1Dh
        quint8 green_y;                            // 1Eh
        quint8 blue_x;                             // 1Fh
        quint8 blue_y;                             // 20h
        quint8 white_x;                            // 21h
        quint8 white_y;                            // 22h

        // 23h-24h: Established Timings (3 bytes)
        quint8 established_timings[2];
        quint8 manufacturers_reserved_timings;     // 25h

        // 26h-35h: Standard Timing Identification (16 bytes)
        quint8 standard_timing_identification[8][2];

        // 36h-7Dh: Detailed Timing Descriptions (72 bytes)
        quint8 detailed_timing_description[4][18];

        // 7Eh: Indicates the number of (optional) Extension EDID blocks to follow.
        quint8 extension_flag;

        // 7Fh: This byte should be programmed such that a one - byte checksum of the
        // entire 128 - byte EDID equals 00h.
        quint8 checksum;
    };

    enum FeatureSupport
    {
        FEATURE_SUPPORT_DEFAULT_GTF_SUPPORTED = 0x01,
        FEATURE_SUPPORT_PREFERRED_TIMING_MODE = 0x02,
        FEATURE_SUPPORT_SRGB                  = 0x04,
        FEATURE_SUPPORT_ACTIVE_OFF            = 0x20,
        FEATURE_SUPPORT_SUSPEND               = 0x40,
        FEATURE_SUPPORT_STANDBY               = 0x80
    };

    enum EstablishedTimings1
    {
        ESTABLISHED_TIMINGS_1_800X600_60HZ = 0x01,
        ESTABLISHED_TIMINGS_1_800X600_56HZ = 0x02,
        ESTABLISHED_TIMINGS_1_640X480_75HZ = 0x04,
        ESTABLISHED_TIMINGS_1_640X480_72HZ = 0x08,
        ESTABLISHED_TIMINGS_1_640X480_67HZ = 0x10,
        ESTABLISHED_TIMINGS_1_640X480_60HZ = 0x20,
        ESTABLISHED_TIMINGS_1_720X400_88HZ = 0x40,
        ESTABLISHED_TIMINGS_1_720X400_70HZ = 0x80
    };

    enum EstablishedTimings2
    {
        ESTABLISHED_TIMINGS_2_1280X1024_75HZ = 0x01,
        ESTABLISHED_TIMINGS_2_1024X768_75HZ  = 0x02,
        ESTABLISHED_TIMINGS_2_1024X768_70HZ  = 0x04,
        ESTABLISHED_TIMINGS_2_1024X768_60HZ  = 0x08,
        ESTABLISHED_TIMINGS_2_1024X768_87HZ  = 0x10,
        ESTABLISHED_TIMINGS_2_832X624_75HZ   = 0x20,
        ESTABLISHED_TIMINGS_2_800X600_75HZ   = 0x40,
        ESTABLISHED_TIMINGS_2_800X600_72HZ   = 0x80
    };

    enum ManufacturersTimings
    {
        // Bits 6-0 reserved.
        MANUFACTURERS_TIMINGS_1152X870_75HZ = 0x80
    };

    QString manufacturerName() const;
    QString monitorName() const;
    QString monitorId() const;
    QString serialNumber() const;
    int weekOfManufacture() const;
    int yearOfManufacture() const;
    int edidVersion() const;
    int edidRevision() const;
    int maxHorizontalImageSize() const; // in cm.
    int maxVerticalImageSize() const; // in cm.
    int horizontalResolution() const;
    int verticalResolution() const;
    double gamma() const;
    quint8 featureSupport() const;
    int minVerticalRate() const; // Hz
    int maxVerticalRate() const; // Hz
    int minHorizontalRate() const; // kHz
    int maxHorizontalRate() const; // kHz
    double pixelClock() const; // MHz
    int maxSupportedPixelClock() const; // MHz

    InputSignalType inputSignalType() const;
    quint8 estabilishedTimings1() const;
    quint8 estabilishedTimings2() const;
    quint8 manufacturersTimings() const;
    int standardTimingsCount() const;
    bool standardTimings(int index, int* width, int* height, int* frequency);

private:
    Edid(std::unique_ptr<quint8[]> data, size_t data_size);

    QString getManufacturerSignature() const;
    quint8* getDescriptor(int type) const;

    std::unique_ptr<quint8[]> data_;
    const size_t data_size_;
    Data* edid_;

    DISALLOW_COPY_AND_ASSIGN(Edid);
};

} // namespace base

#endif // BASE_EDID_H
