//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "base/guid.h"
#include "base/string_util.h"

namespace aspia {

namespace {

bool isGUIDv4(const std::string& guid)
{
    // The format of GUID version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
    // where y is one of [8, 9, A, B].
    return Guid::isValid(guid) && guid[14] == '4' &&
        (guid[19] == '8' || guid[19] == '9' || guid[19] == 'A' ||
         guid[19] == 'a' || guid[19] == 'B' || guid[19] == 'b');
}

} // namespace

TEST(guid_test, guid_generates_all_zeroes)
{
    uint64_t bytes[] = { 0, 0 };
    std::string clientid = Guid::randomDataToGUIDString(bytes);
    EXPECT_EQ("00000000-0000-0000-0000-000000000000", clientid);
}

TEST(guid_test, guid_generates_correctly)
{
    uint64_t bytes[] = { 0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL };
    std::string clientid = Guid::randomDataToGUIDString(bytes);
    EXPECT_EQ("01234567-89ab-cdef-fedc-ba9876543210", clientid);
}

TEST(guid_test, guid_correctly_formatted)
{
    const int kIterations = 10;
    for (int it = 0; it < kIterations; ++it)
    {
        std::string guid = Guid::create();
        EXPECT_TRUE(Guid::isValid(guid));
        EXPECT_TRUE(Guid::isStrictValid(guid));
        EXPECT_TRUE(Guid::isValid(toLowerASCII(guid)));
        EXPECT_TRUE(Guid::isValid(toUpperASCII(guid)));
    }
}

TEST(guid_test, guid_basic_uniqueness)
{
    const int kIterations = 10;
    for (int it = 0; it < kIterations; ++it)
    {
        std::string guid1 = Guid::create();
        std::string guid2 = Guid::create();
        EXPECT_EQ(36U, guid1.length());
        EXPECT_EQ(36U, guid2.length());
        EXPECT_NE(guid1, guid2);
        EXPECT_TRUE(isGUIDv4(guid1));
        EXPECT_TRUE(isGUIDv4(guid2));
    }
}

} // namespace aspia
