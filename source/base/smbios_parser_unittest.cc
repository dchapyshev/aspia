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
TEST(SmbiosParserTest, RejectedDumpHasDeterministicGetters)
{
    // Shorter than the fixed dump header: rejected, and the getters must return zeros rather
    // than uninitialized memory.
    SmbiosTableEnumerator enumerator(QByteArray(3, '\x7F'));

    EXPECT_TRUE(enumerator.isAtEnd());
    EXPECT_EQ(enumerator.majorVersion(), 0);
    EXPECT_EQ(enumerator.minorVersion(), 0);
    EXPECT_EQ(enumerator.length(), 0u);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedTrailingTableNotExposed)
{
    // The dump header claims more data than the dump carries, and the last table is cut in the
    // middle of its string area. Without clamping the declared length to the real data, the
    // zeroed slack behind the data would provide a fake terminator and expose the cut table.
    QByteArray tables;
    tables += makeTable(SMBIOS_TABLE_TYPE_BIOS, QByteArray(0x12 - 4, '\0'), { "Vendor" });

    tables.append(static_cast<char>(SMBIOS_TABLE_TYPE_BASEBOARD));
    tables.append(static_cast<char>(sizeof(SmbiosTable)));
    tables.append(static_cast<char>(0));
    tables.append(static_cast<char>(0));
    tables.append("CUT"); // string area without the double null terminator

    const QList<quint8> types =
        enumerateTypes(makeDump(tables, static_cast<int>(tables.size()) + 100));

    ASSERT_EQ(types.size(), 1);
    EXPECT_EQ(types[0], SMBIOS_TABLE_TYPE_BIOS);
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
    ASSERT_TRUE(bios.isValid());
    EXPECT_EQ(bios.vendor(), QString("AMI"));
    EXPECT_EQ(bios.version(), QString("1.2.3"));
    EXPECT_EQ(bios.releaseDate(), QString("01/01/2020"));
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedBiosIsInvalid)
{
    // A BIOS table shorter than the SMBIOS 2.0 minimum (12h) must be reported as invalid.
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_BIOS, QByteArray(4, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosBios(enumerator.table()).isValid());
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
TEST(SmbiosParserTest, SystemUuid)
{
    // A full SMBIOS 2.1 system table: 4 string fields, the UUID and the wakeup type.
    QByteArray formatted(0x19 - 4, '\0');
    for (int i = 0; i < 16; ++i)
        formatted[4 + i] = static_cast<char>(i + 1);

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_SYSTEM, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());
    ASSERT_EQ(enumerator.table()->type, SMBIOS_TABLE_TYPE_SYSTEM);

    SmbiosSystem system(enumerator.table());
    ASSERT_TRUE(system.isValid());

    const QByteArray uuid = system.uuid();
    ASSERT_EQ(uuid.size(), 16);
    for (int i = 0; i < 16; ++i)
        EXPECT_EQ(static_cast<quint8>(uuid[i]), i + 1);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, SystemUuidUnsetOrUnknown)
{
    // All 0x00 (not set) and all 0xFF (set but unknown) UUIDs are both reported as absent.
    for (char filler : { '\x00', '\xFF' })
    {
        QByteArray formatted(0x19 - 4, '\0');
        for (int i = 0; i < 16; ++i)
            formatted[4 + i] = filler;

        const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_SYSTEM, formatted, {}));

        SmbiosTableEnumerator enumerator(dump);
        ASSERT_FALSE(enumerator.isAtEnd());

        SmbiosSystem system(enumerator.table());
        ASSERT_TRUE(system.isValid());
        EXPECT_TRUE(system.uuid().isEmpty());
    }
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, SystemUuidMissingOnShortTable)
{
    // An SMBIOS 2.0 system table (length 0x08) predates the UUID field.
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_SYSTEM, QByteArray(0x08 - 4, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosSystem system(enumerator.table());
    ASSERT_TRUE(system.isValid());
    EXPECT_TRUE(system.uuid().isEmpty());
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
TEST(SmbiosParserTest, ChassisFields)
{
    QByteArray formatted(0x15 - 4, '\0');
    formatted[0x04 - 4] = 1;                       // manufacturer: string #1
    formatted[0x05 - 4] = static_cast<char>(0x83); // type: desktop with the lock present
    formatted[0x06 - 4] = 2;                       // version: string #2
    formatted[0x07 - 4] = 3;                       // serial_number: string #3
    formatted[0x08 - 4] = 4;                       // asset_tag: string #4
    formatted[0x09 - 4] = 3;                       // boot_up_state: safe
    formatted[0x0A - 4] = 3;                       // power_supply_state: safe
    formatted[0x0B - 4] = 4;                       // thermal_state: warning
    formatted[0x0C - 4] = 3;                       // security_status: none
    formatted[0x11 - 4] = 2;                       // height: 2U
    formatted[0x12 - 4] = 1;                       // power_cords

    const QByteArray dump = makeDump(makeTable(
        SMBIOS_TABLE_TYPE_CHASSIS, formatted, { "Dell Inc.", "1.0", "SN123", "AT456" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosChassis chassis(enumerator.table());
    ASSERT_TRUE(chassis.isValid());
    EXPECT_EQ(chassis.manufacturer(), QString("Dell Inc."));
    EXPECT_EQ(chassis.version(), QString("1.0"));
    EXPECT_EQ(chassis.serialNumber(), QString("SN123"));
    EXPECT_EQ(chassis.assetTag(), QString("AT456"));
    EXPECT_EQ(chassis.type(), QString("Desktop"));
    EXPECT_TRUE(chassis.isLockPresent());
    EXPECT_EQ(chassis.bootUpState(), QString("Safe"));
    EXPECT_EQ(chassis.powerSupplyState(), QString("Safe"));
    EXPECT_EQ(chassis.thermalState(), QString("Warning"));
    EXPECT_EQ(chassis.securityStatus(), QString("None"));
    EXPECT_EQ(chassis.height(), 2u);
    EXPECT_EQ(chassis.powerCords(), 1u);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, ChassisSkuNumber)
{
    // Two contained elements of three bytes each sit between the fixed fields and the SKU number.
    QByteArray formatted(0x1C - 4, '\0');
    formatted[0x13 - 4] = 2; // element_count
    formatted[0x14 - 4] = 3; // element_length
    formatted[0x1B - 4] = 1; // sku_number: string #1

    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_CHASSIS, formatted, { "SKU-42" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_EQ(SmbiosChassis(enumerator.table()).skuNumber(), QString("SKU-42"));
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, ChassisSkuNumberBehindTable)
{
    // The declared contained elements push the SKU number past the end of the table: reading it
    // would leave the formatted area.
    QByteArray formatted(0x1C - 4, '\0');
    formatted[0x13 - 4] = static_cast<char>(0xFF); // element_count
    formatted[0x14 - 4] = static_cast<char>(0xFF); // element_length

    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_CHASSIS, formatted, { "SKU-42" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_TRUE(SmbiosChassis(enumerator.table()).skuNumber().isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, ChassisShortTableOmitsLateFields)
{
    // An SMBIOS 2.0 chassis table (length 09h) carries neither the states nor the height.
    QByteArray formatted(0x09 - 4, '\0');
    formatted[0x05 - 4] = 3; // type: desktop

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_CHASSIS, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosChassis chassis(enumerator.table());
    ASSERT_TRUE(chassis.isValid());
    EXPECT_EQ(chassis.type(), QString("Desktop"));
    EXPECT_FALSE(chassis.isLockPresent());
    EXPECT_TRUE(chassis.bootUpState().isEmpty());
    EXPECT_TRUE(chassis.powerSupplyState().isEmpty());
    EXPECT_TRUE(chassis.thermalState().isEmpty());
    EXPECT_TRUE(chassis.securityStatus().isEmpty());
    EXPECT_TRUE(chassis.skuNumber().isEmpty());
    EXPECT_EQ(chassis.height(), 0u);
    EXPECT_EQ(chassis.powerCords(), 0u);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedChassisIsInvalid)
{
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_CHASSIS, QByteArray(0x08 - 4, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosChassis(enumerator.table()).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, ProcessorFields)
{
    QByteArray formatted(0x28 - 4, '\0');
    formatted[0x04 - 4] = 1;                       // socket_designation: string #1
    formatted[0x05 - 4] = 3;                       // type: central processor
    formatted[0x06 - 4] = static_cast<char>(0xC6); // family: Intel Core i7
    formatted[0x07 - 4] = 2;                       // manufacturer: string #2

    for (int i = 0; i < 8; ++i)
        formatted[0x08 - 4 + i] = static_cast<char>(i + 1); // id

    formatted[0x10 - 4] = 3;                       // version: string #3
    formatted[0x11 - 4] = static_cast<char>(0x8D); // voltage: 1.3 V
    formatted[0x12 - 4] = 100;                     // external_clock: 100 MHz
    formatted[0x14 - 4] = static_cast<char>(0x30); // max_speed: 4400 MHz
    formatted[0x15 - 4] = static_cast<char>(0x11);
    formatted[0x16 - 4] = static_cast<char>(0x10); // current_speed: 3600 MHz
    formatted[0x17 - 4] = static_cast<char>(0x0E);
    formatted[0x18 - 4] = static_cast<char>(0x41); // status: populated and enabled
    formatted[0x19 - 4] = static_cast<char>(0x32); // upgrade: socket LGA1151
    formatted[0x1A - 4] = static_cast<char>(0x11); // l1_cache_handle
    formatted[0x1C - 4] = static_cast<char>(0x12); // l2_cache_handle
    formatted[0x1E - 4] = static_cast<char>(0x13); // l3_cache_handle
    formatted[0x20 - 4] = 4;                       // serial_number: string #4
    formatted[0x21 - 4] = 5;                       // asset_tag: string #5
    formatted[0x22 - 4] = 6;                       // part_number: string #6
    formatted[0x23 - 4] = 8;                       // core_count
    formatted[0x24 - 4] = 8;                       // core_enabled
    formatted[0x25 - 4] = 16;                      // thread_count
    formatted[0x26 - 4] = static_cast<char>(0x0C); // characteristics: 64-bit and multi-core

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_PROCESSOR, formatted,
        { "CPU0", "Intel", "Core i7-9700K", "SN789", "AT321", "PN654" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosProcessor processor(enumerator.table());
    ASSERT_TRUE(processor.isValid());
    EXPECT_TRUE(processor.isPopulated());
    EXPECT_EQ(processor.manufacturer(), QString("Intel"));
    EXPECT_EQ(processor.version(), QString("Core i7-9700K"));
    EXPECT_EQ(processor.serialNumber(), QString("SN789"));
    EXPECT_EQ(processor.assetTag(), QString("AT321"));
    EXPECT_EQ(processor.partNumber(), QString("PN654"));
    EXPECT_EQ(processor.socketDesignation(), QString("CPU0"));
    EXPECT_EQ(processor.type(), QString("Central Processor"));
    EXPECT_EQ(processor.family(), QString("Intel Core i7"));
    EXPECT_EQ(processor.status(), QString("Enabled"));
    EXPECT_EQ(processor.upgrade(), QString("Socket LGA1151"));
    EXPECT_EQ(processor.id(), 0x0807060504030201ULL);
    EXPECT_DOUBLE_EQ(processor.voltage(), 1.3);
    EXPECT_EQ(processor.externalClock(), 100u);
    EXPECT_EQ(processor.maxSpeed(), 4400u);
    EXPECT_EQ(processor.currentSpeed(), 3600u);
    EXPECT_EQ(processor.coreCount(), 8u);
    EXPECT_EQ(processor.coreEnabled(), 8u);
    EXPECT_EQ(processor.threadCount(), 16u);
    EXPECT_EQ(processor.threadEnabled(), 0u); // The table is too short to carry the field.
    EXPECT_EQ(processor.l1CacheHandle(), 0x0011);
    EXPECT_EQ(processor.l2CacheHandle(), 0x0012);
    EXPECT_EQ(processor.l3CacheHandle(), 0x0013);
    EXPECT_TRUE(processor.is64Bit());
    EXPECT_TRUE(processor.isMultiCore());
    EXPECT_FALSE(processor.isHardwareThread());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, ProcessorFamilyFromSecondField)
{
    auto makeProcessor = [](quint8 length) -> QByteArray
    {
        QByteArray formatted(length - 4, '\0');
        formatted[0x06 - 4] = static_cast<char>(0xFE); // family: take the family2 field

        if (length >= 0x2A)
        {
            formatted[0x28 - 4] = static_cast<char>(0x01); // family2: ARMv8 (0x0101)
            formatted[0x29 - 4] = static_cast<char>(0x01);
        }

        return makeTable(SMBIOS_TABLE_TYPE_PROCESSOR, formatted, {});
    };

    auto familyOf = [](const QByteArray& dump) -> QString
    {
        SmbiosTableEnumerator enumerator(dump);
        if (enumerator.isAtEnd())
            return QString("<no table>");
        return SmbiosProcessor(enumerator.table()).family();
    };

    EXPECT_EQ(familyOf(makeDump(makeProcessor(0x2A))), QString("ARMv8"));

    // The table is too short to carry the family2 field.
    EXPECT_TRUE(familyOf(makeDump(makeProcessor(0x28))).isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, ProcessorExtendedCoreCounts)
{
    auto makeProcessor = [](quint8 length) -> QByteArray
    {
        QByteArray formatted(length - 4, '\0');
        formatted[0x23 - 4] = static_cast<char>(0xFF); // core_count: take the core_count2 field
        formatted[0x24 - 4] = static_cast<char>(0xFF); // core_enabled: take core_enabled2
        formatted[0x25 - 4] = static_cast<char>(0xFF); // thread_count: take thread_count2

        if (length >= 0x30)
        {
            formatted[0x2A - 4] = static_cast<char>(0x90); // core_count2: 400
            formatted[0x2B - 4] = static_cast<char>(0x01);
            formatted[0x2C - 4] = static_cast<char>(0x90); // core_enabled2: 400
            formatted[0x2D - 4] = static_cast<char>(0x01);
            formatted[0x2E - 4] = static_cast<char>(0x20); // thread_count2: 800
            formatted[0x2F - 4] = static_cast<char>(0x03);
        }

        return makeTable(SMBIOS_TABLE_TYPE_PROCESSOR, formatted, {});
    };

    const QByteArray extended = makeDump(makeProcessor(0x30));

    SmbiosTableEnumerator extended_enumerator(extended);
    ASSERT_FALSE(extended_enumerator.isAtEnd());

    SmbiosProcessor extended_processor(extended_enumerator.table());
    EXPECT_EQ(extended_processor.coreCount(), 400u);
    EXPECT_EQ(extended_processor.coreEnabled(), 400u);
    EXPECT_EQ(extended_processor.threadCount(), 800u);

    // An SMBIOS 2.5 table cannot tell the counts the byte-sized fields overflowed on.
    const QByteArray legacy = makeDump(makeProcessor(0x28));

    SmbiosTableEnumerator legacy_enumerator(legacy);
    ASSERT_FALSE(legacy_enumerator.isAtEnd());

    SmbiosProcessor legacy_processor(legacy_enumerator.table());
    EXPECT_EQ(legacy_processor.coreCount(), 0u);
    EXPECT_EQ(legacy_processor.coreEnabled(), 0u);
    EXPECT_EQ(legacy_processor.threadCount(), 0u);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, ProcessorShortTableOmitsLateFields)
{
    // An SMBIOS 2.0 processor table (length 1Ah) with the legacy voltage encoding, which lists
    // the supported voltages instead of the one in use.
    QByteArray formatted(0x1A - 4, '\0');
    formatted[0x11 - 4] = 0x03; // voltage: 5.0 V and 3.3 V capable

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_PROCESSOR, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosProcessor processor(enumerator.table());
    ASSERT_TRUE(processor.isValid());
    EXPECT_DOUBLE_EQ(processor.voltage(), 0.0);
    EXPECT_TRUE(processor.serialNumber().isEmpty());
    EXPECT_TRUE(processor.assetTag().isEmpty());
    EXPECT_TRUE(processor.partNumber().isEmpty());
    EXPECT_EQ(processor.coreCount(), 0u);
    EXPECT_EQ(processor.coreEnabled(), 0u);
    EXPECT_EQ(processor.threadCount(), 0u);
    EXPECT_EQ(processor.threadEnabled(), 0u);
    EXPECT_EQ(processor.l1CacheHandle(), 0xFFFF); // The cache handles appeared in SMBIOS 2.1.
    EXPECT_EQ(processor.l2CacheHandle(), 0xFFFF);
    EXPECT_EQ(processor.l3CacheHandle(), 0xFFFF);
    EXPECT_FALSE(processor.is64Bit());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, ProcessorUnknownCharacteristics)
{
    // Bit 1 marks the characteristics as unknown: the remaining bits must not be reported.
    QByteArray formatted(0x28 - 4, '\0');
    formatted[0x26 - 4] = static_cast<char>(0x06); // characteristics: unknown, but 64-bit set

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_PROCESSOR, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosProcessor(enumerator.table()).is64Bit());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedProcessorIsInvalid)
{
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_PROCESSOR, QByteArray(0x19 - 4, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosProcessor(enumerator.table()).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, CacheFields)
{
    QByteArray formatted(0x13 - 4, '\0');
    formatted[0x04 - 4] = 1;                       // socket_designation: string #1
    formatted[0x05 - 4] = static_cast<char>(0x89); // configuration: L2, socketed, enabled
    formatted[0x06 - 4] = static_cast<char>(0x01); // configuration: write back mode
    formatted[0x07 - 4] = static_cast<char>(0x00); // max_size: 512 KB with 1K granularity
    formatted[0x08 - 4] = static_cast<char>(0x02);
    formatted[0x09 - 4] = static_cast<char>(0x00); // current_size: 512 KB
    formatted[0x0A - 4] = static_cast<char>(0x02);
    formatted[0x0B - 4] = static_cast<char>(0x30); // supported_sram_type: pipeline burst, sync
    formatted[0x0D - 4] = static_cast<char>(0x20); // current_sram_type: synchronous
    formatted[0x0F - 4] = 10;                      // speed: 10 ns
    formatted[0x10 - 4] = 5;                       // error_correction_type: single-bit ECC
    formatted[0x11 - 4] = 5;                       // type: unified
    formatted[0x12 - 4] = 7;                       // associativity: 8-way set-associative

    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_CACHE, formatted, { "L2 Cache" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosCache cache(enumerator.table());
    ASSERT_TRUE(cache.isValid());
    EXPECT_TRUE(cache.isEnabled());
    EXPECT_TRUE(cache.isSocketed());
    EXPECT_EQ(cache.designation(), QString("L2 Cache"));
    EXPECT_EQ(cache.location(), QString("Internal"));
    EXPECT_EQ(cache.mode(), QString("Write Back"));
    EXPECT_EQ(cache.type(), QString("Unified"));
    EXPECT_EQ(cache.errorCorrectionType(), QString("Single-bit ECC"));
    EXPECT_EQ(cache.associativity(), QString("8-way Set-Associative"));
    EXPECT_EQ(cache.currentSramType(), QString("Synchronous"));
    EXPECT_EQ(cache.level(), 2);
    EXPECT_EQ(cache.maxSize(), 512ULL * 1024ULL);
    EXPECT_EQ(cache.currentSize(), 512ULL * 1024ULL);
    EXPECT_EQ(cache.speed(), 10u);
    EXPECT_FALSE(cache.supportsNonBurst());
    EXPECT_FALSE(cache.supportsBurst());
    EXPECT_TRUE(cache.supportsPipelineBurst());
    EXPECT_TRUE(cache.supportsSynchronous());
    EXPECT_FALSE(cache.supportsAsynchronous());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, CacheExtendedSize)
{
    auto makeCache = [](quint8 length) -> QByteArray
    {
        QByteArray formatted(length - 4, '\0');
        formatted[0x07 - 4] = static_cast<char>(0xFF); // max_size: see the max_size2 field
        formatted[0x08 - 4] = static_cast<char>(0xFF);
        formatted[0x09 - 4] = static_cast<char>(0xFF); // current_size: see current_size2
        formatted[0x0A - 4] = static_cast<char>(0xFF);

        if (length >= 0x1B)
        {
            // 1024 units of 64K each: 64 MB.
            formatted[0x14 - 4] = static_cast<char>(0x04); // max_size2
            formatted[0x16 - 4] = static_cast<char>(0x80);
            formatted[0x18 - 4] = static_cast<char>(0x04); // current_size2
            formatted[0x1A - 4] = static_cast<char>(0x80);
        }

        return makeTable(SMBIOS_TABLE_TYPE_CACHE, formatted, {});
    };

    const QByteArray extended = makeDump(makeCache(0x1B));

    SmbiosTableEnumerator extended_enumerator(extended);
    ASSERT_FALSE(extended_enumerator.isAtEnd());

    SmbiosCache extended_cache(extended_enumerator.table());
    EXPECT_EQ(extended_cache.maxSize(), 64ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(extended_cache.currentSize(), 64ULL * 1024ULL * 1024ULL);

    // An SMBIOS 2.1 table cannot tell the size the 16-bit fields overflowed on.
    const QByteArray legacy = makeDump(makeCache(0x13));

    SmbiosTableEnumerator legacy_enumerator(legacy);
    ASSERT_FALSE(legacy_enumerator.isAtEnd());

    SmbiosCache legacy_cache(legacy_enumerator.table());
    EXPECT_EQ(legacy_cache.maxSize(), 0ULL);
    EXPECT_EQ(legacy_cache.currentSize(), 0ULL);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, CacheShortTableOmitsLateFields)
{
    // An SMBIOS 2.0 cache table (length 0Fh) carries neither the speed nor the type fields.
    QByteArray formatted(0x0F - 4, '\0');
    formatted[0x05 - 4] = static_cast<char>(0x22); // configuration: L3, external location
    formatted[0x07 - 4] = static_cast<char>(0x80); // max_size: 128 units of 64K each, 8 MB
    formatted[0x08 - 4] = static_cast<char>(0x80);

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_CACHE, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosCache cache(enumerator.table());
    ASSERT_TRUE(cache.isValid());
    EXPECT_EQ(cache.level(), 3);
    EXPECT_EQ(cache.location(), QString("External"));
    EXPECT_FALSE(cache.isEnabled());
    EXPECT_EQ(cache.maxSize(), 8ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(cache.currentSize(), 0ULL);
    EXPECT_EQ(cache.speed(), 0u);
    EXPECT_TRUE(cache.type().isEmpty());
    EXPECT_TRUE(cache.errorCorrectionType().isEmpty());
    EXPECT_TRUE(cache.associativity().isEmpty());
    EXPECT_TRUE(cache.currentSramType().isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedCacheIsInvalid)
{
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_CACHE, QByteArray(0x0E - 4, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosCache(enumerator.table()).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, PortConnectorFields)
{
    QByteArray formatted(0x09 - 4, '\0');
    formatted[0x04 - 4] = 1;                       // internal_designator: string #1
    formatted[0x05 - 4] = static_cast<char>(0x16); // internal_connector: on board IDE
    formatted[0x06 - 4] = 2;                       // external_designator: string #2
    formatted[0x07 - 4] = static_cast<char>(0x12); // external_connector: access bus (USB)
    formatted[0x08 - 4] = static_cast<char>(0x10); // type: USB

    const QByteArray dump = makeDump(
        makeTable(SMBIOS_TABLE_TYPE_PORT_CONNECTOR, formatted, { "J1A1", "USB1" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosPortConnector port(enumerator.table());
    ASSERT_TRUE(port.isValid());
    EXPECT_EQ(port.internalDesignator(), QString("J1A1"));
    EXPECT_EQ(port.internalConnectorType(), QString("On Board IDE"));
    EXPECT_EQ(port.externalDesignator(), QString("USB1"));
    EXPECT_EQ(port.externalConnectorType(), QString("Access Bus (USB)"));
    EXPECT_EQ(port.type(), QString("USB"));
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, PortConnectorOutOfRangeValues)
{
    // FFh means 'other' for both the connectors and the port type, while the ranges above A0h
    // hold the legacy PC-98 and 8251 values.
    QByteArray formatted(0x09 - 4, '\0');
    formatted[0x05 - 4] = static_cast<char>(0xFF); // internal_connector: other
    formatted[0x07 - 4] = static_cast<char>(0xA0); // external_connector: PC-98
    formatted[0x08 - 4] = static_cast<char>(0xA1); // type: 8251 FIFO compatible

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_PORT_CONNECTOR, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosPortConnector port(enumerator.table());
    EXPECT_EQ(port.internalConnectorType(), QString("Other"));
    EXPECT_EQ(port.externalConnectorType(), QString("PC-98"));
    EXPECT_EQ(port.type(), QString("8251 FIFO Compatible"));
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedPortConnectorIsInvalid)
{
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_PORT_CONNECTOR, QByteArray(0x08 - 4, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosPortConnector(enumerator.table()).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, SystemSlotFields)
{
    QByteArray formatted(0x11 - 4, '\0');
    formatted[0x04 - 4] = 1;                       // designation: string #1
    formatted[0x05 - 4] = static_cast<char>(0x1C); // type: PCI Express x16
    formatted[0x06 - 4] = static_cast<char>(0x0D); // data_bus_width: x16
    formatted[0x07 - 4] = 3;                       // usage: available
    formatted[0x08 - 4] = 4;                       // slot_length: long
    formatted[0x09 - 4] = 5;                       // id
    formatted[0x0B - 4] = static_cast<char>(0x0C); // characteristics1: 3.3 V, shared
    formatted[0x0C - 4] = static_cast<char>(0x0A); // characteristics2: hot-plug, bifurcation
    formatted[0x0F - 4] = 3;                       // bus_number
    formatted[0x10 - 4] = static_cast<char>(0x08); // device_function: device 1, function 0

    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_SYSTEM_SLOT, formatted, { "PCIEX16_1" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosSystemSlot slot(enumerator.table());
    ASSERT_TRUE(slot.isValid());
    EXPECT_EQ(slot.designation(), QString("PCIEX16_1"));
    EXPECT_EQ(slot.type(), QString("PCI Express x16"));
    EXPECT_EQ(slot.dataBusWidth(), QString("x16"));
    EXPECT_EQ(slot.usage(), QString("Available"));
    EXPECT_EQ(slot.length(), QString("Long Length"));
    EXPECT_EQ(slot.id(), 5);
    EXPECT_TRUE(slot.hasBusAddress());
    EXPECT_EQ(slot.segmentGroupNumber(), 0);
    EXPECT_EQ(slot.busNumber(), 3);
    EXPECT_EQ(slot.deviceNumber(), 1);
    EXPECT_EQ(slot.functionNumber(), 0);
    EXPECT_FALSE(slot.provides5Volts());
    EXPECT_TRUE(slot.provides3Volts());
    EXPECT_TRUE(slot.isShared());
    EXPECT_FALSE(slot.supportsPme());
    EXPECT_TRUE(slot.supportsHotPlug());
    EXPECT_FALSE(slot.supportsSmbus());
    EXPECT_TRUE(slot.supportsBifurcation());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, SystemSlotLegacyTypeRange)
{
    // Some firmware still reports PCI Express slots in the legacy A0h-B6h range.
    QByteArray formatted(0x0C - 4, '\0');
    formatted[0x05 - 4] = static_cast<char>(0xA5); // type: PCI Express

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_SYSTEM_SLOT, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_EQ(SmbiosSystemSlot(enumerator.table()).type(), QString("PCI Express"));
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, SystemSlotUnknownCharacteristics)
{
    // Bit 0 marks the characteristics as unknown, and the bus address is not applicable.
    QByteArray formatted(0x11 - 4, '\0');
    formatted[0x0B - 4] = static_cast<char>(0x03); // characteristics1: unknown, but 5 V set
    formatted[0x0C - 4] = static_cast<char>(0x02); // characteristics2: hot-plug
    formatted[0x0D - 4] = static_cast<char>(0xFF); // segment_group
    formatted[0x0E - 4] = static_cast<char>(0xFF);
    formatted[0x0F - 4] = static_cast<char>(0xFF); // bus_number
    formatted[0x10 - 4] = static_cast<char>(0xFF); // device_function

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_SYSTEM_SLOT, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosSystemSlot slot(enumerator.table());
    ASSERT_TRUE(slot.isValid());
    EXPECT_FALSE(slot.provides5Volts());
    EXPECT_FALSE(slot.supportsHotPlug());
    EXPECT_FALSE(slot.hasBusAddress());
    EXPECT_EQ(slot.busNumber(), 0);
    EXPECT_EQ(slot.deviceNumber(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, SystemSlotShortTableOmitsLateFields)
{
    // An SMBIOS 2.0 slot table (length 0Ch) carries neither the second characteristics byte nor
    // the bus address.
    QByteArray formatted(0x0C - 4, '\0');
    formatted[0x0B - 4] = static_cast<char>(0x02); // characteristics1: 5 V

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_SYSTEM_SLOT, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosSystemSlot slot(enumerator.table());
    ASSERT_TRUE(slot.isValid());
    EXPECT_TRUE(slot.provides5Volts());
    EXPECT_FALSE(slot.supportsPme());
    EXPECT_FALSE(slot.supportsHotPlug());
    EXPECT_FALSE(slot.supportsSmbus());
    EXPECT_FALSE(slot.supportsBifurcation());
    EXPECT_FALSE(slot.hasBusAddress());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedSystemSlotIsInvalid)
{
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_SYSTEM_SLOT, QByteArray(0x0B - 4, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosSystemSlot(enumerator.table()).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, MemoryArrayFields)
{
    QByteArray formatted(0x0F - 4, '\0');
    formatted[0x04 - 4] = 3;                       // location: system board
    formatted[0x05 - 4] = 3;                       // use: system memory
    formatted[0x06 - 4] = 3;                       // error_correction: none
    formatted[0x0A - 4] = static_cast<char>(0x02); // max_capacity: 32 GB in kilobytes
    formatted[0x0D - 4] = 4;                       // device_count

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_MEMORY_ARRAY, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosMemoryArray array(enumerator.table());
    ASSERT_TRUE(array.isValid());
    EXPECT_EQ(array.location(), QString("System Board Or Motherboard"));
    EXPECT_EQ(array.use(), QString("System Memory"));
    EXPECT_EQ(array.errorCorrection(), QString("None"));
    EXPECT_EQ(array.maxCapacity(), 32ULL * 1024ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(array.deviceCount(), 4);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, MemoryArrayExtendedCapacity)
{
    auto makeArray = [](quint8 length) -> QByteArray
    {
        QByteArray formatted(length - 4, '\0');

        // max_capacity: the capacity does not fit the field.
        formatted[0x0A - 4] = static_cast<char>(0x80);

        if (length >= 0x17)
        {
            // ext_max_capacity: 4 TB in bytes.
            formatted[0x14 - 4] = static_cast<char>(0x04);
        }

        return makeTable(SMBIOS_TABLE_TYPE_MEMORY_ARRAY, formatted, {});
    };

    const QByteArray extended = makeDump(makeArray(0x17));

    SmbiosTableEnumerator extended_enumerator(extended);
    ASSERT_FALSE(extended_enumerator.isAtEnd());

    EXPECT_EQ(SmbiosMemoryArray(extended_enumerator.table()).maxCapacity(),
              4ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL);

    // An SMBIOS 2.1 table cannot tell the capacity the 32-bit field overflowed on.
    const QByteArray legacy = makeDump(makeArray(0x0F));

    SmbiosTableEnumerator legacy_enumerator(legacy);
    ASSERT_FALSE(legacy_enumerator.isAtEnd());

    EXPECT_EQ(SmbiosMemoryArray(legacy_enumerator.table()).maxCapacity(), 0ULL);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedMemoryArrayIsInvalid)
{
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_MEMORY_ARRAY, QByteArray(0x0E - 4, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosMemoryArray(enumerator.table()).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, MemoryDeviceFields)
{
    QByteArray formatted(0x15 - 4, '\0');
    formatted[0x04 - 4] = static_cast<char>(0x21); // memory_array_handle
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
    EXPECT_EQ(device.arrayHandle(), 0x0021);
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

    auto presentOf = [](const QByteArray& dump) -> bool
    {
        SmbiosTableEnumerator enumerator(dump);
        if (enumerator.isAtEnd())
            return false;
        return SmbiosMemoryDevice(enumerator.table()).isPresent();
    };

    // Size in kB (bit 15 set): 1024 kB.
    EXPECT_EQ(sizeOf(makeDump(makeDevice(0x8000 | 1024, 0, 0x15))), 1024ULL * 1024ULL);

    // An empty socket: no device, zero size.
    EXPECT_EQ(sizeOf(makeDump(makeDevice(0, 0, 0x15))), 0ULL);
    EXPECT_FALSE(presentOf(makeDump(makeDevice(0, 0, 0x15))));

    // 0xFFFF: the device is present, but its size is unknown.
    EXPECT_EQ(sizeOf(makeDump(makeDevice(0xFFFF, 0, 0x15))), 0ULL);
    EXPECT_TRUE(presentOf(makeDump(makeDevice(0xFFFF, 0, 0x15))));

    // 0x7FFF points to the extended size field, but the table is too short to carry it.
    EXPECT_EQ(sizeOf(makeDump(makeDevice(0x7FFF, 0, 0x15))), 0ULL);

    // Extended size: 32768 MB with zero low bits selects the GB branch (32 GB).
    EXPECT_EQ(sizeOf(makeDump(makeDevice(0x7FFF, 32768, 0x20))),
              32ULL * 1024ULL * 1024ULL * 1024ULL);
}
