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

#include "base/stl_util.h"

#include <gtest/gtest.h>

#include <map>

namespace base {

TEST(STLUtilTest, GenericContains)
{
    const char allowed_chars[] = { 'a', 'b', 'c', 'd' };
    EXPECT_TRUE(contains(allowed_chars, 'a'));
    EXPECT_FALSE(contains(allowed_chars, 'z'));
    EXPECT_FALSE(contains(allowed_chars, 0));

    const char allowed_chars_including_nul[] = "abcd";
    EXPECT_TRUE(contains(allowed_chars_including_nul, 0));
}

TEST(STLUtilTest, ContainsWithFindAndNpos)
{
    std::string str = "abcd";

    EXPECT_TRUE(contains(str, 'a'));
    EXPECT_FALSE(contains(str, 'z'));
    EXPECT_FALSE(contains(str, 0));
}

TEST(STLUtilTest, ContainsWithFindAndEnd)
{
    std::set<int> set = {1, 2, 3, 4};

    EXPECT_TRUE(contains(set, 1));
    EXPECT_FALSE(contains(set, 5));
    EXPECT_FALSE(contains(set, 0));
}

TEST(ContainsKey, Map)
{
    std::map<char, char> map;

    map.emplace('a', '0');
    map.emplace('b', '1');
    map.emplace('c', '2');
    map.emplace('d', '3');

    EXPECT_TRUE(contains(map, 'a'));
    EXPECT_FALSE(contains(map, 'z'));
    EXPECT_FALSE(contains(map, 0));
}

} // namespace base
