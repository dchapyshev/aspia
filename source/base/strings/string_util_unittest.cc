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

#include "base/strings/string_util.h"

#include <gtest/gtest.h>

namespace base {

TEST(StringUtilTest, CaseCompare)
{
    int ret = compareCaseInsensitive(u"qWeRtY", u"qwerty");
    EXPECT_EQ(ret, 0);

    ret = compareCaseInsensitive(u"qwerty", u"qwerty");
    EXPECT_EQ(ret, 0);

    ret = compareCaseInsensitive(u"QWERTY", u"QWERTY");
    EXPECT_EQ(ret, 0);

    ret = compareCaseInsensitive(u"1WERTY", u"QWERTY");
    EXPECT_NE(ret, 0);
}

TEST(StringUtilTest, ToLower)
{
    std::u16string source(u"qWeRtY");
    std::u16string target = toLower(source);
    EXPECT_FALSE(target.empty());
    EXPECT_EQ(u"qwerty", target);
}

TEST(StringUtilTest, ToUpper)
{
    std::u16string source(u"qWeRtY");
    std::u16string target = toUpper(source);
    EXPECT_FALSE(target.empty());
    EXPECT_EQ(u"QWERTY", target);
}

} // namespace base
