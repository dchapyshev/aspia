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
TEST(SmbiosParserTest, BiosAddressAndRomSize)
{
    QByteArray formatted(0x12 - 4, '\0');
    formatted[2] = static_cast<char>(0x00); // address_segment: E800h
    formatted[3] = static_cast<char>(0xE8);
    formatted[5] = static_cast<char>(0x0F); // rom_size: 16 blocks of 64K

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_BIOS, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosBios bios(enumerator.table());
    ASSERT_TRUE(bios.isValid());
    EXPECT_EQ(bios.address(), 0xE8000u);
    EXPECT_EQ(bios.romSize(), 1024ull * 1024);

    // The releases appeared in SMBIOS 2.4 and a 2.0 table carries none of them.
    EXPECT_TRUE(bios.revision().isEmpty());
    EXPECT_TRUE(bios.firmwareRevision().isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, BiosRomSizeExtended)
{
    auto makeBios = [](int length, quint16 ext_rom_size)
    {
        QByteArray formatted(length - 4, '\0');
        formatted[5] = static_cast<char>(0xFF); // rom_size: does not fit the byte

        if (formatted.size() > 21)
        {
            formatted[20] = static_cast<char>(ext_rom_size & 0xFF);
            formatted[21] = static_cast<char>(ext_rom_size >> 8);
        }

        return makeDump(makeTable(SMBIOS_TABLE_TYPE_BIOS, formatted, {}));
    };

    auto romSizeOf = [](const QByteArray& dump)
    {
        SmbiosTableEnumerator enumerator(dump);
        if (enumerator.isAtEnd())
            return quint64(0);

        return SmbiosBios(enumerator.table()).romSize();
    };

    // The two upper bits of the extended field carry the unit of the size.
    EXPECT_EQ(romSizeOf(makeBios(0x1A, 32)), 32ull * 1024 * 1024);
    EXPECT_EQ(romSizeOf(makeBios(0x1A, 0x4000 | 4)), 4ull * 1024 * 1024 * 1024);

    // A reserved unit leaves the size unknown.
    EXPECT_EQ(romSizeOf(makeBios(0x1A, 0x8000 | 8)), 0ull);

    // Firmware older than SMBIOS 3.1 has no extended field to read.
    EXPECT_EQ(romSizeOf(makeBios(0x18, 32)), 0ull);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, BiosRevisions)
{
    auto makeBios = [](int length, quint8 ctrl_major_release)
    {
        QByteArray formatted(length - 4, '\0');
        formatted[16] = static_cast<char>(5);  // major_release
        formatted[17] = static_cast<char>(13); // minor_release

        if (formatted.size() > 19)
        {
            formatted[18] = static_cast<char>(ctrl_major_release);
            formatted[19] = static_cast<char>(2); // ctrl_minor_release
        }

        return makeDump(makeTable(SMBIOS_TABLE_TYPE_BIOS, formatted, {}));
    };

    const QByteArray with_controller = makeBios(0x18, 1);
    SmbiosTableEnumerator with_controller_enumerator(with_controller);
    ASSERT_FALSE(with_controller_enumerator.isAtEnd());

    SmbiosBios with_controller_bios(with_controller_enumerator.table());
    EXPECT_EQ(with_controller_bios.revision(), QString("5.13"));
    EXPECT_EQ(with_controller_bios.firmwareRevision(), QString("1.2"));

    // FFh in the major byte means the system has no embedded controller.
    const QByteArray without_controller = makeBios(0x18, 0xFF);
    SmbiosTableEnumerator without_controller_enumerator(without_controller);
    ASSERT_FALSE(without_controller_enumerator.isAtEnd());

    SmbiosBios without_controller_bios(without_controller_enumerator.table());
    EXPECT_EQ(without_controller_bios.revision(), QString("5.13"));
    EXPECT_TRUE(without_controller_bios.firmwareRevision().isEmpty());

    // A table that ends before the bytes of the controller.
    const QByteArray truncated = makeBios(0x16, 1);
    SmbiosTableEnumerator truncated_enumerator(truncated);
    ASSERT_FALSE(truncated_enumerator.isAtEnd());

    SmbiosBios truncated_bios(truncated_enumerator.table());
    EXPECT_EQ(truncated_bios.revision(), QString("5.13"));
    EXPECT_TRUE(truncated_bios.firmwareRevision().isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, BiosCharacteristics)
{
    QByteArray formatted(0x14 - 4, '\0');
    formatted[6] = static_cast<char>(0x80);  // characters bits 0-7: PCI (bit 7)
    formatted[7] = static_cast<char>(0x88);  // bits 8-15: upgradeable (11), boot from CD (15)
    formatted[14] = static_cast<char>(0x01); // extension byte 1: ACPI
    formatted[15] = static_cast<char>(0x08); // extension byte 2: UEFI

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_BIOS, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    const QStringList characteristics = SmbiosBios(enumerator.table()).characteristics();

    ASSERT_EQ(characteristics.size(), 5);
    EXPECT_TRUE(characteristics.contains("PCI is supported"));
    EXPECT_TRUE(characteristics.contains("BIOS is upgradeable"));
    EXPECT_TRUE(characteristics.contains("Boot from CD is supported"));
    EXPECT_TRUE(characteristics.contains("ACPI is supported"));
    EXPECT_TRUE(characteristics.contains("UEFI specification is supported"));
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, BiosCharacteristicsNotSupported)
{
    QByteArray formatted(0x14 - 4, '\0');

    // Bit 3 tells that the firmware fills none of the bits above it, bit 7 is set anyway.
    formatted[6] = static_cast<char>(0x88);
    formatted[15] = static_cast<char>(0x10); // extension byte 2: the system is a virtual machine

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_BIOS, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    // The bits of the main field are dropped, the extension bytes still count.
    const QStringList characteristics = SmbiosBios(enumerator.table()).characteristics();

    ASSERT_EQ(characteristics.size(), 1);
    EXPECT_EQ(characteristics[0], QString("The system is a virtual machine"));
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, BiosCharacteristicsWithoutExtensionBytes)
{
    // A 2.0 table has no extension bytes: the string area sits where they would be and must not
    // be read as features.
    QByteArray formatted(0x12 - 4, '\0');
    formatted[0] = 1; // vendor: string #1

    const QByteArray dump = makeDump(
        makeTable(SMBIOS_TABLE_TYPE_BIOS, formatted, { "AMI" }) + endOfTable());

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosBios bios(enumerator.table());
    ASSERT_EQ(bios.vendor(), QString("AMI"));
    EXPECT_TRUE(bios.characteristics().isEmpty());
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
TEST(SmbiosParserTest, BaseboardFields)
{
    QByteArray formatted(0x0F - 4, '\0');
    formatted[0] = 1;                       // manufacturer: string #1
    formatted[1] = 2;                       // product: string #2
    formatted[2] = 3;                       // version: string #3
    formatted[3] = 4;                       // serial_number: string #4
    formatted[4] = 5;                       // asset_tag: string #5
    formatted[5] = static_cast<char>(0x09); // feature_flags: hosting board, replaceable
    formatted[6] = 6;                       // location: string #6
    formatted[9] = static_cast<char>(0x0A); // board_type: motherboard

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_BASEBOARD, formatted,
        { "ASUS", "PRIME B550", "Rev X.0x", "SN-1", "Tag-1", "Base Board" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosBaseboard baseboard(enumerator.table());
    ASSERT_TRUE(baseboard.isValid());
    EXPECT_EQ(baseboard.manufacturer(), QString("ASUS"));
    EXPECT_EQ(baseboard.product(), QString("PRIME B550"));
    EXPECT_EQ(baseboard.version(), QString("Rev X.0x"));
    EXPECT_EQ(baseboard.serialNumber(), QString("SN-1"));
    EXPECT_EQ(baseboard.assetTag(), QString("Tag-1"));
    EXPECT_EQ(baseboard.location(), QString("Base Board"));
    EXPECT_EQ(baseboard.type(), QString("Motherboard"));
    EXPECT_TRUE(baseboard.isHostingBoard());
    EXPECT_TRUE(baseboard.isReplaceable());
    EXPECT_FALSE(baseboard.requiresDaughterBoard());
    EXPECT_FALSE(baseboard.isRemovable());
    EXPECT_FALSE(baseboard.isHotSwappable());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, BaseboardWithoutOptionalFields)
{
    // A table that ends right after the serial number: the string area sits where the remaining
    // fields would be and must not be read as them.
    QByteArray formatted(0x08 - 4, '\0');
    formatted[0] = 1; // manufacturer: string #1

    const QByteArray dump = makeDump(
        makeTable(SMBIOS_TABLE_TYPE_BASEBOARD, formatted, { "ASUS" }) + endOfTable());

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosBaseboard baseboard(enumerator.table());
    ASSERT_TRUE(baseboard.isValid());
    EXPECT_EQ(baseboard.manufacturer(), QString("ASUS"));
    EXPECT_TRUE(baseboard.assetTag().isEmpty());
    EXPECT_TRUE(baseboard.location().isEmpty());
    EXPECT_TRUE(baseboard.type().isEmpty());
    EXPECT_FALSE(baseboard.isHostingBoard());
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
TEST(SmbiosParserTest, ProcessorModernFields)
{
    // A full SMBIOS 3.8 table: a current Intel part sits in the family2 range added by the
    // later versions of the specification, and the socket also comes as a string.
    QByteArray formatted(0x33 - 4, '\0');
    formatted[0x06 - 4] = static_cast<char>(0xFE); // family: take the family2 field
    formatted[0x19 - 4] = static_cast<char>(0x55); // upgrade: socket LGA1851
    formatted[0x28 - 4] = static_cast<char>(0x06); // family2: Intel Core Ultra 7 (0x0306)
    formatted[0x29 - 4] = static_cast<char>(0x03);
    formatted[0x32 - 4] = 1;                       // socket_type: string #1

    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_PROCESSOR, formatted, { "LGA1851" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosProcessor processor(enumerator.table());
    ASSERT_TRUE(processor.isValid());
    EXPECT_EQ(processor.family(), QString("Intel Core Ultra 7"));
    EXPECT_EQ(processor.upgrade(), QString("Socket LGA1851"));
    EXPECT_EQ(processor.socketType(), QString("LGA1851"));
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
    formatted[0x05 - 4] = static_cast<char>(0xBD); // type: PCI Express Gen 4 x16
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
    EXPECT_EQ(slot.type(), QString("PCI Express Gen 4 x16"));
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
TEST(SmbiosParserTest, SystemSlotTypeRanges)
{
    // The slot types are scattered over four ranges with gaps between them.
    auto typeOf = [](quint8 type) -> QString
    {
        QByteArray formatted(0x0C - 4, '\0');
        formatted[0x05 - 4] = static_cast<char>(type);

        const QByteArray dump =
            makeDump(makeTable(SMBIOS_TABLE_TYPE_SYSTEM_SLOT, formatted, {}));

        SmbiosTableEnumerator enumerator(dump);
        if (enumerator.isAtEnd())
            return QString("<no table>");
        return SmbiosSystemSlot(enumerator.table()).type();
    };

    EXPECT_EQ(typeOf(0x12), QString("PCI-X"));
    EXPECT_EQ(typeOf(0x17), QString("M.2 Socket 3"));
    EXPECT_EQ(typeOf(0x28), QString("OCP NIC Prior to 3.0"));
    EXPECT_EQ(typeOf(0x30), QString("CXL Flexbus 1.0"));
    EXPECT_EQ(typeOf(0xA5), QString("PCI Express"));
    EXPECT_EQ(typeOf(0xB6), QString("PCI Express Gen 3 x16"));
    EXPECT_EQ(typeOf(0xBE), QString("PCI Express Gen 5"));
    EXPECT_EQ(typeOf(0xC6), QString("EDSFF E3 Form Factor"));

    // The gaps between the ranges hold no type.
    EXPECT_TRUE(typeOf(0x29).isEmpty());
    EXPECT_TRUE(typeOf(0x9F).isEmpty());
    EXPECT_TRUE(typeOf(0xB7).isEmpty());
    EXPECT_TRUE(typeOf(0xC7).isEmpty());
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
TEST(SmbiosParserTest, OnBoardDevicesFields)
{
    // Two devices in a single table: an enabled video adapter and a disabled sound controller.
    QByteArray formatted(0x08 - 4, '\0');
    formatted[0x04 - 4] = static_cast<char>(0x83); // device_type: video, enabled
    formatted[0x05 - 4] = 1;                       // description: string #1
    formatted[0x06 - 4] = static_cast<char>(0x07); // device_type: sound, disabled
    formatted[0x07 - 4] = 2;                       // description: string #2

    const QByteArray dump = makeDump(makeTable(
        SMBIOS_TABLE_TYPE_ONBOARD_DEVICE, formatted, { "Intel UHD 630", "Realtek ALC1220" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosOnBoardDevices devices(enumerator.table());
    ASSERT_TRUE(devices.isValid());
    ASSERT_EQ(devices.count(), 2);
    EXPECT_EQ(devices.type(0), QString("Video"));
    EXPECT_EQ(devices.description(0), QString("Intel UHD 630"));
    EXPECT_TRUE(devices.isEnabled(0));
    EXPECT_EQ(devices.type(1), QString("Sound"));
    EXPECT_EQ(devices.description(1), QString("Realtek ALC1220"));
    EXPECT_FALSE(devices.isEnabled(1));

    // Indexes outside the table must not reach the memory behind it.
    EXPECT_TRUE(devices.type(2).isEmpty());
    EXPECT_TRUE(devices.description(2).isEmpty());
    EXPECT_FALSE(devices.isEnabled(2));
    EXPECT_TRUE(devices.type(-1).isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedOnBoardDevicesIsInvalid)
{
    // A table without a single complete device pair.
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_ONBOARD_DEVICE, QByteArray(1, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosOnBoardDevices devices(enumerator.table());
    EXPECT_FALSE(devices.isValid());
    EXPECT_EQ(devices.count(), 0);
    EXPECT_TRUE(devices.type(0).isEmpty());
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
    formatted[0x08 - 4] = static_cast<char>(0x48); // total_width: 72 bits
    formatted[0x0A - 4] = static_cast<char>(0x40); // data_width: 64 bits
    formatted[0x0C - 4] = static_cast<char>(0x00); // module_size low byte: 2048 MB
    formatted[0x0D - 4] = static_cast<char>(0x08); // module_size high byte
    formatted[0x0E - 4] = static_cast<char>(0x09); // form_factor: DIMM
    formatted[0x10 - 4] = 1;                       // device_location: string #1
    formatted[0x11 - 4] = 2;                       // bank_locator: string #2
    formatted[0x12 - 4] = static_cast<char>(0x1A); // memory_type: DDR4

    const QByteArray dump = makeDump(
        makeTable(SMBIOS_TABLE_TYPE_MEMORY_DEVICE, formatted, { "DIMM_A1", "BANK 0" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosMemoryDevice device(enumerator.table());
    ASSERT_TRUE(device.isValid());
    EXPECT_TRUE(device.isPresent());
    EXPECT_EQ(device.size(), 2048ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(device.location(), QString("DIMM_A1"));
    EXPECT_EQ(device.bankLocator(), QString("BANK 0"));
    EXPECT_EQ(device.type(), QString("DDR4"));
    EXPECT_EQ(device.formFactor(), QString("DIMM"));
    EXPECT_EQ(device.totalWidth(), 72);
    EXPECT_EQ(device.dataWidth(), 64);
    EXPECT_EQ(device.arrayHandle(), 0x0021);

    // The table is too short to carry any of the fields below.
    EXPECT_EQ(device.speed(), 0u);
    EXPECT_EQ(device.configuredSpeed(), 0u);
    EXPECT_EQ(device.rank(), 0);
    EXPECT_EQ(device.configuredVoltage(), 0u);
    EXPECT_TRUE(device.serialNumber().isEmpty());
    EXPECT_TRUE(device.assetTag().isEmpty());
    EXPECT_TRUE(device.technology().isEmpty());
    EXPECT_TRUE(device.firmwareVersion().isEmpty());
    EXPECT_EQ(device.nonVolatileSize(), 0ULL);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, MemoryDeviceModernFields)
{
    // A full SMBIOS 3.7 table (length 64h) with the fields the later versions brought in.
    QByteArray formatted(0x64 - 4, '\0');
    formatted[0x0C - 4] = static_cast<char>(0xFF); // module_size: see the extended field
    formatted[0x0D - 4] = static_cast<char>(0x7F);
    formatted[0x0E - 4] = static_cast<char>(0x12); // form_factor: CUDIMM
    formatted[0x10 - 4] = 1;                       // device_location: string #1
    formatted[0x11 - 4] = 2;                       // bank_locator: string #2
    formatted[0x12 - 4] = static_cast<char>(0x25); // memory_type: MRDIMM
    formatted[0x14 - 4] = static_cast<char>(0x20); // type_detail: registered
    formatted[0x15 - 4] = static_cast<char>(0xFF); // speed: see the extended field
    formatted[0x16 - 4] = static_cast<char>(0xFF);
    formatted[0x17 - 4] = 3;                       // manufacturer: string #3
    formatted[0x18 - 4] = 4;                       // serial_number: string #4
    formatted[0x19 - 4] = 5;                       // asset_tag: string #5
    formatted[0x1A - 4] = 6;                       // part_number: string #6
    formatted[0x1B - 4] = 2;                       // attributes: rank 2
    formatted[0x1C - 4] = static_cast<char>(0x00); // ext_size: 32768 MB
    formatted[0x1D - 4] = static_cast<char>(0x80);
    formatted[0x20 - 4] = static_cast<char>(0xFF); // configured_speed: see the extended field
    formatted[0x21 - 4] = static_cast<char>(0xFF);
    formatted[0x22 - 4] = static_cast<char>(0x4C); // min_voltage: 1100 mV
    formatted[0x23 - 4] = static_cast<char>(0x04);
    formatted[0x24 - 4] = static_cast<char>(0x4C); // max_voltage: 1100 mV
    formatted[0x25 - 4] = static_cast<char>(0x04);
    formatted[0x26 - 4] = static_cast<char>(0x4C); // configured_voltage: 1100 mV
    formatted[0x27 - 4] = static_cast<char>(0x04);
    formatted[0x28 - 4] = 4;                       // technology: NVDIMM-N
    formatted[0x2B - 4] = 7;                       // firmware_version: string #7
    formatted[0x38 - 4] = static_cast<char>(0x20); // non_volatile_size: 128 GB
    formatted[0x40 - 4] = static_cast<char>(0x04); // volatile_size: 16 GB

    for (int i = 0; i < 8; ++i)
        formatted[0x44 - 4 + i] = static_cast<char>(0xFF); // cache_size: unknown

    formatted[0x54 - 4] = static_cast<char>(0x00); // ext_speed: 12800 MT/s
    formatted[0x55 - 4] = static_cast<char>(0x32);
    formatted[0x58 - 4] = static_cast<char>(0xE0); // ext_configured_speed: 12000 MT/s
    formatted[0x59 - 4] = static_cast<char>(0x2E);

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_MEMORY_DEVICE, formatted,
        { "DIMM_A1", "BANK 0", "Micron", "SN123", "AT1", "PN-XYZ", "FW-1.2" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosMemoryDevice device(enumerator.table());
    ASSERT_TRUE(device.isValid());
    EXPECT_EQ(device.size(), 32ULL * 1024ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(device.formFactor(), QString("CUDIMM"));
    EXPECT_EQ(device.type(), QString("MRDIMM"));
    EXPECT_EQ(device.technology(), QString("NVDIMM-N"));
    EXPECT_EQ(device.manufacturer(), QString("Micron"));
    EXPECT_EQ(device.serialNumber(), QString("SN123"));
    EXPECT_EQ(device.assetTag(), QString("AT1"));
    EXPECT_EQ(device.partNumber(), QString("PN-XYZ"));
    EXPECT_EQ(device.firmwareVersion(), QString("FW-1.2"));
    EXPECT_EQ(device.speed(), 12800u);
    EXPECT_EQ(device.configuredSpeed(), 12000u);
    EXPECT_EQ(device.rank(), 2);
    EXPECT_EQ(device.minVoltage(), 1100u);
    EXPECT_EQ(device.maxVoltage(), 1100u);
    EXPECT_EQ(device.configuredVoltage(), 1100u);
    EXPECT_EQ(device.nonVolatileSize(), 128ULL * 1024ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(device.volatileSize(), 16ULL * 1024ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(device.cacheSize(), 0ULL); // All ones mean the size is unknown.
    EXPECT_EQ(device.logicalSize(), 0ULL);

    // Bit 13 of the type detail stands for a registered module.
    const QStringList detail = device.typeDetail();
    ASSERT_EQ(detail.size(), 1);
    EXPECT_EQ(detail[0], QString("Registered (Buffered)"));
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, MemoryDeviceSpeedWithoutExtendedFields)
{
    // An SMBIOS 2.8 table (length 28h) whose speeds overflowed the 16-bit fields, but which is
    // too short to carry the extended ones.
    QByteArray formatted(0x28 - 4, '\0');
    formatted[0x15 - 4] = static_cast<char>(0xFF); // speed
    formatted[0x16 - 4] = static_cast<char>(0xFF);
    formatted[0x20 - 4] = static_cast<char>(0xFF); // configured_speed
    formatted[0x21 - 4] = static_cast<char>(0xFF);

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_MEMORY_DEVICE, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosMemoryDevice device(enumerator.table());
    ASSERT_TRUE(device.isValid());
    EXPECT_EQ(device.speed(), 0u);
    EXPECT_EQ(device.configuredSpeed(), 0u);
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

    // Extended size: the field counts megabytes, so 32768 of them make 32 GB.
    EXPECT_EQ(sizeOf(makeDump(makeDevice(0x7FFF, 32768, 0x20))),
              32ULL * 1024ULL * 1024ULL * 1024ULL);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, MemoryArrayAddressFields)
{
    QByteArray formatted(0x0F - 4, '\0');
    formatted[0x08 - 4] = static_cast<char>(0xFF); // end_address: the 1048575th kilobyte
    formatted[0x09 - 4] = static_cast<char>(0xFF);
    formatted[0x0A - 4] = static_cast<char>(0x0F);
    formatted[0x0D - 4] = static_cast<char>(0x10); // array_handle
    formatted[0x0E - 4] = 2;                       // partition_width

    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_MEMORY_ARRAY_ADDRESS, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosMemoryArrayAddress address(enumerator.table());
    ASSERT_TRUE(address.isValid());
    EXPECT_EQ(address.startAddress(), 0ULL);
    EXPECT_EQ(address.endAddress(), 1024ULL * 1024ULL * 1024ULL - 1ULL);
    EXPECT_EQ(address.size(), 1024ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(address.arrayHandle(), 0x1000);
    EXPECT_EQ(address.partitionWidth(), 2);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, MemoryArrayAddressExtended)
{
    auto makeAddress = [](quint8 length) -> QByteArray
    {
        QByteArray formatted(length - 4, '\0');

        // start_address: the address does not fit the field.
        for (int i = 0; i < 4; ++i)
            formatted[0x04 - 4 + i] = static_cast<char>(0xFF);

        formatted[0x0E - 4] = static_cast<char>(0xFF); // partition_width: unknown

        if (length >= 0x1F)
        {
            formatted[0x13 - 4] = static_cast<char>(0x01); // ext_start_address: 4 GB

            for (int i = 0; i < 4; ++i)
                formatted[0x17 - 4 + i] = static_cast<char>(0xFF); // ext_end_address: 8 GB - 1
            formatted[0x1B - 4] = static_cast<char>(0x01);
        }

        return makeTable(SMBIOS_TABLE_TYPE_MEMORY_ARRAY_ADDRESS, formatted, {});
    };

    const QByteArray extended = makeDump(makeAddress(0x1F));

    SmbiosTableEnumerator extended_enumerator(extended);
    ASSERT_FALSE(extended_enumerator.isAtEnd());

    SmbiosMemoryArrayAddress extended_address(extended_enumerator.table());
    EXPECT_EQ(extended_address.startAddress(), 4ULL * 1024ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(extended_address.endAddress(), 8ULL * 1024ULL * 1024ULL * 1024ULL - 1ULL);
    EXPECT_EQ(extended_address.size(), 4ULL * 1024ULL * 1024ULL * 1024ULL);
    EXPECT_EQ(extended_address.partitionWidth(), 0);

    // An SMBIOS 2.1 table cannot tell the addresses the 32-bit fields overflowed on.
    const QByteArray legacy = makeDump(makeAddress(0x0F));

    SmbiosTableEnumerator legacy_enumerator(legacy);
    ASSERT_FALSE(legacy_enumerator.isAtEnd());

    SmbiosMemoryArrayAddress legacy_address(legacy_enumerator.table());
    EXPECT_EQ(legacy_address.startAddress(), 0ULL);
    EXPECT_EQ(legacy_address.endAddress(), 0ULL);
    EXPECT_EQ(legacy_address.size(), 0ULL);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedMemoryArrayAddressIsInvalid)
{
    const QByteArray dump = makeDump(
        makeTable(SMBIOS_TABLE_TYPE_MEMORY_ARRAY_ADDRESS, QByteArray(0x0E - 4, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosMemoryArrayAddress(enumerator.table()).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, PointingDeviceFields)
{
    QByteArray formatted(0x07 - 4, '\0');
    formatted[0x04 - 4] = 3; // type: mouse
    formatted[0x05 - 4] = 4; // interface_type: PS/2
    formatted[0x06 - 4] = 3; // button_count

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_POINTING_DEVICE, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosPointingDevice device(enumerator.table());
    ASSERT_TRUE(device.isValid());
    EXPECT_EQ(device.type(), QString("Mouse"));
    EXPECT_EQ(device.interfaceType(), QString("PS/2"));
    EXPECT_EQ(device.buttonCount(), 3);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, PointingDeviceBusInterfaceRange)
{
    // The interfaces above A0h form a separate range of values.
    QByteArray formatted(0x07 - 4, '\0');
    formatted[0x04 - 4] = 7;                       // type: touch pad
    formatted[0x05 - 4] = static_cast<char>(0xA2); // interface_type: USB
    formatted[0x06 - 4] = 2;                       // button_count

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_POINTING_DEVICE, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosPointingDevice device(enumerator.table());
    EXPECT_EQ(device.type(), QString("Touch Pad"));
    EXPECT_EQ(device.interfaceType(), QString("USB"));
    EXPECT_EQ(device.buttonCount(), 2);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedPointingDeviceIsInvalid)
{
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_POINTING_DEVICE, QByteArray(0x06 - 4, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosPointingDevice(enumerator.table()).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, PortableBatteryFields)
{
    QByteArray formatted(0x1A - 4, '\0');
    formatted[0x04 - 4] = 1;                       // location: string #1
    formatted[0x05 - 4] = 2;                       // manufacturer: string #2
    formatted[0x06 - 4] = 3;                       // manufacture_date: string #3
    formatted[0x07 - 4] = 4;                       // serial_number: string #4
    formatted[0x08 - 4] = 5;                       // device_name: string #5
    formatted[0x09 - 4] = 6;                       // device_chemistry: lithium-ion
    formatted[0x0A - 4] = static_cast<char>(0xA0); // design_capacity: 4000 units
    formatted[0x0B - 4] = static_cast<char>(0x0F);
    formatted[0x0C - 4] = static_cast<char>(0x88); // design_voltage: 11400 mV
    formatted[0x0D - 4] = static_cast<char>(0x2C);
    formatted[0x0E - 4] = 6;                       // sbds_version: string #6
    formatted[0x0F - 4] = 3;                       // max_error: 3 percent
    formatted[0x15 - 4] = 10;                      // capacity_multiplier

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_PORTABLE_BATTERY, formatted,
        { "Rear", "Sony", "03/15/2020", "SN-1234", "DELL ABC123", "1.0" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosPortableBattery battery(enumerator.table());
    ASSERT_TRUE(battery.isValid());
    EXPECT_EQ(battery.location(), QString("Rear"));
    EXPECT_EQ(battery.manufacturer(), QString("Sony"));
    EXPECT_EQ(battery.manufactureDate(), QString("03/15/2020"));
    EXPECT_EQ(battery.serialNumber(), QString("SN-1234"));
    EXPECT_EQ(battery.deviceName(), QString("DELL ABC123"));
    EXPECT_EQ(battery.chemistry(), QString("Lithium-ion"));
    EXPECT_EQ(battery.sbdsVersion(), QString("1.0"));
    EXPECT_EQ(battery.designCapacity(), 40000u); // Scaled by the multiplier.
    EXPECT_EQ(battery.designVoltage(), 11400u);
    EXPECT_EQ(battery.maxError(), 3);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, PortableBatterySbdsFields)
{
    // A battery following the Smart Battery Data Specification: the date, the serial number and
    // the chemistry come from the SBDS fields instead of the usual ones.
    QByteArray formatted(0x1A - 4, '\0');
    formatted[0x09 - 4] = 2;                       // device_chemistry: unknown
    formatted[0x10 - 4] = static_cast<char>(0x2B); // sbds_serial_number: 1A2B
    formatted[0x11 - 4] = static_cast<char>(0x1A);
    formatted[0x12 - 4] = static_cast<char>(0xCF); // sbds_manufacture_date: 2021-06-15
    formatted[0x13 - 4] = static_cast<char>(0x52);
    formatted[0x14 - 4] = 1;                       // sbds_device_chemistry: string #1

    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_PORTABLE_BATTERY, formatted, { "LION" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosPortableBattery battery(enumerator.table());
    ASSERT_TRUE(battery.isValid());
    EXPECT_EQ(battery.manufactureDate(), QString("2021-06-15"));
    EXPECT_EQ(battery.serialNumber(), QString("1A2B"));
    EXPECT_EQ(battery.chemistry(), QString("LION"));
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, PortableBatteryShortTableOmitsLateFields)
{
    // An SMBIOS 2.1 battery table (length 10h) carries none of the SBDS fields.
    QByteArray formatted(0x10 - 4, '\0');
    formatted[0x09 - 4] = 2;                       // device_chemistry: unknown
    formatted[0x0A - 4] = static_cast<char>(0xA0); // design_capacity: 4000 mWh
    formatted[0x0B - 4] = static_cast<char>(0x0F);
    formatted[0x0F - 4] = static_cast<char>(0xFF); // max_error: unknown

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_PORTABLE_BATTERY, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosPortableBattery battery(enumerator.table());
    ASSERT_TRUE(battery.isValid());
    EXPECT_TRUE(battery.manufactureDate().isEmpty());
    EXPECT_TRUE(battery.serialNumber().isEmpty());
    EXPECT_EQ(battery.chemistry(), QString("Unknown"));
    EXPECT_EQ(battery.designCapacity(), 4000u); // No multiplier to scale by.
    EXPECT_EQ(battery.maxError(), -1);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedPortableBatteryIsInvalid)
{
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_PORTABLE_BATTERY, QByteArray(0x0F - 4, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosPortableBattery(enumerator.table()).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, PowerSupplyFields)
{
    QByteArray formatted(0x16 - 4, '\0');
    formatted[0x04 - 4] = 1;                       // unit_group
    formatted[0x05 - 4] = 1;                       // location: string #1
    formatted[0x06 - 4] = 2;                       // device_name: string #2
    formatted[0x07 - 4] = 3;                       // manufacturer: string #3
    formatted[0x08 - 4] = 4;                       // serial_number: string #4
    formatted[0x09 - 4] = 5;                       // asset_tag: string #5
    formatted[0x0A - 4] = 6;                       // model_part_number: string #6
    formatted[0x0B - 4] = 7;                       // revision_level: string #7
    formatted[0x0C - 4] = static_cast<char>(0xE8); // max_power_capacity: 65000 mW
    formatted[0x0D - 4] = static_cast<char>(0xFD);
    formatted[0x0E - 4] = static_cast<char>(0xA3); // characteristics: switching, OK, auto-switch,
    formatted[0x0F - 4] = static_cast<char>(0x11); // present, hot-replaceable

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_POWER_SUPPLY, formatted,
        { "Internal", "PSU1", "Delta", "SN-77", "AT-88", "DPS-500", "1.1" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosPowerSupply supply(enumerator.table());
    ASSERT_TRUE(supply.isValid());
    EXPECT_TRUE(supply.isPresent());
    EXPECT_FALSE(supply.isUnplugged());
    EXPECT_TRUE(supply.isHotReplaceable());
    EXPECT_EQ(supply.unitGroup(), 1);
    EXPECT_EQ(supply.location(), QString("Internal"));
    EXPECT_EQ(supply.deviceName(), QString("PSU1"));
    EXPECT_EQ(supply.manufacturer(), QString("Delta"));
    EXPECT_EQ(supply.serialNumber(), QString("SN-77"));
    EXPECT_EQ(supply.assetTag(), QString("AT-88"));
    EXPECT_EQ(supply.modelPartNumber(), QString("DPS-500"));
    EXPECT_EQ(supply.revisionLevel(), QString("1.1"));
    EXPECT_EQ(supply.type(), QString("Switching"));
    EXPECT_EQ(supply.status(), QString("OK"));
    EXPECT_EQ(supply.inputVoltageRangeSwitching(), QString("Auto-switch"));
    EXPECT_EQ(supply.maxPowerCapacity(), 65000u);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, PowerSupplyUnknownFields)
{
    // A supply with the capacity reported as unknown and an empty characteristics word.
    QByteArray formatted(0x16 - 4, '\0');
    formatted[0x0D - 4] = static_cast<char>(0x80); // max_power_capacity: unknown

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_POWER_SUPPLY, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosPowerSupply supply(enumerator.table());
    ASSERT_TRUE(supply.isValid());
    EXPECT_EQ(supply.maxPowerCapacity(), 0u);
    EXPECT_TRUE(supply.type().isEmpty());
    EXPECT_TRUE(supply.status().isEmpty());
    EXPECT_TRUE(supply.inputVoltageRangeSwitching().isEmpty());
    EXPECT_FALSE(supply.isPresent());
    EXPECT_FALSE(supply.isUnplugged());
    EXPECT_FALSE(supply.isHotReplaceable());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedPowerSupplyIsInvalid)
{
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_POWER_SUPPLY, QByteArray(0x15 - 4, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosPowerSupply(enumerator.table()).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, OnBoardDeviceExtFields)
{
    QByteArray formatted(0x0B - 4, '\0');
    formatted[0x04 - 4] = 1;                       // designation: string #1
    formatted[0x05 - 4] = static_cast<char>(0x85); // device_type: ethernet, enabled
    formatted[0x06 - 4] = 1;                       // type_instance
    formatted[0x09 - 4] = 2;                       // bus_number
    formatted[0x0A - 4] = static_cast<char>(0x10); // device_function: device 2, function 0

    const QByteArray dump = makeDump(
        makeTable(SMBIOS_TABLE_TYPE_ONBOARD_DEVICE_EXT, formatted, { "Onboard LAN" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosOnBoardDeviceExt device(enumerator.table());
    ASSERT_TRUE(device.isValid());
    EXPECT_TRUE(device.isEnabled());
    EXPECT_EQ(device.designation(), QString("Onboard LAN"));
    EXPECT_EQ(device.type(), QString("Ethernet"));
    EXPECT_EQ(device.typeInstance(), 1);
    EXPECT_TRUE(device.hasBusAddress());
    EXPECT_EQ(device.segmentGroupNumber(), 0);
    EXPECT_EQ(device.busNumber(), 2);
    EXPECT_EQ(device.deviceNumber(), 2);
    EXPECT_EQ(device.functionNumber(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, OnBoardDeviceExtWithoutBusAddress)
{
    // A disabled device outside a PCI bus: the address fields are all ones.
    QByteArray formatted(0x0B - 4, '\0');
    formatted[0x05 - 4] = static_cast<char>(0x0B); // device_type: wireless LAN, disabled
    formatted[0x07 - 4] = static_cast<char>(0xFF); // segment_group
    formatted[0x08 - 4] = static_cast<char>(0xFF);
    formatted[0x09 - 4] = static_cast<char>(0xFF); // bus_number
    formatted[0x0A - 4] = static_cast<char>(0xFF); // device_function

    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_ONBOARD_DEVICE_EXT, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosOnBoardDeviceExt device(enumerator.table());
    ASSERT_TRUE(device.isValid());
    EXPECT_FALSE(device.isEnabled());
    EXPECT_EQ(device.type(), QString("Wireless LAN"));
    EXPECT_FALSE(device.hasBusAddress());
    EXPECT_EQ(device.busNumber(), 0);
    EXPECT_EQ(device.deviceNumber(), 0);
    EXPECT_EQ(device.functionNumber(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedOnBoardDeviceExtIsInvalid)
{
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_ONBOARD_DEVICE_EXT, QByteArray(0x0A - 4, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosOnBoardDeviceExt(enumerator.table()).isValid());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TpmDeviceFields)
{
    QByteArray formatted(0x1F - 4, '\0');
    formatted[0x04 - 4] = 'M';                     // vendor_id
    formatted[0x05 - 4] = 'S';
    formatted[0x06 - 4] = 'F';
    formatted[0x07 - 4] = 'T';
    formatted[0x08 - 4] = 2;                       // major_version
    formatted[0x09 - 4] = 0;                       // minor_version
    formatted[0x0A - 4] = 2;                       // firmware_version1: 7.2 packed in halves
    formatted[0x0C - 4] = 7;
    formatted[0x12 - 4] = 1;                       // description: string #1
    formatted[0x13 - 4] = static_cast<char>(0x18); // characteristics: firmware and software

    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_TPM_DEVICE, formatted, { "INTC TPM 2.0" }));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosTpmDevice tpm(enumerator.table());
    ASSERT_TRUE(tpm.isValid());
    EXPECT_EQ(tpm.vendorId(), QString("MSFT"));
    EXPECT_EQ(tpm.specVersion(), QString("2.0"));
    EXPECT_EQ(tpm.firmwareVersion(), QString("7.2"));
    EXPECT_EQ(tpm.description(), QString("INTC TPM 2.0"));
    EXPECT_TRUE(tpm.isFamilyConfigurableByFirmware());
    EXPECT_TRUE(tpm.isFamilyConfigurableBySoftware());
    EXPECT_FALSE(tpm.isFamilyConfigurableByOem());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TpmDeviceLegacySpecVersion)
{
    // A TPM 1.2 device packs the firmware version differently and pads the vendor id with zeros.
    QByteArray formatted(0x1F - 4, '\0');
    formatted[0x04 - 4] = 'I'; // vendor_id
    formatted[0x05 - 4] = 'F';
    formatted[0x06 - 4] = 'X';
    formatted[0x08 - 4] = 1;   // major_version
    formatted[0x09 - 4] = 2;   // minor_version
    formatted[0x0B - 4] = 3;   // firmware_version1: 3.19 packed in bytes
    formatted[0x0C - 4] = 19;

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_TPM_DEVICE, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosTpmDevice tpm(enumerator.table());
    EXPECT_EQ(tpm.vendorId(), QString("IFX"));
    EXPECT_EQ(tpm.specVersion(), QString("1.2"));
    EXPECT_EQ(tpm.firmwareVersion(), QString("3.19"));
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TpmDeviceUnsupportedCharacteristics)
{
    // Bit 2 marks the characteristics as not supported, and a numeric vendor id has nothing to
    // show as text.
    QByteArray formatted(0x1F - 4, '\0');
    formatted[0x04 - 4] = 1;                       // vendor_id
    formatted[0x05 - 4] = 2;
    formatted[0x06 - 4] = 3;
    formatted[0x07 - 4] = 4;
    formatted[0x13 - 4] = static_cast<char>(0x0C); // characteristics: not supported, firmware

    const QByteArray dump = makeDump(makeTable(SMBIOS_TABLE_TYPE_TPM_DEVICE, formatted, {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    SmbiosTpmDevice tpm(enumerator.table());
    EXPECT_TRUE(tpm.vendorId().isEmpty());
    EXPECT_TRUE(tpm.specVersion().isEmpty());
    EXPECT_TRUE(tpm.firmwareVersion().isEmpty());
    EXPECT_FALSE(tpm.isFamilyConfigurableByFirmware());
    EXPECT_FALSE(tpm.isFamilyConfigurableBySoftware());
    EXPECT_FALSE(tpm.isFamilyConfigurableByOem());
}

//--------------------------------------------------------------------------------------------------
TEST(SmbiosParserTest, TruncatedTpmDeviceIsInvalid)
{
    const QByteArray dump =
        makeDump(makeTable(SMBIOS_TABLE_TYPE_TPM_DEVICE, QByteArray(0x1E - 4, '\0'), {}));

    SmbiosTableEnumerator enumerator(dump);
    ASSERT_FALSE(enumerator.isAtEnd());

    EXPECT_FALSE(SmbiosTpmDevice(enumerator.table()).isValid());
}
