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

#include "base/string_util.h"

#include <gtest/gtest.h>

#include <span>
#include <string>
#include <string_view>
#include <vector>

//--------------------------------------------------------------------------------------------------
TEST(StringUtilTest, Cat)
{
    EXPECT_EQ(strCat({}), std::string());
    EXPECT_EQ(strCat({"abc"}), std::string("abc"));
    EXPECT_EQ(strCat({"a", "b", "c"}), std::string("abc"));

    const std::string name = "id";
    const char* definition = "INTEGER NOT NULL DEFAULT 0";
    EXPECT_EQ(strCat({"ALTER TABLE \"t\" ADD COLUMN \"", name, "\" ", definition}),
              std::string("ALTER TABLE \"t\" ADD COLUMN \"id\" INTEGER NOT NULL DEFAULT 0"));
}

//--------------------------------------------------------------------------------------------------
TEST(StringUtilTest, CatSpan)
{
    const std::vector<std::string_view> parts = {"a", "bb", "ccc"};
    EXPECT_EQ(strCat(parts), std::string("abbccc"));
    EXPECT_EQ(strCat(std::span<const std::string_view>()), std::string());
}

//--------------------------------------------------------------------------------------------------
TEST(StringUtilTest, CatHandlesEmbeddedNulls)
{
    const std::string with_null("a\0b", 3);
    const std::string result = strCat({with_null, "c"});
    EXPECT_EQ(result.size(), 4u);
    EXPECT_EQ(result, std::string("a\0bc", 4));
}

//--------------------------------------------------------------------------------------------------
TEST(StringUtilTest, Trimmed)
{
    EXPECT_EQ(strTrimmed(""), std::string_view(""));
    EXPECT_EQ(strTrimmed("   "), std::string_view(""));
    EXPECT_EQ(strTrimmed("abc"), std::string_view("abc"));
    EXPECT_EQ(strTrimmed("  abc"), std::string_view("abc"));
    EXPECT_EQ(strTrimmed("abc  "), std::string_view("abc"));
    EXPECT_EQ(strTrimmed(" \t\r\n abc def \v\f "), std::string_view("abc def"));
}

//--------------------------------------------------------------------------------------------------
TEST(StringUtilTest, TrimmedKeepsMultiByteUtf8)
{
    // Cyrillic bytes are all above 0x7F and must survive trimming untouched.
    const std::string_view name = "  \xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82  ";
    EXPECT_EQ(strTrimmed(name), std::string_view("\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"));
}
