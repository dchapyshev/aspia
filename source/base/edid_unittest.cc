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

#include "base/edid.h"

#include <QString>

#include <gtest/gtest.h>

namespace base {

namespace {

// Helper to create a minimal valid EDID block (128 bytes) with correct header and checksum.
// The caller can modify fields before passing to Edid constructor.
QByteArray createValidEdidBlock()
{
    QByteArray data(128, 0);
    quint8* raw = reinterpret_cast<quint8*>(data.data());

    // EDID header: 00 FF FF FF FF FF FF 00
    raw[0] = 0x00;
    raw[1] = 0xFF;
    raw[2] = 0xFF;
    raw[3] = 0xFF;
    raw[4] = 0xFF;
    raw[5] = 0xFF;
    raw[6] = 0xFF;
    raw[7] = 0x00;

    // Manufacturer ID: "DEL" (Dell)
    // D=4, E=5, L=12
    // Value = (4 << 10) | (5 << 5) | 12 = 0x10AC
    // EDID stores big-endian: raw[8] = 0x10, raw[9] = 0xAC
    raw[8] = 0x10;  // high byte (big endian)
    raw[9] = 0xAC;  // low byte

    // Product code (little-endian): 0x1234
    raw[10] = 0x34;
    raw[11] = 0x12;

    // Serial number (little-endian): 0x00000001
    raw[12] = 0x01;
    raw[13] = 0x00;
    raw[14] = 0x00;
    raw[15] = 0x00;

    // Week of manufacture: 10
    raw[16] = 10;

    // Year of manufacture: 30 (=> 1990 + 30 = 2020)
    raw[17] = 30;

    // EDID version 1, revision 3
    raw[18] = 1;
    raw[19] = 3;

    // Video input definition: digital (bit 7 set)
    raw[20] = 0x80;

    // Max horizontal image size: 53 cm
    raw[21] = 53;

    // Max vertical image size: 30 cm
    raw[22] = 30;

    // Gamma: 220 => (220/100) + 1.0 = 3.2
    raw[23] = 220;

    // Feature support
    raw[24] = 0x06; // PREFERRED_TIMING_MODE | SRGB

    // Color characteristics (bytes 25-34) - leave as zeros.

    // Established timings 1 (byte 35)
    raw[35] = 0x21; // 640x480@60Hz | 800x600@60Hz

    // Established timings 2 (byte 36)
    raw[36] = 0x08; // 1024x768@60Hz

    // Manufacturers reserved timings (byte 37)
    raw[37] = 0x00;

    // Standard timing identification (bytes 38-53) - set all to 0x0101 (unused).
    for (int i = 38; i < 54; i += 2)
    {
        raw[i] = 0x01;
        raw[i + 1] = 0x01;
    }

    // First standard timing: 1280x1024@60Hz
    // width = (byte1 + 31) * 8 => byte1 = 1280/8 - 31 = 129
    // aspect = 5:4 => bits 7:6 = 0x02, frequency = 60 - 60 = 0 => byte2 = 0x80
    raw[38] = 129;
    raw[39] = 0x80;

    // Detailed timing descriptor #1 (bytes 54-71): a real timing block.
    // Pixel clock: 14850 (= 148.50 MHz), little-endian
    raw[54] = 0x02;
    raw[55] = 0x3A;
    // Horizontal active: lower 8 bits = 0x80 (lo=128)
    raw[56] = 0x80; // horizontal_active (lower 8 bits = 1920 & 0xFF = 0x80)
    // Horizontal blanking: lower 8 bits
    raw[57] = 0x18; // horizontal_blanking
    // Upper nibble: upper 4 bits of h_active, lower nibble: upper 4 bits of h_blanking
    // 1920 = 0x780 => upper 4 bits = 0x7
    // h_blanking = 0x118 => upper 4 bits = 0x1
    raw[58] = 0x71; // (0x7 << 4) | 0x1
    // Vertical active: lower 8 bits of 1080 = 0x38
    raw[59] = 0x38; // vertical_active
    // Vertical blanking
    raw[60] = 0x2D;
    // Upper nibble: upper 4 bits of v_active, lower nibble: upper 4 bits of v_blanking
    // 1080 = 0x438 => upper 4 bits = 0x4
    raw[61] = 0x40; // (0x4 << 4) | 0x0
    // Remaining bytes of detailed timing (sync, image size, etc.)
    raw[62] = 0x58; // h sync offset
    raw[63] = 0x2C; // h sync pulse width
    raw[64] = 0x45; // v sync (lower nibble offset, upper nibble pulse width)
    raw[65] = 0x00; // combined sync
    raw[66] = 0x09; // horizontal_image_size lower 8 bits (example: 521mm = 0x209)
    raw[67] = 0x25; // vertical_image_size lower 8 bits (example: 293mm = 0x125)
    raw[68] = 0x21; // upper nibble: upper 4 bits of h_image_size, lower: upper 4 bits of v_image_size
    raw[69] = 0x00; // h border
    raw[70] = 0x00; // v border
    raw[71] = 0x1E; // flags

    // Detailed timing descriptor #2 (bytes 72-89): Monitor range limits descriptor
    raw[72] = 0x00; // flag = 0x0000
    raw[73] = 0x00;
    raw[74] = 0x00; // reserved1
    raw[75] = 0xFD; // data_type_tag = Monitor Range Limits
    raw[76] = 0x00; // reserved2
    // descriptor_data[0..4]: min_v_rate, max_v_rate, min_h_rate, max_h_rate, max_pixel_clock
    raw[77] = 56;   // min vertical rate: 56 Hz
    raw[78] = 76;   // max vertical rate: 76 Hz
    raw[79] = 30;   // min horizontal rate: 30 kHz
    raw[80] = 81;   // max horizontal rate: 81 kHz
    raw[81] = 17;   // max supported pixel clock: 17 * 10 = 170 MHz
    // Remaining bytes of this descriptor can be zero.

    // Detailed timing descriptor #3 (bytes 90-107): Monitor name descriptor
    raw[90] = 0x00;  // flag = 0x0000
    raw[91] = 0x00;
    raw[92] = 0x00;  // reserved1
    raw[93] = 0xFC;  // data_type_tag = Monitor Name ASCII
    raw[94] = 0x00;  // reserved2
    // "Test Monitor" + 0x0A terminator
    const char* name = "Test Monitor";
    for (int i = 0; i < 12; ++i)
        raw[95 + i] = static_cast<quint8>(name[i]);
    raw[107] = 0x0A; // terminator

    // Detailed timing descriptor #4 (bytes 108-125): Serial number descriptor
    raw[108] = 0x00; // flag = 0x0000
    raw[109] = 0x00;
    raw[110] = 0x00; // reserved1
    raw[111] = 0xFF; // data_type_tag = Monitor Serial Number ASCII
    raw[112] = 0x00; // reserved2
    // "ABC1234567" + 0x0A terminator
    const char* sn = "ABC1234567";
    for (int i = 0; i < 10; ++i)
        raw[113 + i] = static_cast<quint8>(sn[i]);
    raw[123] = 0x0A; // terminator

    // Extension flag (byte 126)
    raw[126] = 0;

    // Compute checksum so that sum of all 128 bytes == 0 (mod 256).
    quint8 sum = 0;
    for (int i = 0; i < 127; ++i)
        sum += raw[i];
    raw[127] = static_cast<quint8>(0x100 - sum);

    return data;
}

} // namespace

TEST(edid_test, empty_buffer)
{
    Edid edid{QByteArray()};
    EXPECT_FALSE(edid.isValid());
}

TEST(edid_test, too_short_buffer)
{
    QByteArray data(64, 0);
    Edid edid(data);
    EXPECT_FALSE(edid.isValid());
}

TEST(edid_test, invalid_header)
{
    QByteArray data(128, 0);
    // All zeros - wrong header.
    Edid edid(data);
    EXPECT_FALSE(edid.isValid());
}

TEST(edid_test, invalid_checksum)
{
    QByteArray data = createValidEdidBlock();
    // Corrupt the checksum.
    data[127] = static_cast<char>(data[127] + 1);
    Edid edid(data);
    EXPECT_FALSE(edid.isValid());
}

TEST(edid_test, valid_edid)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    EXPECT_TRUE(edid.isValid());
}

TEST(edid_test, year_of_manufacture)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_EQ(edid.yearOfManufacture(), 2020);
}

TEST(edid_test, week_of_manufacture)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_EQ(edid.weekOfManufacture(), 10);
}

TEST(edid_test, week_of_manufacture_invalid)
{
    QByteArray data = createValidEdidBlock();
    quint8* raw = reinterpret_cast<quint8*>(data.data());

    raw[16] = 0; // invalid week (< 1)

    // Recompute checksum.
    quint8 sum = 0;
    for (int i = 0; i < 127; ++i)
        sum += raw[i];
    raw[127] = static_cast<quint8>(0x100 - sum);

    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_EQ(edid.weekOfManufacture(), 0);
}

TEST(edid_test, edid_version)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_EQ(edid.edidVersion(), 1);
    EXPECT_EQ(edid.edidRevision(), 3);
}

TEST(edid_test, image_size)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_EQ(edid.maxHorizontalImageSize(), 53);
    EXPECT_EQ(edid.maxVerticalImageSize(), 30);
}

TEST(edid_test, gamma)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_DOUBLE_EQ(edid.gamma(), 3.2);
}

TEST(edid_test, gamma_undefined)
{
    QByteArray data = createValidEdidBlock();
    quint8* raw = reinterpret_cast<quint8*>(data.data());

    raw[23] = 0xFF; // gamma = 0xFF means undefined

    // Recompute checksum.
    quint8 sum = 0;
    for (int i = 0; i < 127; ++i)
        sum += raw[i];
    raw[127] = static_cast<quint8>(0x100 - sum);

    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_DOUBLE_EQ(edid.gamma(), 0.0);
}

TEST(edid_test, feature_support)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_EQ(edid.featureSupport(), 0x06);
}

TEST(edid_test, input_signal_type_digital)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_EQ(edid.inputSignalType(), Edid::INPUT_SIGNAL_TYPE_DIGITAL);
}

TEST(edid_test, input_signal_type_analog)
{
    QByteArray data = createValidEdidBlock();
    quint8* raw = reinterpret_cast<quint8*>(data.data());

    raw[20] = 0x00; // clear bit 7 => analog

    // Recompute checksum.
    quint8 sum = 0;
    for (int i = 0; i < 127; ++i)
        sum += raw[i];
    raw[127] = static_cast<quint8>(0x100 - sum);

    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_EQ(edid.inputSignalType(), Edid::INPUT_SIGNAL_TYPE_ANALOG);
}

TEST(edid_test, manufacturer_name)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());

    // Manufacturer ID was set to "DEL" => "Dell"
    EXPECT_EQ(edid.manufacturerName(), "Dell");
}

TEST(edid_test, monitor_name)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_EQ(edid.monitorName(), "Test Monitor");
}

TEST(edid_test, serial_number)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_EQ(edid.serialNumber(), "ABC1234567");
}

TEST(edid_test, monitor_range_limits)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_EQ(edid.minVerticalRate(), 56);
    EXPECT_EQ(edid.maxVerticalRate(), 76);
    EXPECT_EQ(edid.minHorizontalRate(), 30);
    EXPECT_EQ(edid.maxHorizontalRate(), 81);
    EXPECT_EQ(edid.maxSupportedPixelClock(), 170);
}

TEST(edid_test, horizontal_resolution)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());

    // horizontal_active = 0x80 (raw[56]), upper nibble from raw[58] = 0x7
    // => (0x7 << 8) | 0x80 = 0x780 = 1920
    EXPECT_EQ(edid.horizontalResolution(), 1920);
}

TEST(edid_test, vertical_resolution)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());

    // vertical_active = 0x38 (raw[59]), upper nibble from raw[61] = 0x4
    // => (0x4 << 8) | 0x38 = 0x438 = 1080
    EXPECT_EQ(edid.verticalResolution(), 1080);
}

TEST(edid_test, pixel_clock)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());

    // Pixel clock is stored at raw[54..55] as little-endian uint16.
    // raw[54]=0x02, raw[55]=0x3A => value = 0x3A02 = 14850
    // pixelClock() returns value / 100.0 = 148.50 MHz
    EXPECT_DOUBLE_EQ(edid.pixelClock(), 148.50);
}

TEST(edid_test, established_timings)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());

    EXPECT_EQ(edid.estabilishedTimings1(), 0x21);
    EXPECT_EQ(edid.estabilishedTimings2(), 0x08);
    EXPECT_EQ(edid.manufacturersTimings(), 0x00);
}

TEST(edid_test, standard_timings_count)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_EQ(edid.standardTimingsCount(), 8);
}

TEST(edid_test, standard_timings_first_entry)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());

    int width = 0, height = 0, frequency = 0;
    // First entry: 1280x1024@60Hz (aspect 5:4)
    EXPECT_TRUE(edid.standardTimings(0, &width, &height, &frequency));
    EXPECT_EQ(width, 1280);
    EXPECT_EQ(height, 1024);
    EXPECT_EQ(frequency, 60);
}

TEST(edid_test, standard_timings_unused_entry)
{
    QByteArray data = createValidEdidBlock();
    Edid edid(data);
    ASSERT_TRUE(edid.isValid());

    int width = 0, height = 0, frequency = 0;
    // Second entry is 0x01 0x01 (unused).
    EXPECT_FALSE(edid.standardTimings(1, &width, &height, &frequency));
}

TEST(edid_test, manufacturer_unknown)
{
    QByteArray data = createValidEdidBlock();
    quint8* raw = reinterpret_cast<quint8*>(data.data());

    // Set manufacturer to "ZZZ" which is not in the table.
    // Z=26 => (26 << 10) | (26 << 5) | 26 = 0x6B5A
    // Big-endian: raw[8] = 0x6B, raw[9] = 0x5A
    raw[8] = 0x6B;
    raw[9] = 0x5A;

    // Recompute checksum.
    quint8 sum = 0;
    for (int i = 0; i < 127; ++i)
        sum += raw[i];
    raw[127] = static_cast<quint8>(0x100 - sum);

    Edid edid(data);
    ASSERT_TRUE(edid.isValid());
    EXPECT_TRUE(edid.manufacturerName().isEmpty());
}

TEST(edid_test, standard_timings_aspect_16_9)
{
    QByteArray data = createValidEdidBlock();
    quint8* raw = reinterpret_cast<quint8*>(data.data());

    // Set second standard timing entry (bytes 40-41).
    // 1920x1080@60Hz, aspect 16:9
    // width = (byte1 + 31) * 8 => byte1 = 1920/8 - 31 = 209
    // aspect 16:9 => bits 7:6 = 0x03, frequency = 60 - 60 = 0 => byte2 = 0xC0
    raw[40] = 209;
    raw[41] = 0xC0;

    // Recompute checksum.
    quint8 sum = 0;
    for (int i = 0; i < 127; ++i)
        sum += raw[i];
    raw[127] = static_cast<quint8>(0x100 - sum);

    Edid edid(data);
    ASSERT_TRUE(edid.isValid());

    int width = 0, height = 0, frequency = 0;
    EXPECT_TRUE(edid.standardTimings(1, &width, &height, &frequency));
    EXPECT_EQ(width, 1920);
    EXPECT_EQ(height, 1080);
    EXPECT_EQ(frequency, 60);
}

TEST(edid_test, standard_timings_aspect_4_3)
{
    QByteArray data = createValidEdidBlock();
    quint8* raw = reinterpret_cast<quint8*>(data.data());

    // Set second standard timing entry (bytes 40-41).
    // 1024x768@75Hz, aspect 4:3
    // width = (byte1 + 31) * 8 => byte1 = 1024/8 - 31 = 97
    // aspect 4:3 => bits 7:6 = 0x01, frequency = 75 - 60 = 15 => byte2 = 0x4F
    raw[40] = 97;
    raw[41] = 0x4F;

    // Recompute checksum.
    quint8 sum = 0;
    for (int i = 0; i < 127; ++i)
        sum += raw[i];
    raw[127] = static_cast<quint8>(0x100 - sum);

    Edid edid(data);
    ASSERT_TRUE(edid.isValid());

    int width = 0, height = 0, frequency = 0;
    EXPECT_TRUE(edid.standardTimings(1, &width, &height, &frequency));
    EXPECT_EQ(width, 1024);
    EXPECT_EQ(height, 768);
    EXPECT_EQ(frequency, 75);
}

TEST(edid_test, standard_timings_aspect_16_10)
{
    QByteArray data = createValidEdidBlock();
    quint8* raw = reinterpret_cast<quint8*>(data.data());

    // Set second standard timing entry (bytes 40-41).
    // 1680x1050@60Hz, aspect 16:10 (EDID v1.3: ratio 0x00 = 16:10)
    // width = (byte1 + 31) * 8 => byte1 = 1680/8 - 31 = 179
    // aspect 16:10 => bits 7:6 = 0x00, frequency = 60 - 60 = 0 => byte2 = 0x00
    raw[40] = 179;
    raw[41] = 0x00;

    // Recompute checksum.
    quint8 sum = 0;
    for (int i = 0; i < 127; ++i)
        sum += raw[i];
    raw[127] = static_cast<quint8>(0x100 - sum);

    Edid edid(data);
    ASSERT_TRUE(edid.isValid());

    int width = 0, height = 0, frequency = 0;
    EXPECT_TRUE(edid.standardTimings(1, &width, &height, &frequency));
    EXPECT_EQ(width, 1680);
    EXPECT_EQ(height, 1050);
    EXPECT_EQ(frequency, 60);
}

} // namespace base
