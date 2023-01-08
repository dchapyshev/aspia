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

#include "base/memory/byte_array.h"

#include <random>

#include <gtest/gtest.h>

namespace base {

namespace {

uint64_t randomNumber64()
{
    std::random_device random_device;
    std::mt19937 engine(random_device());

    std::uniform_int_distribution<uint64_t> uniform_distance(0, std::numeric_limits<uint64_t>::max());
    return uniform_distance(engine);
}

} // namespace

TEST(ByteArray, Compare)
{
    EXPECT_EQ(compare(ByteArray(), ByteArray()), 0);
    EXPECT_EQ(compare(fromStdString("some_data"), ByteArray()), 1);
    EXPECT_EQ(compare(ByteArray(), fromStdString("some_data")), -1);

    EXPECT_EQ(compare(fromStdString("some_data"), fromStdString("some_data")), 0);
    EXPECT_EQ(compare(fromStdString("some_data"), fromStdString("some_dat")), 1);
    EXPECT_EQ(compare(fromStdString("some_dat"), fromStdString("some_data")), -1);

    EXPECT_EQ(compare(fromStdString("some_data1"), fromStdString("some_data2")), -1);
    EXPECT_EQ(compare(fromStdString("some_data2"), fromStdString("some_data1")), 1);
}

TEST(ByteArray, DataConvert)
{
    for (int i = 0; i < 10; ++i)
    {
        uint64_t random_number = randomNumber64();

        std::unique_ptr<char[]> source = std::make_unique<char[]>(sizeof(uint64_t));
        memcpy(source.get(), &random_number, sizeof(uint64_t));

        base::ByteArray array = fromData(source.get(), sizeof(uint64_t));
        EXPECT_EQ(array.size(), sizeof(uint64_t));

        EXPECT_EQ(memcmp(source.get(), array.data(), sizeof(uint64_t)), 0);
    }
}

TEST(ByteArray, StringConvert)
{
    for (int i = 0; i < 10; ++i)
    {
        uint64_t random_number = randomNumber64();

        std::string source;

        source.resize(sizeof(uint64_t));
        memcpy(source.data(), &random_number, sizeof(uint64_t));

        base::ByteArray array = fromStdString(source);
        EXPECT_EQ(array.size(), sizeof(uint64_t));

        std::string target = toStdString(array);

        EXPECT_EQ(source, target);
    }
}

TEST(ByteArray, HexConvert)
{
    std::string source_hex("00B1A6F3");

    base::ByteArray array = fromHex(source_hex);
    EXPECT_EQ(array.size(), source_hex.size() / 2);

    uint32_t bytes = 0xF3A6B100;
    EXPECT_EQ(sizeof(uint32_t), array.size());

    EXPECT_EQ(memcmp(&bytes, array.data(), array.size()), 0);

    std::string target_hex = toHex(array);
    EXPECT_EQ(source_hex, target_hex);

    struct Table
    {
        const char* hex;
        const char* expected;
    } table[] =
    {
        { "0B1",  "00B1" },
        { "0",    "00"   },
        { " 0",   "00"   },
        { "0 ",   "00"   },
        { "",     ""     },
        { " ",    ""     },
        { "    ", ""     },
        { "B%!$", "0B"   }
    };

    for (size_t i = 0; i < std::size(table); ++i)
    {
        EXPECT_EQ(toHex(fromHex(table[i].hex)), table[i].expected);
    }
}

} // namespace base
