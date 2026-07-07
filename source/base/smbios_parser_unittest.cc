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

#include <gtest/gtest.h>

#include <QByteArray>
#include <QList>

namespace {

//--------------------------------------------------------------------------------------------------
// Wraps raw structure-table data into the SmbiosDump layout: an 8-byte header followed by the
// table data. |declared_length| overrides the length field to simulate a header that lies about
// the amount of data; by default it matches the data size.
QByteArray makeDump(const QByteArray& tables, int declared_length = -1)
{
    const quint32 length = declared_length >= 0 ? static_cast<quint32>(declared_length) :
                                                  static_cast<quint32>(tables.size());

    QByteArray dump;
    dump.append(static_cast<char>(0)); // used_20_calling_method
    dump.append(static_cast<char>(3)); // smbios_major_version
    dump.append(static_cast<char>(2)); // smbios_minor_version
    dump.append(static_cast<char>(0)); // dmi_revision
    dump.append(reinterpret_cast<const char*>(&length), sizeof(length));
    dump.append(tables);
    return dump;
}

//--------------------------------------------------------------------------------------------------
// Builds one structure: a 4-byte header, |formatted| bytes after it and a string area. The
// declared length is 4 + formatted.size(). An empty string list produces the mandatory
// double-null terminator.
QByteArray makeTable(quint8 type, const QByteArray& formatted, const QList<QByteArray>& strings)
{
    QByteArray table;
    table.append(static_cast<char>(type));
    table.append(static_cast<char>(sizeof(SmbiosTable) + formatted.size()));
    table.append(static_cast<char>(0)); // handle low byte
    table.append(static_cast<char>(0)); // handle high byte
    table.append(formatted);

    for (const QByteArray& string : strings)
    {
        table.append(string);
        table.append(static_cast<char>(0));
    }

    if (strings.isEmpty())
        table.append(static_cast<char>(0));
    table.append(static_cast<char>(0));

    return table;
}

//--------------------------------------------------------------------------------------------------
QByteArray endOfTable()
{
    return makeTable(SMBIOS_TABLE_TYPE_END_OF_TABLE, QByteArray(), {});
}

//--------------------------------------------------------------------------------------------------
QList<quint8> enumerateTypes(const QByteArray& dump)
{
    QList<quint8> types;

    for (SmbiosTableEnumerator enumerator(dump); !enumerator.isAtEnd(); enumerator.advance())
        types.append(enumerator.table()->type);

    return types;
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, EnumeratesValidTables)
{
    QByteArray tables;
    tables += makeTable(SMBIOS_TABLE_TYPE_BIOS, QByteArray(0x12 - 4, '\0'), { "Vendor" });
    tables += makeTable(SMBIOS_TABLE_TYPE_BASEBOARD, QByteArray(0x0F - 4, '\0'), { "Maker" });
    tables += endOfTable();

    const QList<quint8> types = enumerateTypes(makeDump(tables));

    ASSERT_EQ(types.size(), 2);
    EXPECT_EQ(types[0], SMBIOS_TABLE_TYPE_BIOS);
    EXPECT_EQ(types[1], SMBIOS_TABLE_TYPE_BASEBOARD);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, EmptyAndOversizedDumps)
{
    EXPECT_TRUE(SmbiosTableEnumerator(QByteArray()).isAtEnd());

    const QByteArray oversized(static_cast<qsizetype>(sizeof(SmbiosDump)) + 1, '\0');
    EXPECT_TRUE(SmbiosTableEnumerator(oversized).isAtEnd());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, BiosStrings)
{
    QByteArray formatted(0x12 - 4, '\0');
    formatted[0] = 1; // vendor: string #1
    formatted[1] = 2; // version: string #2
    formatted[4] = 3; // release_date: string #3

    const QByteArray dump = makeDump(
        makeTable(SMBIOS_TABLE_TYPE_BIOS, formatted, { "AMI", "1.2.3", "01/01/2020" }) +
        endOfTable());

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosBios bios(enumerator.table());
    EXPECT_EQ(bios.vendor(), QString("AMI"));
    EXPECT_EQ(bios.version(), QString("1.2.3"));
    EXPECT_EQ(bios.releaseDate(), QString("01/01/2020"));
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, StringNumberOutOfRange)
{
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_BIOS, QByteArray(0x12 - 4, '\0'), { "Only" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    const SmbiosTable* table = enumerator.table();
    EXPECT_TRUE(smbiosString(table, 0).isEmpty());
    EXPECT_EQ(smbiosString(table, 1), QString("Only"));
    EXPECT_TRUE(smbiosString(table, 5).isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, LengthOverrunningBufferIsNotExposed)
{
    // The header claims 200 bytes of formatted area, but the buffer ends long before that.
    // Exposing such a table would let field accesses and string walks leave the buffer.
    QByteArray table;
    table.append(static_cast<char>(SMBIOS_TABLE_TYPE_BIOS));
    table.append(static_cast<char>(200));
    table.append(QByteArray(10, '\0'));

    EXPECT_TRUE(SmbiosTableEnumerator(makeDump(table)).isAtEnd());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, UnterminatedStringAreaIsNotExposed)
{
    // A valid header, but the string area runs to the end of the buffer without the double
    // null terminator - a string walk over this table would leave the buffer.
    QByteArray table;
    table.append(static_cast<char>(SMBIOS_TABLE_TYPE_BIOS));
    table.append(static_cast<char>(sizeof(SmbiosTable)));
    table.append(static_cast<char>(0));
    table.append(static_cast<char>(0));
    table.append("AAAAAAAA");

    EXPECT_TRUE(SmbiosTableEnumerator(makeDump(table)).isAtEnd());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, ShortEntryStopsEnumeration)
{
    QByteArray tables;
    tables += makeTable(SMBIOS_TABLE_TYPE_BIOS, QByteArray(0x12 - 4, '\0'), { "Vendor" });

    // A header with length less than 4 makes locating the next entry impossible.
    tables.append(static_cast<char>(SMBIOS_TABLE_TYPE_BASEBOARD));
    tables.append(static_cast<char>(3));
    tables.append(QByteArray(8, '\0'));

    const QList<quint8> types = enumerateTypes(makeDump(tables));

    ASSERT_EQ(types.size(), 1);
    EXPECT_EQ(types[0], SMBIOS_TABLE_TYPE_BIOS);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, EndOfTableStopsEnumeration)
{
    QByteArray tables;
    tables += makeTable(SMBIOS_TABLE_TYPE_BIOS, QByteArray(0x12 - 4, '\0'), { "Vendor" });
    tables += endOfTable();
    tables += makeTable(SMBIOS_TABLE_TYPE_BASEBOARD, QByteArray(0x0F - 4, '\0'), { "Ghost" });

    const QList<quint8> types = enumerateTypes(makeDump(tables));

    ASSERT_EQ(types.size(), 1);
    EXPECT_EQ(types[0], SMBIOS_TABLE_TYPE_BIOS);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, LyingHeaderLengthIsContained)
{
    // The dump header claims far more table data than the dump actually carries. The zeroed
    // slack after the real data must terminate the enumeration instead of producing tables.
    const QByteArray tables =
        makeTable(SMBIOS_TABLE_TYPE_BIOS, QByteArray(0x12 - 4, '\0'), { "Vendor" });

    const QList<quint8> types = enumerateTypes(makeDump(tables, 4000));

    ASSERT_EQ(types.size(), 1);
    EXPECT_EQ(types[0], SMBIOS_TABLE_TYPE_BIOS);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, BaseboardStrings)
{
    QByteArray formatted(0x0F - 4, '\0');
    formatted[0] = 1; // manufacturer: string #1
    formatted[1] = 2; // product: string #2

    const QByteArray dump = makeDump(
        makeTable(SMBIOS_TABLE_TYPE_BASEBOARD, formatted, { "ASUS", "PRIME B550" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosBaseboard baseboard(enumerator.table());
    ASSERT_TRUE(baseboard.isValid());
    EXPECT_EQ(baseboard.manufacturer(), QString("ASUS"));
    EXPECT_EQ(baseboard.product(), QString("PRIME B550"));
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedBaseboardIsInvalid)
{
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_BASEBOARD, QByteArray(2, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosBaseboard(enumerator.table()).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, MemoryDeviceFields)
{
    QByteArray formatted(0x15 - 4, '\0');
    formatted[0x0C - 4] = static_cast<char>(0x00); // module_size low byte: 2048 MB
    formatted[0x0D - 4] = static_cast<char>(0x08); // module_size high byte
    formatted[0x0E - 4] = static_cast<char>(0x09); // form_factor: DIMM
    formatted[0x10 - 4] = 1;                       // device_location: string #1
    formatted[0x12 - 4] = static_cast<char>(0x1A); // memory_type: DDR4

    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_MEMORY_DEVICE, formatted, { "DIMM_A1" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosMemoryDevice device(enumerator.table());
    ASSERT_TRUE(device.isValid());
    EXPECT_TRUE(device.isPresent());
    EXPECT_EQ(device.size(), 2048ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(device.location(), QString("DIMM_A1"));
    EXPECT_EQ(device.type(), QString("DDR4"));
    EXPECT_EQ(device.formFactor(), QString("DIMM"));
    EXPECT_EQ(device.speed(), 0u); // The table is too short to carry the speed field.
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, MemoryDeviceSizeVariants)
{
    auto makeDevice = [](quint16 module_size, quint32 ext_size, quint8 length) -> QByteArray
    {
        QByteArray formatted(length - 4, '\0');
        formatted[0x0C - 4] = static_cast<char>(module_size & 0xFF);
        formatted[0x0D - 4] = static_cast<char>(module_size >> 8);

        if (length >= 0x20)
        {
            for (int i = 0; i < 4; ++i)
                formatted[0x1C - 4 + i] = static_cast<char>((ext_size >> (i * 8)) & 0xFF);
        }

        return makeTable(SMBIOS_TABLE_TYPE_MEMORY_DEVICE, formatted, {});
    };

    auto sizeOf = [](const QByteArray& dump) -> quint64
    {
        SmbiosTableEnumerator enumerator(dump);
        if (enumerator.isAtEnd())
            return quint64(-1);
        return SmbiosMemoryDevice(enumerator.table()).size();
    };

    // Size in kB (bit 15 set): 1024 kB.
    EXPECT_EQ(sizeOf(makeDump(makeDevice(0x8000 | 1024, 0, 0x15))), 1024ULL * 1024ULL);

    // No installed device.
    EXPECT_EQ(sizeOf(makeDump(makeDevice(0xFFFF, 0, 0x15))), 0ULL);

    // Extended size: 32768 MB with zero low bits selects the GB branch (32 GB).
    EXPECT_EQ(sizeOf(makeDump(makeDevice(0x7FFF, 32768, 0x20))),
              32ULL * 1024ULL * 1024ULL * 1024ULL);
}
