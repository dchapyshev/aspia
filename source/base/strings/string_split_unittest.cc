//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/strings/string_split.h"

#include "base/macros_magic.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_constants.h"
#include "base/strings/unicode.h"

#include <cstddef>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;

namespace base {

TEST(StringUtilTest, SplitString_Basics)
{
    std::vector<std::string> r;

    r = splitString(std::string(), ",:;", KEEP_WHITESPACE, SPLIT_WANT_ALL);
    EXPECT_TRUE(r.empty());

    // Empty separator list
    r = splitString("hello, world", "", KEEP_WHITESPACE, SPLIT_WANT_ALL);
    ASSERT_EQ(1u, r.size());
    EXPECT_EQ("hello, world", r[0]);

    // Should split on any of the separators.
    r = splitString("::,,;;", ",:;", KEEP_WHITESPACE, SPLIT_WANT_ALL);
    ASSERT_EQ(7u, r.size());
    for (auto str : r)
        ASSERT_TRUE(str.empty());

    r = splitString("red, green; blue:", ",:;", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
    ASSERT_EQ(3u, r.size());
    EXPECT_EQ("red", r[0]);
    EXPECT_EQ("green", r[1]);
    EXPECT_EQ("blue", r[2]);

    // Want to split a string along whitespace sequences.
    r = splitString("  red green   \tblue\n", " \t\n", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
    ASSERT_EQ(3u, r.size());
    EXPECT_EQ("red", r[0]);
    EXPECT_EQ("green", r[1]);
    EXPECT_EQ("blue", r[2]);

    // Weird case of splitting on spaces but not trimming.
    r = splitString(" red ", " ", TRIM_WHITESPACE, SPLIT_WANT_ALL);
    ASSERT_EQ(3u, r.size());
    EXPECT_EQ("", r[0]);  // Before the first space.
    EXPECT_EQ("red", r[1]);
    EXPECT_EQ("", r[2]);  // After the last space.
}

TEST(StringUtilTest, SplitString_WhitespaceAndResultType)
{
    std::vector<std::string> r;

    // Empty input handling.
    r = splitString(std::string(), ",", KEEP_WHITESPACE, SPLIT_WANT_ALL);
    EXPECT_TRUE(r.empty());
    r = splitString(std::string(), ",", KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY);
    EXPECT_TRUE(r.empty());

    // Input string is space and we're trimming.
    r = splitString(" ", ",", TRIM_WHITESPACE, SPLIT_WANT_ALL);
    ASSERT_EQ(1u, r.size());
    EXPECT_EQ("", r[0]);
    r = splitString(" ", ",", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
    EXPECT_TRUE(r.empty());

    // Test all 4 combinations of flags on ", ,".
    r = splitString(", ,", ",", KEEP_WHITESPACE, SPLIT_WANT_ALL);
    ASSERT_EQ(3u, r.size());
    EXPECT_EQ("", r[0]);
    EXPECT_EQ(" ", r[1]);
    EXPECT_EQ("", r[2]);
    r = splitString(", ,", ",", KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY);
    ASSERT_EQ(1u, r.size());
    ASSERT_EQ(" ", r[0]);
    r = splitString(", ,", ",", TRIM_WHITESPACE, SPLIT_WANT_ALL);
    ASSERT_EQ(3u, r.size());
    EXPECT_EQ("", r[0]);
    EXPECT_EQ("", r[1]);
    EXPECT_EQ("", r[2]);
    r = splitString(", ,", ",", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
    ASSERT_TRUE(r.empty());
}

TEST(StringSplitTest, StringSplitKeepWhitespace)
{
    std::vector<std::string> r;

    r = splitString("   ", "*", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    ASSERT_EQ(1U, r.size());
    EXPECT_EQ(r[0], "   ");

    r = splitString("\t  \ta\t ", "\t", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    ASSERT_EQ(4U, r.size());
    EXPECT_EQ(r[0], "");
    EXPECT_EQ(r[1], "  ");
    EXPECT_EQ(r[2], "a");
    EXPECT_EQ(r[3], " ");

    r = splitString("\ta\t\nb\tcc", "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    ASSERT_EQ(2U, r.size());
    EXPECT_EQ(r[0], "\ta\t");
    EXPECT_EQ(r[1], "b\tcc");
}

TEST(StringSplitTest, SplitStringAlongWhitespace)
{
    struct TestData
    {
        const char* input;
        const size_t expected_result_count;
        const char* output1;
        const char* output2;
    } data[] =
    {
        { "a", 1, "a", "" },
        { " ", 0, "", "" },
        { " a", 1, "a", "" },
        { " ab ", 1, "ab", "" },
        { " ab c", 2, "ab", "c" },
        { " ab c ", 2, "ab", "c" },
        { " ab cd", 2, "ab", "cd" },
        { " ab cd ", 2, "ab", "cd" },
        { " \ta\t", 1, "a", "" },
        { " b\ta\t", 2, "b", "a" },
        { " b\tat", 2, "b", "at" },
        { "b\tat", 2, "b", "at" },
        { "b\t at", 2, "b", "at" },
    };

    for (const auto& i : data)
    {
        std::vector<std::string> results =
            splitString(i.input, kWhitespaceASCII, KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY);
        ASSERT_EQ(i.expected_result_count, results.size());
        if (i.expected_result_count > 0)
            ASSERT_EQ(i.output1, results[0]);
        if (i.expected_result_count > 1)
            ASSERT_EQ(i.output2, results[1]);
    }
}

} // namespace base
