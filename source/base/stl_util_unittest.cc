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

} // namespace base
