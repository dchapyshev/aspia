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

#include "base/xml_settings.h"

#include <QBuffer>
#include <QByteArray>
#include <QPoint>
#include <QRect>
#include <QSize>

#include <gtest/gtest.h>

namespace base {

namespace {

// Helper functions for write -> read roundtrip via QBuffer.
bool writeMap(const QSettings::SettingsMap& map, QByteArray& output)
{
    QBuffer device(&output);
    device.open(QIODevice::WriteOnly);
    return XmlSettings::writeFunc(device, map);
}

bool readMap(const QByteArray& input, QSettings::SettingsMap& map)
{
    QByteArray data(input);
    QBuffer device(&data);
    device.open(QIODevice::ReadOnly);
    return XmlSettings::readFunc(device, map);
}

bool roundtrip(const QSettings::SettingsMap& original, QSettings::SettingsMap& restored)
{
    QByteArray xml;
    if (!writeMap(original, xml))
        return false;
    return readMap(xml, restored);
}

} // namespace

// ============================================================================
// format()
// ============================================================================

TEST(xml_settings_test, format_returns_valid)
{
    QSettings::Format fmt = XmlSettings::format();
    EXPECT_NE(fmt, QSettings::InvalidFormat);
}

TEST(xml_settings_test, format_is_stable)
{
    QSettings::Format fmt1 = XmlSettings::format();
    QSettings::Format fmt2 = XmlSettings::format();
    EXPECT_EQ(fmt1, fmt2);
}

// ============================================================================
// writeFunc / readFunc: simple types
// ============================================================================

TEST(xml_settings_test, roundtrip_string)
{
    QSettings::SettingsMap original;
    original["key"] = QVariant(QString("hello world"));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["key"].toString(), "hello world");
}

TEST(xml_settings_test, roundtrip_int)
{
    QSettings::SettingsMap original;
    original["number"] = QVariant(42);

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    // Simple types are written as strings; on read they come back as QString.
    EXPECT_EQ(restored["number"].toString(), "42");
}

TEST(xml_settings_test, roundtrip_bool_true)
{
    QSettings::SettingsMap original;
    original["flag"] = QVariant(true);

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["flag"].toString(), "true");
}

TEST(xml_settings_test, roundtrip_bool_false)
{
    QSettings::SettingsMap original;
    original["flag"] = QVariant(false);

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["flag"].toString(), "false");
}

TEST(xml_settings_test, roundtrip_double)
{
    QSettings::SettingsMap original;
    original["pi"] = QVariant(3.14);

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_DOUBLE_EQ(restored["pi"].toString().toDouble(), 3.14);
}

TEST(xml_settings_test, roundtrip_uint)
{
    QSettings::SettingsMap original;
    original["val"] = QVariant(static_cast<uint>(4000000000u));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["val"].toString(), "4000000000");
}

TEST(xml_settings_test, roundtrip_longlong)
{
    QSettings::SettingsMap original;
    original["big"] = QVariant(static_cast<qlonglong>(9000000000LL));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["big"].toString(), "9000000000");
}

TEST(xml_settings_test, roundtrip_ulonglong)
{
    QSettings::SettingsMap original;
    original["ubig"] = QVariant(static_cast<qulonglong>(18000000000ULL));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["ubig"].toString(), "18000000000");
}

// ============================================================================
// writeFunc / readFunc: QByteArray (hex encoding)
// ============================================================================

TEST(xml_settings_test, roundtrip_byte_array)
{
    QByteArray data;
    data.append('\x00');
    data.append('\x01');
    data.append('\xFF');
    data.append('\xAB');

    QSettings::SettingsMap original;
    original["data"] = QVariant(data);

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["data"].toByteArray(), data);
}

TEST(xml_settings_test, roundtrip_byte_array_empty)
{
    QSettings::SettingsMap original;
    original["empty"] = QVariant(QByteArray());

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_TRUE(restored["empty"].toByteArray().isEmpty());
}

// ============================================================================
// writeFunc / readFunc: QRect
// ============================================================================

TEST(xml_settings_test, roundtrip_rect)
{
    QSettings::SettingsMap original;
    original["window"] = QVariant(QRect(10, 20, 1920, 1080));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["window"].toRect(), QRect(10, 20, 1920, 1080));
}

TEST(xml_settings_test, roundtrip_rect_negative)
{
    QSettings::SettingsMap original;
    original["r"] = QVariant(QRect(-100, -200, 640, 480));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["r"].toRect(), QRect(-100, -200, 640, 480));
}

TEST(xml_settings_test, roundtrip_rect_zero)
{
    QSettings::SettingsMap original;
    original["r"] = QVariant(QRect(0, 0, 0, 0));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["r"].toRect(), QRect(0, 0, 0, 0));
}

// ============================================================================
// writeFunc / readFunc: QSize
// ============================================================================

TEST(xml_settings_test, roundtrip_size)
{
    QSettings::SettingsMap original;
    original["screen"] = QVariant(QSize(1920, 1080));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["screen"].toSize(), QSize(1920, 1080));
}

TEST(xml_settings_test, roundtrip_size_zero)
{
    QSettings::SettingsMap original;
    original["s"] = QVariant(QSize(0, 0));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["s"].toSize(), QSize(0, 0));
}

// ============================================================================
// writeFunc / readFunc: QPoint
// ============================================================================

TEST(xml_settings_test, roundtrip_point)
{
    QSettings::SettingsMap original;
    original["pos"] = QVariant(QPoint(123, 456));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["pos"].toPoint(), QPoint(123, 456));
}

TEST(xml_settings_test, roundtrip_point_negative)
{
    QSettings::SettingsMap original;
    original["p"] = QVariant(QPoint(-50, -75));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["p"].toPoint(), QPoint(-50, -75));
}

// ============================================================================
// writeFunc / readFunc: groups (nested keys)
// ============================================================================

TEST(xml_settings_test, roundtrip_grouped_keys)
{
    QSettings::SettingsMap original;
    original["network/port"] = QVariant(QString("8050"));
    original["network/host"] = QVariant(QString("localhost"));
    original["ui/theme"] = QVariant(QString("dark"));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["network/port"].toString(), "8050");
    EXPECT_EQ(restored["network/host"].toString(), "localhost");
    EXPECT_EQ(restored["ui/theme"].toString(), "dark");
}

TEST(xml_settings_test, roundtrip_deeply_nested)
{
    QSettings::SettingsMap original;
    original["a/b/c/d"] = QVariant(QString("deep"));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["a/b/c/d"].toString(), "deep");
}

// ============================================================================
// writeFunc / readFunc: multiple keys
// ============================================================================

TEST(xml_settings_test, roundtrip_multiple_keys)
{
    QSettings::SettingsMap original;
    original["alpha"] = QVariant(QString("aaa"));
    original["beta"] = QVariant(QString("bbb"));
    original["gamma"] = QVariant(QString("ccc"));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored.size(), 3);
    EXPECT_EQ(restored["alpha"].toString(), "aaa");
    EXPECT_EQ(restored["beta"].toString(), "bbb");
    EXPECT_EQ(restored["gamma"].toString(), "ccc");
}

TEST(xml_settings_test, roundtrip_mixed_types)
{
    QSettings::SettingsMap original;
    original["name"] = QVariant(QString("test"));
    original["count"] = QVariant(42);
    original["rect"] = QVariant(QRect(0, 0, 800, 600));
    original["size"] = QVariant(QSize(1024, 768));
    original["pos"] = QVariant(QPoint(50, 100));
    original["blob"] = QVariant(QByteArray("binary\x00data", 11));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["name"].toString(), "test");
    EXPECT_EQ(restored["count"].toString(), "42");
    EXPECT_EQ(restored["rect"].toRect(), QRect(0, 0, 800, 600));
    EXPECT_EQ(restored["size"].toSize(), QSize(1024, 768));
    EXPECT_EQ(restored["pos"].toPoint(), QPoint(50, 100));
    EXPECT_EQ(restored["blob"].toByteArray(), QByteArray("binary\x00data", 11));
}

// ============================================================================
// writeFunc / readFunc: empty map
// ============================================================================

TEST(xml_settings_test, roundtrip_empty_map)
{
    QSettings::SettingsMap original;
    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_TRUE(restored.isEmpty());
}

// ============================================================================
// readFunc: invalid XML
// ============================================================================

TEST(xml_settings_test, read_invalid_xml)
{
    QByteArray garbage("<<<not xml at all>>>");
    QSettings::SettingsMap map;
    EXPECT_FALSE(readMap(garbage, map));
}

TEST(xml_settings_test, read_empty_input)
{
    QByteArray empty;
    QSettings::SettingsMap map;
    // Empty input — XML reader will report an error (no <settings>).
    readMap(empty, map);
    EXPECT_TRUE(map.isEmpty());
}

TEST(xml_settings_test, read_no_settings_element)
{
    QByteArray xml("<?xml version=\"1.0\"?><other><value name=\"x\">1</value></other>");
    QSettings::SettingsMap map;
    EXPECT_FALSE(readMap(xml, map));
}

TEST(xml_settings_test, read_value_without_name)
{
    QByteArray xml("<?xml version=\"1.0\"?><settings><value>data</value></settings>");
    QSettings::SettingsMap map;
    EXPECT_FALSE(readMap(xml, map));
}

// ============================================================================
// writeFunc: output is valid XML
// ============================================================================

TEST(xml_settings_test, write_produces_xml_header)
{
    QSettings::SettingsMap map;
    map["key"] = QVariant(QString("value"));

    QByteArray output;
    ASSERT_TRUE(writeMap(map, output));

    QString xml = QString::fromUtf8(output);
    EXPECT_TRUE(xml.contains("<?xml"));
    EXPECT_TRUE(xml.contains("<settings>"));
    EXPECT_TRUE(xml.contains("</settings>"));
}

// ============================================================================
// Groups with shared prefix
// ============================================================================

TEST(xml_settings_test, roundtrip_shared_group_prefix)
{
    QSettings::SettingsMap original;
    original["server/address"] = QVariant(QString("192.168.1.1"));
    original["server/port"] = QVariant(QString("443"));
    original["server/timeout"] = QVariant(QString("30"));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored.size(), 3);
    EXPECT_EQ(restored["server/address"].toString(), "192.168.1.1");
    EXPECT_EQ(restored["server/port"].toString(), "443");
    EXPECT_EQ(restored["server/timeout"].toString(), "30");
}

// ============================================================================
// Special characters in values
// ============================================================================

TEST(xml_settings_test, roundtrip_special_chars_in_value)
{
    QSettings::SettingsMap original;
    original["path"] = QVariant(QString("C:\\Users\\test\\file <1> & \"2\""));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["path"].toString(), "C:\\Users\\test\\file <1> & \"2\"");
}

TEST(xml_settings_test, roundtrip_unicode_value)
{
    QSettings::SettingsMap original;
    original["text"] = QVariant(QString::fromUtf8(u8"Привет мир"));

    QSettings::SettingsMap restored;
    ASSERT_TRUE(roundtrip(original, restored));
    EXPECT_EQ(restored["text"].toString(), QString::fromUtf8(u8"Привет мир"));
}

} // namespace base
