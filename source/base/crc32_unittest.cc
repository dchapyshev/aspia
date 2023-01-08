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

#include "base/crc32.h"

#include <gtest/gtest.h>

namespace base {

// Table was generated similarly to sample code for CRC-32 given on:
// http://www.w3.org/TR/PNG/#D-CRCAppendix.
TEST(Crc32Test, TableTest)
{
    for (int i = 0; i < 256; ++i)
    {
        uint32_t checksum = i;
        for (int j = 0; j < 8; ++j)
        {
            const uint32_t kReversedPolynomial = 0xEDB88320L;
            if (checksum & 1)
                checksum = kReversedPolynomial ^ (checksum >> 1);
            else
                checksum >>= 1;
        }
        EXPECT_EQ(kCrcTable[i], checksum);
    }
}

// A CRC of nothing should always be zero.
TEST(Crc32Test, ZeroTest)
{
    EXPECT_EQ(0U, crc32(0, nullptr, 0));
}

} // namespace base
