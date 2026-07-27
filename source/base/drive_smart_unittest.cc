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

#include "base/drive_smart.h"

#include <gtest/gtest.h>

namespace {

constexpr qsizetype kTableSectorSize = 512;
constexpr qsizetype kTableEntrySize = 12;
constexpr qsizetype kTableFirstEntry = 2;

//--------------------------------------------------------------------------------------------------
void setIdentifyWord(QByteArray* data, int word, quint16 value)
{
    quint8* raw = reinterpret_cast<quint8*>(data->data()) + (word * 2);

    raw[0] = static_cast<quint8>(value & 0xFF);
    raw[1] = static_cast<quint8>(value >> 8);
}

//--------------------------------------------------------------------------------------------------
// Strings are stored as 16-bit words with the characters of each word swapped.
void setIdentifyString(QByteArray* data, int word, qsizetype size, const QByteArray& value)
{
    quint8* raw = reinterpret_cast<quint8*>(data->data()) + (word * 2);

    for (qsizetype i = 0; i < size; i += 2)
    {
        raw[i] = static_cast<quint8>(i + 1 < value.size() ? value[i + 1] : ' ');
        raw[i + 1] = static_cast<quint8>(i < value.size() ? value[i] : ' ');
    }
}

//--------------------------------------------------------------------------------------------------
QByteArray createIdentifyData()
{
    QByteArray data(AtaIdentify::kDataSize, 0);

    setIdentifyString(&data, 27, 40, "Samsung SSD 990 PRO 2TB");
    setIdentifyString(&data, 10, 20, "S6Z1NJ0T123456");
    setIdentifyString(&data, 23, 8, "4B2QJXD7");

    // Cache size in 512-byte units.
    setIdentifyWord(&data, 21, 512);

    // S.M.A.R.T. supported and enabled.
    setIdentifyWord(&data, 82, 0x0001);
    setIdentifyWord(&data, 85, 0x0001);

    return data;
}

//--------------------------------------------------------------------------------------------------
QByteArray createTableSector()
{
    QByteArray sector(kTableSectorSize, 0);

    // Data structure revision number.
    sector[0] = 0x10;

    return sector;
}

//--------------------------------------------------------------------------------------------------
void setTableAttribute(QByteArray* sector, int slot, quint8 id, quint16 status_flags, quint8 value,
                       quint8 worst_value, quint64 raw)
{
    quint8* entry = reinterpret_cast<quint8*>(sector->data()) + kTableFirstEntry +
        (slot * kTableEntrySize);

    entry[0] = id;
    entry[1] = static_cast<quint8>(status_flags & 0xFF);
    entry[2] = static_cast<quint8>(status_flags >> 8);
    entry[3] = value;
    entry[4] = worst_value;

    for (int i = 0; i < 6; ++i)
        entry[5 + i] = static_cast<quint8>((raw >> (8 * i)) & 0xFF);
}

//--------------------------------------------------------------------------------------------------
void setTableThreshold(QByteArray* sector, int slot, quint8 id, quint8 threshold)
{
    quint8* entry = reinterpret_cast<quint8*>(sector->data()) + kTableFirstEntry +
        (slot * kTableEntrySize);

    entry[0] = id;
    entry[1] = threshold;
}

//--------------------------------------------------------------------------------------------------
void setLogByte(QByteArray* log, qsizetype offset, quint8 value)
{
    (*log)[offset] = static_cast<char>(value);
}

//--------------------------------------------------------------------------------------------------
void setLogWord(QByteArray* log, qsizetype offset, quint16 value)
{
    quint8* raw = reinterpret_cast<quint8*>(log->data()) + offset;

    raw[0] = static_cast<quint8>(value & 0xFF);
    raw[1] = static_cast<quint8>(value >> 8);
}

//--------------------------------------------------------------------------------------------------
void setLogDword(QByteArray* log, qsizetype offset, quint32 value)
{
    quint8* raw = reinterpret_cast<quint8*>(log->data()) + offset;

    for (int i = 0; i < 4; ++i)
        raw[i] = static_cast<quint8>((value >> (8 * i)) & 0xFF);
}

//--------------------------------------------------------------------------------------------------
// The counters of the log page are 128 bits wide.
void setLogCounter(QByteArray* log, qsizetype offset, quint64 low, quint64 high)
{
    quint8* raw = reinterpret_cast<quint8*>(log->data()) + offset;

    for (int i = 0; i < 8; ++i)
    {
        raw[i] = static_cast<quint8>((low >> (8 * i)) & 0xFF);
        raw[8 + i] = static_cast<quint8>((high >> (8 * i)) & 0xFF);
    }
}

} // namespace

TEST(AtaIdentifyTest, EmptyBuffer)
{
    const QByteArray empty;
    AtaIdentify identify(empty);

    EXPECT_FALSE(identify.isValid());
    EXPECT_TRUE(identify.model().isEmpty());
}

TEST(AtaIdentifyTest, TruncatedBuffer)
{
    QByteArray data = createIdentifyData();
    data.truncate(AtaIdentify::kDataSize - 1);

    AtaIdentify identify(data);

    EXPECT_FALSE(identify.isValid());
}

TEST(AtaIdentifyTest, EmptyModelIsNotValid)
{
    QByteArray data(AtaIdentify::kDataSize, 0);

    AtaIdentify identify(data);

    EXPECT_FALSE(identify.isValid());
}

TEST(AtaIdentifyTest, Strings)
{
    AtaIdentify identify(createIdentifyData());

    ASSERT_TRUE(identify.isValid());
    EXPECT_EQ(identify.model(), "Samsung SSD 990 PRO 2TB");
    EXPECT_EQ(identify.serialNumber(), "S6Z1NJ0T123456");
    EXPECT_EQ(identify.firmwareRevision(), "4B2QJXD7");
}

TEST(AtaIdentifyTest, BufferSize)
{
    AtaIdentify identify(createIdentifyData());

    EXPECT_EQ(identify.bufferSize(), 512 * 512);
}

TEST(AtaIdentifyTest, Smart)
{
    QByteArray data = createIdentifyData();

    AtaIdentify supported(data);
    EXPECT_TRUE(supported.isSmartSupported());
    EXPECT_TRUE(supported.isSmartEnabled());

    // A drive can support the feature while having it turned off.
    setIdentifyWord(&data, 85, 0x0000);

    AtaIdentify disabled(data);
    EXPECT_TRUE(disabled.isSmartSupported());
    EXPECT_FALSE(disabled.isSmartEnabled());
}

TEST(AtaIdentifyTest, SolidStateMedia)
{
    QByteArray data = createIdentifyData();
    setIdentifyWord(&data, 217, 1);

    AtaIdentify identify(data);

    EXPECT_TRUE(identify.isSolidState());
    EXPECT_EQ(identify.rotationRate(), 0u);
}

TEST(AtaIdentifyTest, RotationRate)
{
    QByteArray data = createIdentifyData();
    setIdentifyWord(&data, 217, 7200);

    AtaIdentify identify(data);

    EXPECT_FALSE(identify.isSolidState());
    EXPECT_EQ(identify.rotationRate(), 7200u);
}

TEST(AtaIdentifyTest, ReservedRotationRate)
{
    QByteArray data = createIdentifyData();

    // Rates between 0002h and 0400h are reserved and must not be reported as a rate.
    setIdentifyWord(&data, 217, 0x0400);

    AtaIdentify reserved(data);
    EXPECT_FALSE(reserved.isSolidState());
    EXPECT_EQ(reserved.rotationRate(), 0u);

    setIdentifyWord(&data, 217, 0xFFFF);

    AtaIdentify not_reported(data);
    EXPECT_EQ(not_reported.rotationRate(), 0u);
}

TEST(AtaIdentifyTest, RemovableMedia)
{
    QByteArray data = createIdentifyData();

    AtaIdentify fixed(data);
    EXPECT_FALSE(fixed.isRemovable());

    setIdentifyWord(&data, 0, 0x0080);

    AtaIdentify removable(data);
    EXPECT_TRUE(removable.isRemovable());
}

TEST(AtaSmartTest, EmptyBuffers)
{
    const QByteArray empty;
    AtaSmart smart(empty, empty);

    EXPECT_FALSE(smart.isValid());
    EXPECT_TRUE(smart.attributes().isEmpty());
}

TEST(AtaSmartTest, TruncatedSector)
{
    QByteArray attributes = createTableSector();
    setTableAttribute(&attributes, 0, 0x05, AtaSmart::STATUS_FLAG_PRE_FAILURE, 100, 100, 0);
    attributes.truncate(kTableSectorSize - 1);

    AtaSmart smart(attributes, QByteArray());

    EXPECT_FALSE(smart.isValid());
}

TEST(AtaSmartTest, EmptySlotsAreSkipped)
{
    QByteArray attributes = createTableSector();

    // The used slots do not have to be the first ones.
    setTableAttribute(&attributes, 3, 0x09, AtaSmart::STATUS_FLAG_ONLINE, 95, 90, 1234);
    setTableAttribute(&attributes, 7, 0x0C, AtaSmart::STATUS_FLAG_ONLINE, 99, 99, 56);

    AtaSmart smart(attributes, QByteArray());

    ASSERT_TRUE(smart.isValid());
    ASSERT_EQ(smart.attributes().count(), 2);
    EXPECT_EQ(smart.attributes().at(0).id, 0x09);
    EXPECT_EQ(smart.attributes().at(1).id, 0x0C);
}

TEST(AtaSmartTest, AttributeFields)
{
    QByteArray attributes = createTableSector();
    setTableAttribute(&attributes, 0, 0x05, AtaSmart::STATUS_FLAG_PRE_FAILURE, 100, 98,
                      0x0000FFFFFFFFFFFFull);

    AtaSmart smart(attributes, QByteArray());

    ASSERT_TRUE(smart.isValid());
    ASSERT_EQ(smart.attributes().count(), 1);

    const AtaSmart::Attribute& attribute = smart.attributes().at(0);

    EXPECT_EQ(attribute.id, 0x05);
    EXPECT_EQ(attribute.status_flags, AtaSmart::STATUS_FLAG_PRE_FAILURE);
    EXPECT_EQ(attribute.value, 100);
    EXPECT_EQ(attribute.worst_value, 98);

    // The raw value is 48 bits wide and must not pick up the reserved byte that follows it.
    EXPECT_EQ(attribute.raw, 0x0000FFFFFFFFFFFFull);
}

TEST(AtaSmartTest, RawValueIsLittleEndian)
{
    QByteArray attributes = createTableSector();
    setTableAttribute(&attributes, 0, 0x09, 0, 100, 100, 0x060504030201ull);

    AtaSmart smart(attributes, QByteArray());

    ASSERT_EQ(smart.attributes().count(), 1);
    EXPECT_EQ(smart.attributes().at(0).raw, 0x060504030201ull);
}

TEST(AtaSmartTest, ThresholdsMatchedById)
{
    QByteArray attributes = createTableSector();
    setTableAttribute(&attributes, 0, 0x05, 0, 100, 100, 0);
    setTableAttribute(&attributes, 1, 0x09, 0, 95, 95, 0);

    // The threshold table lists the attributes in a different order.
    QByteArray thresholds = createTableSector();
    setTableThreshold(&thresholds, 0, 0x09, 0);
    setTableThreshold(&thresholds, 1, 0x05, 36);

    AtaSmart smart(attributes, thresholds);

    ASSERT_EQ(smart.attributes().count(), 2);
    EXPECT_EQ(smart.attributes().at(0).threshold, 36);
    EXPECT_EQ(smart.attributes().at(1).threshold, 0);
}

TEST(AtaSmartTest, MissingThresholdsReadAsZero)
{
    QByteArray attributes = createTableSector();
    setTableAttribute(&attributes, 0, 0x05, 0, 100, 100, 0);

    AtaSmart smart(attributes, QByteArray());

    ASSERT_EQ(smart.attributes().count(), 1);
    EXPECT_EQ(smart.attributes().at(0).threshold, 0);
}

TEST(AtaSmartTest, AllSlotsUsed)
{
    QByteArray attributes = createTableSector();

    for (int i = 0; i < 30; ++i)
        setTableAttribute(&attributes, i, static_cast<quint8>(i + 1), 0, 100, 100, 0);

    AtaSmart smart(attributes, QByteArray());

    // The table holds 30 entries and the parser must not read past the last one.
    EXPECT_EQ(smart.attributes().count(), 30);
}

TEST(NvmeSmartTest, EmptyBuffer)
{
    const QByteArray empty;
    NvmeSmart smart(empty);

    EXPECT_FALSE(smart.isValid());
}

TEST(NvmeSmartTest, TruncatedBuffer)
{
    NvmeSmart smart(QByteArray(NvmeSmart::kHealthLogSize - 1, 0));

    EXPECT_FALSE(smart.isValid());
}

TEST(NvmeSmartTest, ZeroedLogIsValid)
{
    NvmeSmart smart(QByteArray(NvmeSmart::kHealthLogSize, 0));

    ASSERT_TRUE(smart.isValid());
    EXPECT_EQ(smart.healthInfo().critical_warning, 0);
    EXPECT_EQ(smart.healthInfo().power_on_hours, 0u);
}

TEST(NvmeSmartTest, Fields)
{
    QByteArray log(NvmeSmart::kHealthLogSize, 0);

    setLogByte(&log, 0, NvmeSmart::CRITICAL_WARNING_RELIABILITY_DEGRADED);
    setLogWord(&log, 1, 313); // 40 degrees Celsius.
    setLogByte(&log, 3, 96);
    setLogByte(&log, 4, 10);
    setLogByte(&log, 5, 4);
    setLogCounter(&log, 32, 123456789, 0);
    setLogCounter(&log, 48, 987654321, 0);
    setLogCounter(&log, 64, 5555, 0);
    setLogCounter(&log, 80, 6666, 0);
    setLogCounter(&log, 96, 777, 0);
    setLogCounter(&log, 112, 1024, 0);
    setLogCounter(&log, 128, 8760, 0);
    setLogCounter(&log, 144, 7, 0);
    setLogCounter(&log, 160, 0, 0);
    setLogCounter(&log, 176, 3, 0);
    setLogDword(&log, 192, 15);
    setLogDword(&log, 196, 2);
    setLogWord(&log, 200, 310);
    setLogWord(&log, 214, 320);

    NvmeSmart smart(log);
    ASSERT_TRUE(smart.isValid());

    const NvmeSmart::HealthInfo& info = smart.healthInfo();

    EXPECT_EQ(info.critical_warning, NvmeSmart::CRITICAL_WARNING_RELIABILITY_DEGRADED);
    EXPECT_EQ(info.composite_temperature, 313);
    EXPECT_EQ(info.available_spare, 96);
    EXPECT_EQ(info.available_spare_threshold, 10);
    EXPECT_EQ(info.percentage_used, 4);
    EXPECT_EQ(info.data_units_read, 123456789u);
    EXPECT_EQ(info.data_units_written, 987654321u);
    EXPECT_EQ(info.host_read_commands, 5555u);
    EXPECT_EQ(info.host_write_commands, 6666u);
    EXPECT_EQ(info.controller_busy_time, 777u);
    EXPECT_EQ(info.power_cycles, 1024u);
    EXPECT_EQ(info.power_on_hours, 8760u);
    EXPECT_EQ(info.unsafe_shutdowns, 7u);
    EXPECT_EQ(info.media_errors, 0u);
    EXPECT_EQ(info.error_log_entries, 3u);
    EXPECT_EQ(info.warning_temperature_time, 15u);
    EXPECT_EQ(info.critical_temperature_time, 2u);

    // Only the first and the last sensor were filled in.
    EXPECT_EQ(info.temperature_sensor[0], 310);
    EXPECT_EQ(info.temperature_sensor[1], 0);
    EXPECT_EQ(info.temperature_sensor[7], 320);
}

TEST(NvmeSmartTest, CounterIsTruncatedToLowBits)
{
    QByteArray log(NvmeSmart::kHealthLogSize, 0);

    setLogCounter(&log, 128, 0xFFFFFFFFFFFFFFFFull, 0x1234);

    NvmeSmart smart(log);

    ASSERT_TRUE(smart.isValid());
    EXPECT_EQ(smart.healthInfo().power_on_hours, 0xFFFFFFFFFFFFFFFFull);
}
