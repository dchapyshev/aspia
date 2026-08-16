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

#include "base/ini_file.h"

#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace {

class IniFileTest : public testing::Test
{
public:
    QString dirPath() const { return temp_dir_.path(); }
    QString filePath() const { return temp_dir_.path() + "/test.ini"; }

    void writeFile(const QByteArray& content)
    {
        QFile file(filePath());
        ASSERT_TRUE(file.open(QFile::WriteOnly));
        ASSERT_EQ(file.write(content), content.size());
    }

    // The line endings of the written file are those of the platform, so they are translated back
    // for comparison with what the test expects.
    QByteArray readFile() const
    {
        QFile file(filePath());
        if (!file.open(QFile::ReadOnly | QFile::Text))
            return QByteArray();
        return file.readAll();
    }

private:
    QTemporaryDir temp_dir_;
};

} // namespace

// ============================================================================
// reading
// ============================================================================

TEST_F(IniFileTest, missing_file_is_empty_and_not_an_error)
{
    IniFile ini(filePath());

    EXPECT_TRUE(ini.isEmpty());
    EXPECT_FALSE(ini.hasErrors());
    EXPECT_TRUE(ini.stringValue("Server", "port").isEmpty());
}

TEST_F(IniFileTest, unreadable_file_is_an_error)
{
    // A directory opens as a file on no platform.
    IniFile ini(dirPath());

    EXPECT_TRUE(ini.hasErrors());
    EXPECT_TRUE(ini.isEmpty());
}

TEST_F(IniFileTest, sections_and_keys)
{
    writeFile("[Server]\nport=8060\nhost=127.0.0.1\n\n[Keys]\nprivate_key=aabb\n");

    IniFile ini(filePath());

    EXPECT_FALSE(ini.isEmpty());
    EXPECT_FALSE(ini.hasErrors());
    EXPECT_EQ(ini.filePath(), filePath());
    EXPECT_EQ(ini.stringValue("Server", "port"), "8060");
    EXPECT_EQ(ini.stringValue("Server", "host"), "127.0.0.1");
    EXPECT_EQ(ini.stringValue("Keys", "private_key"), "aabb");
}

TEST_F(IniFileTest, unknown_section_or_key_is_empty)
{
    writeFile("[Server]\nport=8060\n");

    IniFile ini(filePath());

    EXPECT_TRUE(ini.stringValue("Server", "host").isEmpty());
    EXPECT_TRUE(ini.stringValue("Peer", "port").isEmpty());
}

TEST_F(IniFileTest, spaces_are_trimmed)
{
    writeFile("  [ Server ]  \n\t port \t = \t 8060 \t\n");

    IniFile ini(filePath());

    EXPECT_EQ(ini.stringValue("Server", "port"), "8060");
}

TEST_F(IniFileTest, comments_and_blank_lines_are_ignored)
{
    writeFile("; comment\n# another comment\n\n[Server]\n\n; port=1\nport=8060\n");

    IniFile ini(filePath());

    EXPECT_FALSE(ini.hasErrors());
    EXPECT_EQ(ini.stringValue("Server", "port"), "8060");
}

TEST_F(IniFileTest, windows_line_endings_are_handled)
{
    writeFile("[Server]\r\nport=8060\r\n");

    IniFile ini(filePath());

    EXPECT_FALSE(ini.hasErrors());
    EXPECT_EQ(ini.stringValue("Server", "port"), "8060");
}

TEST_F(IniFileTest, byte_order_mark_is_skipped)
{
    writeFile("\xEF\xBB\xBF[Server]\nport=8060\n");

    IniFile ini(filePath());

    EXPECT_FALSE(ini.hasErrors());
    EXPECT_EQ(ini.stringValue("Server", "port"), "8060");
}

TEST_F(IniFileTest, byte_order_mark_before_spaces_is_skipped)
{
    // An editor may write the byte order mark and its user may indent the first line.
    writeFile("\xEF\xBB\xBF  [Server]\nport=8060\n");

    IniFile ini(filePath());

    EXPECT_FALSE(ini.hasErrors());
    EXPECT_EQ(ini.stringValue("Server", "port"), "8060");
}

TEST_F(IniFileTest, text_values_are_utf8)
{
    writeFile("[Server]\ncomment=\xC3\xA4\xC3\xB6\n");

    IniFile ini(filePath());

    EXPECT_EQ(ini.stringValue("Server", "comment"), QString::fromUtf8("\xC3\xA4\xC3\xB6"));
}

TEST_F(IniFileTest, value_that_is_not_utf8_survives_a_rewrite)
{
    // The bytes of a value the caller never asks for are written back as they were read.
    writeFile("[Server]\nother=\xE4\xF6\n");

    IniFile ini(filePath());
    ini.setStringValue("Server", "port", "8060");

    ASSERT_TRUE(ini.sync());
    EXPECT_EQ(readFile(), "[Server]\nother=\xE4\xF6\nport=8060\n\n");
}

TEST_F(IniFileTest, keys_before_first_section)
{
    writeFile("port=8060\n\n[Server]\nhost=127.0.0.1\n");

    IniFile ini(filePath());

    EXPECT_FALSE(ini.hasErrors());
    EXPECT_EQ(ini.stringValue(QByteArray(), "port"), "8060");
    EXPECT_EQ(ini.stringValue("Server", "host"), "127.0.0.1");
}

TEST_F(IniFileTest, value_may_contain_assignment)
{
    writeFile("[Service]\nEnvironment=ASPIA_LOG_LEVEL=2\n");

    IniFile ini(filePath());

    EXPECT_EQ(ini.stringValue("Service", "Environment"), "ASPIA_LOG_LEVEL=2");
}

TEST_F(IniFileTest, empty_value_is_kept)
{
    writeFile("[Server]\nhost=\n");

    IniFile ini(filePath());

    EXPECT_FALSE(ini.hasErrors());
    EXPECT_FALSE(ini.isEmpty());
    EXPECT_TRUE(ini.stringValue("Server", "host").isEmpty());
}

TEST_F(IniFileTest, empty_value_is_not_the_default)
{
    // A key that is present with an empty value is not a missing one.
    writeFile("[Server]\nhost=\n");

    IniFile ini(filePath());

    EXPECT_EQ(ini.stringValue("Server", "host", "fallback"), "");
    EXPECT_TRUE(ini.binaryValue("Server", "host", QByteArray("x")).isEmpty());
    EXPECT_EQ(ini.stringValue("Server", "missing", "fallback"), "fallback");

    // An empty value is no number and no boolean.
    EXPECT_EQ(ini.uint64Value("Server", "host", 8060), 8060u);
    EXPECT_TRUE(ini.booleanValue("Server", "host", true));
}

TEST_F(IniFileTest, line_without_assignment_is_an_error)
{
    writeFile("[Server]\nport\n");

    IniFile ini(filePath());

    EXPECT_TRUE(ini.hasErrors());
}

TEST_F(IniFileTest, line_without_key_is_an_error)
{
    writeFile("[Server]\n=8060\n");

    IniFile ini(filePath());

    EXPECT_TRUE(ini.hasErrors());
}

TEST_F(IniFileTest, unterminated_section_is_an_error)
{
    writeFile("[Server\nport=8060\n");

    IniFile ini(filePath());

    EXPECT_TRUE(ini.hasErrors());
}

TEST_F(IniFileTest, keys_after_an_invalid_header_are_not_attributed)
{
    // The keys after the broken header have no section they could belong to, so nothing of them
    // may leak into the section before it, whatever becomes of a damaged file as a whole.
    writeFile("[Server]\nport=8060\n[Peer\nport=8070\nhost=127.0.0.1\n");

    IniFile ini(filePath());

    EXPECT_TRUE(ini.hasErrors());
    EXPECT_TRUE(ini.isEmpty());
    EXPECT_TRUE(ini.stringValue("Server", "host").isEmpty());
}

TEST_F(IniFileTest, empty_section_name_is_an_error)
{
    writeFile("[]\nport=8060\n");

    IniFile ini(filePath());

    EXPECT_TRUE(ini.hasErrors());
}

TEST_F(IniFileTest, repeated_key_is_an_error)
{
    writeFile("[Server]\nport=8060\nport=8061\n");

    IniFile ini(filePath());

    EXPECT_TRUE(ini.hasErrors());
}

TEST_F(IniFileTest, line_that_is_too_long_is_an_error)
{
    // The parser reads a line as a whole into memory, so a runaway line must not be one.
    writeFile("[Server]\nport=8060\nlong=" + QByteArray(64 * 1024, 'a') + "\n");

    IniFile ini(filePath());

    EXPECT_TRUE(ini.hasErrors());
    EXPECT_TRUE(ini.isEmpty());
}

TEST_F(IniFileTest, long_line_within_the_limit_is_read)
{
    const QByteArray value(1024, 'a');
    writeFile("[Server]\nlong=" + value + "\n");

    IniFile ini(filePath());

    EXPECT_FALSE(ini.hasErrors());
    EXPECT_EQ(ini.stringValue("Server", "long"), QString::fromUtf8(value));
}

TEST_F(IniFileTest, file_that_is_too_big_is_an_error)
{
    // Every line is valid, so only the size of the file as a whole may reject it.
    QByteArray content("[Server]\n");
    for (int i = 0; content.size() <= 1024 * 1024; ++i)
        content += "key" + QByteArray::number(i) + "=value\n";
    writeFile(content);

    IniFile ini(filePath());

    EXPECT_TRUE(ini.hasErrors());
    EXPECT_TRUE(ini.isEmpty());
}

TEST_F(IniFileTest, damaged_file_keeps_no_values)
{
    writeFile("[Server]\nport=8060\nbroken\n");

    IniFile ini(filePath());

    EXPECT_TRUE(ini.hasErrors());
    EXPECT_TRUE(ini.isEmpty());
    EXPECT_TRUE(ini.stringValue("Server", "port").isEmpty());
}

TEST_F(IniFileTest, same_key_in_other_section_is_not_a_repeat)
{
    writeFile("[Server]\nport=8060\n\n[Peer]\nport=8070\n");

    IniFile ini(filePath());

    EXPECT_FALSE(ini.hasErrors());
    EXPECT_EQ(ini.stringValue("Server", "port"), "8060");
    EXPECT_EQ(ini.stringValue("Peer", "port"), "8070");
}

TEST_F(IniFileTest, section_continues_after_repeated_header)
{
    writeFile("[Server]\nport=8060\n\n[Server]\nhost=127.0.0.1\n");

    IniFile ini(filePath());

    EXPECT_FALSE(ini.hasErrors());
    EXPECT_EQ(ini.stringValue("Server", "port"), "8060");
    EXPECT_EQ(ini.stringValue("Server", "host"), "127.0.0.1");
}

// ============================================================================
// numbers and booleans
// ============================================================================

TEST_F(IniFileTest, numbers)
{
    writeFile("[Server]\nport=8060\ncount=100\ntimeout=-5\n");

    IniFile ini(filePath());

    EXPECT_EQ(ini.uint64Value("Server", "port", 1), 8060u);
    EXPECT_EQ(ini.uint64Value("Server", "count", 1), 100u);
    EXPECT_EQ(ini.int64Value("Server", "timeout", 1), -5);
}

TEST_F(IniFileTest, missing_number_is_the_default)
{
    IniFile ini(filePath());

    EXPECT_EQ(ini.uint64Value("Server", "port", 8060), 8060u);
    EXPECT_EQ(ini.int64Value("Server", "timeout", -5), -5);
}

TEST_F(IniFileTest, number_that_does_not_fit_is_the_default)
{
    // A word, a negative number for an unsigned value and a number of twenty digits.
    writeFile("[Server]\nfirst=abc\nsecond=-1\nthird=99999999999999999999\n");

    IniFile ini(filePath());

    EXPECT_EQ(ini.uint64Value("Server", "first", 5), 5u);
    EXPECT_EQ(ini.int64Value("Server", "first", 5), 5);
    EXPECT_EQ(ini.uint64Value("Server", "second", 5), 5u);
    EXPECT_EQ(ini.int64Value("Server", "second", 5), -1);
    EXPECT_EQ(ini.uint64Value("Server", "third", 5), 5u);
    EXPECT_EQ(ini.int64Value("Server", "third", 5), 5);
}

TEST_F(IniFileTest, numbers_of_32_bits)
{
    // 3000000000 fits an unsigned value of 32 bits and not a signed one.
    writeFile("[Server]\ncount=100\ntimeout=-5\nbig=3000000000\nnegative_big=-3000000000\n");

    IniFile ini(filePath());

    EXPECT_EQ(ini.uint32Value("Server", "count", 1), 100u);
    EXPECT_EQ(ini.int32Value("Server", "timeout", 1), -5);
    EXPECT_EQ(ini.uint32Value("Server", "big", 5), 3000000000u);
    EXPECT_EQ(ini.int32Value("Server", "big", 5), 5);
    EXPECT_EQ(ini.uint32Value("Server", "negative_big", 5), 5u);
    EXPECT_EQ(ini.int32Value("Server", "negative_big", 5), 5);
}

TEST_F(IniFileTest, numbers_of_16_bits)
{
    // 40000 fits an unsigned value of 16 bits and not a signed one, 99999 fits neither.
    writeFile("[Server]\nport=8060\ntimeout=-5\nbig=40000\nnegative_big=-40000\nhuge=99999\n");

    IniFile ini(filePath());

    EXPECT_EQ(ini.uint16Value("Server", "port", 1), 8060);
    EXPECT_EQ(ini.int16Value("Server", "timeout", 1), -5);
    EXPECT_EQ(ini.uint16Value("Server", "big", 5), 40000);
    EXPECT_EQ(ini.int16Value("Server", "big", 5), 5);
    EXPECT_EQ(ini.uint16Value("Server", "negative_big", 5), 5);
    EXPECT_EQ(ini.int16Value("Server", "negative_big", 5), 5);
    EXPECT_EQ(ini.uint16Value("Server", "huge", 5), 5);
}

TEST_F(IniFileTest, booleans)
{
    writeFile("[Server]\nstun=true\nstats=false\nold_style=1\nold_style_off=0\nbroken=maybe\n");

    IniFile ini(filePath());

    EXPECT_TRUE(ini.booleanValue("Server", "stun", false));
    EXPECT_FALSE(ini.booleanValue("Server", "stats", true));
    EXPECT_TRUE(ini.booleanValue("Server", "old_style", false));
    EXPECT_FALSE(ini.booleanValue("Server", "old_style_off", true));
    EXPECT_TRUE(ini.booleanValue("Server", "broken", true));
    EXPECT_TRUE(ini.booleanValue("Server", "missing", true));
}

TEST_F(IniFileTest, the_type_of_the_value_says_how_it_is_written)
{
    IniFile ini(filePath());
    ini.setStringValue("Server", "text", QString::fromUtf8("\xC3\xA4"));
    ini.setStringValue("Server", "literal", "just text");
    ini.setBinaryValue("Keys", "binary", QByteArray("\x01\x02", 2));

    ASSERT_TRUE(ini.sync());
    EXPECT_EQ(readFile(), "[Keys]\nbinary=0102\n\n[Server]\nliteral=just text\ntext=\xC3\xA4\n\n");
}

TEST_F(IniFileTest, string_of_utf8_bytes_is_written_as_it_is)
{
    IniFile ini(filePath());
    ini.setStringValue("Server", "comment", QByteArray("\xC3\xA4\xC3\xB6"));

    EXPECT_EQ(ini.stringValue("Server", "comment"), QString::fromUtf8("\xC3\xA4\xC3\xB6"));

    ASSERT_TRUE(ini.sync());
    EXPECT_EQ(readFile(), "[Server]\ncomment=\xC3\xA4\xC3\xB6\n\n");
}

TEST_F(IniFileTest, numbers_and_booleans_are_written_as_text)
{
    IniFile ini(filePath());
    ini.setUInt64Value("Server", "port", 8060);
    ini.setInt64Value("Server", "timeout", -5);
    ini.setUInt32Value("Server", "count", 100);
    ini.setInt32Value("Server", "offset", -3);
    ini.setUInt16Value("Server", "slot", 9);
    ini.setInt16Value("Server", "delta", -7);
    ini.setBooleanValue("Server", "stun", true);
    ini.setBooleanValue("Server", "stats", false);

    ASSERT_TRUE(ini.sync());
    EXPECT_EQ(readFile(), "[Server]\ncount=100\ndelta=-7\noffset=-3\nport=8060\nslot=9\n"
                          "stats=0\nstun=1\ntimeout=-5\n\n");
}

// ============================================================================
// binary values
// ============================================================================

TEST_F(IniFileTest, hex_values)
{
    writeFile("[Keys]\nprivate_key=00ff10\n");

    IniFile ini(filePath());

    EXPECT_EQ(ini.binaryValue("Keys", "private_key"), QByteArray("\x00\xFF\x10", 3));
}

TEST_F(IniFileTest, missing_hex_value_is_empty)
{
    IniFile ini(filePath());

    EXPECT_TRUE(ini.binaryValue("Keys", "private_key").isEmpty());
    EXPECT_EQ(ini.binaryValue("Keys", "private_key", QByteArray("\x01", 1)), QByteArray("\x01", 1));
}

TEST_F(IniFileTest, damaged_hex_value_is_empty)
{
    // A value that is not hex at all, one with a character that is not, and one of odd length.
    writeFile("[Keys]\nfirst=zzzz\nsecond=00zz10\nthird=00f\n");

    IniFile ini(filePath());

    EXPECT_TRUE(ini.binaryValue("Keys", "first").isEmpty());
    EXPECT_TRUE(ini.binaryValue("Keys", "second").isEmpty());
    EXPECT_TRUE(ini.binaryValue("Keys", "third").isEmpty());
}

TEST_F(IniFileTest, hex_values_survive_a_write_and_a_read)
{
    const QByteArray key("\x00\x01\xFE\xFF", 4);

    IniFile ini(filePath());
    ini.setBinaryValue("Keys", "private_key", key);

    ASSERT_TRUE(ini.sync());
    EXPECT_EQ(readFile(), "[Keys]\nprivate_key=0001feff\n\n");

    IniFile restored(filePath());

    EXPECT_EQ(restored.binaryValue("Keys", "private_key"), key);
}

// ============================================================================
// sync
// ============================================================================

TEST_F(IniFileTest, sync_writes_sections)
{
    IniFile ini(filePath());
    ini.setStringValue("Server", "port", "8060");
    ini.setStringValue("Server", "host", "127.0.0.1");
    ini.setStringValue("Keys", "private_key", "aabb");

    ASSERT_TRUE(ini.sync());
    EXPECT_EQ(readFile(), "[Keys]\nprivate_key=aabb\n\n[Server]\nhost=127.0.0.1\nport=8060\n\n");
}

TEST_F(IniFileTest, sync_writes_keys_without_section_first)
{
    IniFile ini(filePath());
    ini.setStringValue("Server", "port", "8060");
    ini.setStringValue(QByteArray(), "host", "127.0.0.1");

    ASSERT_TRUE(ini.sync());
    EXPECT_EQ(readFile(), "host=127.0.0.1\n\n[Server]\nport=8060\n\n");
}

TEST_F(IniFileTest, sync_of_empty_settings_writes_nothing)
{
    IniFile ini(filePath());

    ASSERT_TRUE(ini.sync());
    EXPECT_TRUE(readFile().isEmpty());
}

TEST_F(IniFileTest, values_survive_a_write_and_a_read)
{
    IniFile original(filePath());
    original.setStringValue("Server", "port", "8060");
    original.setStringValue("Server", "comment", "\xC3\xA4\xC3\xB6");
    original.setStringValue("Service", "Environment", "ASPIA_LOG_LEVEL=2");

    ASSERT_TRUE(original.sync());

    IniFile restored(filePath());

    EXPECT_FALSE(restored.hasErrors());
    EXPECT_EQ(restored.stringValue("Server", "port"), "8060");
    EXPECT_EQ(restored.stringValue("Server", "comment"), "\xC3\xA4\xC3\xB6");
    EXPECT_EQ(restored.stringValue("Service", "Environment"), "ASPIA_LOG_LEVEL=2");
}

TEST_F(IniFileTest, sync_replaces_the_whole_file)
{
    writeFile("[Server]\nhost=127.0.0.1\nport=8060\n");

    IniFile ini(filePath());
    ini.removeValue("Server", "host");
    ini.setStringValue("Server", "port", "8061");

    ASSERT_TRUE(ini.sync());
    EXPECT_EQ(readFile(), "[Server]\nport=8061\n\n");
}

TEST_F(IniFileTest, sync_after_a_failed_read_does_not_touch_the_file)
{
    // The file may carry a single typo of its owner; writing the empty settings back would
    // replace it with an empty file instead of leaving it for the owner to repair.
    writeFile("[Server]\nport=8060\nbroken\n");

    IniFile ini(filePath());
    ASSERT_TRUE(ini.hasErrors());

    EXPECT_FALSE(ini.sync());
    EXPECT_EQ(readFile(), "[Server]\nport=8060\nbroken\n");
}

TEST_F(IniFileTest, sync_into_missing_directory_fails)
{
    IniFile ini(dirPath() + "/no_such_directory/test.ini");
    ini.setStringValue("Server", "port", "8060");

    EXPECT_FALSE(ini.sync());
}

// ============================================================================
// setters / removeValue
// ============================================================================

TEST_F(IniFileTest, set_value_replaces_existing)
{
    writeFile("[Server]\nport=8060\n");

    IniFile ini(filePath());
    ini.setStringValue("Server", "port", "8061");

    EXPECT_EQ(ini.stringValue("Server", "port"), "8061");
}

TEST_F(IniFileTest, value_with_a_line_break_is_not_stored)
{
    IniFile ini(filePath());
    ini.setStringValue("Server", "port", "8060");
    ini.setStringValue("Server", "motd", "first\nsecond");

    EXPECT_TRUE(ini.stringValue("Server", "motd").isEmpty());

    // The file must not gain an extra line that does not parse.
    ASSERT_TRUE(ini.sync());

    IniFile restored(filePath());

    EXPECT_FALSE(restored.hasErrors());
    EXPECT_EQ(restored.stringValue("Server", "port"), "8060");
}

TEST_F(IniFileTest, value_with_surrounding_spaces_is_not_stored)
{
    // The parser trims a value, so the spaces would not survive the way back through the file.
    IniFile ini(filePath());
    ini.setStringValue("Server", "host", " 127.0.0.1 ");

    EXPECT_TRUE(ini.stringValue("Server", "host").isEmpty());
}

TEST_F(IniFileTest, key_that_does_not_read_back_is_not_stored)
{
    // An empty key does not parse, one with an assignment splits at it, one that starts like a
    // section header does not parse, one that starts like a comment silently disappears, a line
    // break breaks the file and surrounding spaces are trimmed away.
    IniFile ini(filePath());
    ini.setStringValue("Server", QByteArray(), "1");
    ini.setStringValue("Server", "key=other", "1");
    ini.setStringValue("Server", "[key", "1");
    ini.setStringValue("Server", ";key", "1");
    ini.setStringValue("Server", "#key", "1");
    ini.setStringValue("Server", "key\nother", "1");
    ini.setStringValue("Server", " key ", "1");

    EXPECT_TRUE(ini.isEmpty());
}

TEST_F(IniFileTest, section_that_does_not_read_back_is_not_stored)
{
    IniFile ini(filePath());
    ini.setStringValue("Server\n[Peer]", "port", "8060");
    ini.setStringValue(" Server ", "port", "8060");

    EXPECT_TRUE(ini.isEmpty());
}

TEST_F(IniFileTest, remove_value)
{
    writeFile("[Server]\nport=8060\nhost=127.0.0.1\n");

    IniFile ini(filePath());
    ini.removeValue("Server", "port");
    ini.removeValue("Server", "unknown_key");
    ini.removeValue("Peer", "port");

    EXPECT_TRUE(ini.stringValue("Server", "port").isEmpty());
    EXPECT_EQ(ini.stringValue("Server", "host"), "127.0.0.1");
}

TEST_F(IniFileTest, remove_of_last_value_makes_settings_empty)
{
    writeFile("[Server]\nport=8060\n");

    IniFile ini(filePath());
    ini.removeValue("Server", "port");

    EXPECT_TRUE(ini.isEmpty());

    ASSERT_TRUE(ini.sync());
    EXPECT_TRUE(readFile().isEmpty());
}
