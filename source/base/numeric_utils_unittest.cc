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

#include "base/numeric_utils.h"

#include <gtest/gtest.h>

namespace base {

// ============================================================================
// makeUint32
// ============================================================================

TEST(numeric_utils_test, make_uint32_basic)
{
    EXPECT_EQ(makeUint32(0x0001, 0x0002), 0x00010002u);
    EXPECT_EQ(makeUint32(0x1234, 0x5678), 0x12345678u);
    EXPECT_EQ(makeUint32(0xABCD, 0xEF01), 0xABCDEF01u);
}

TEST(numeric_utils_test, make_uint32_zeros)
{
    EXPECT_EQ(makeUint32(0, 0), 0u);
    EXPECT_EQ(makeUint32(0, 0x1234), 0x00001234u);
    EXPECT_EQ(makeUint32(0x1234, 0), 0x12340000u);
}

TEST(numeric_utils_test, make_uint32_max_values)
{
    EXPECT_EQ(makeUint32(0xFFFF, 0xFFFF), 0xFFFFFFFFu);
    EXPECT_EQ(makeUint32(0xFFFF, 0x0000), 0xFFFF0000u);
    EXPECT_EQ(makeUint32(0x0000, 0xFFFF), 0x0000FFFFu);
}

// ============================================================================
// highWord
// ============================================================================

TEST(numeric_utils_test, high_word_basic)
{
    EXPECT_EQ(highWord(0x12345678), 0x1234);
    EXPECT_EQ(highWord(0xABCDEF01), 0xABCD);
}

TEST(numeric_utils_test, high_word_zero)
{
    EXPECT_EQ(highWord(0x00000000), 0x0000);
    EXPECT_EQ(highWord(0x0000FFFF), 0x0000);
}

TEST(numeric_utils_test, high_word_max)
{
    EXPECT_EQ(highWord(0xFFFFFFFF), 0xFFFF);
    EXPECT_EQ(highWord(0xFFFF0000), 0xFFFF);
}

// ============================================================================
// lowWord
// ============================================================================

TEST(numeric_utils_test, low_word_basic)
{
    EXPECT_EQ(lowWord(0x12345678), 0x5678);
    EXPECT_EQ(lowWord(0xABCDEF01), 0xEF01);
}

TEST(numeric_utils_test, low_word_zero)
{
    EXPECT_EQ(lowWord(0x00000000), 0x0000);
    EXPECT_EQ(lowWord(0xFFFF0000), 0x0000);
}

TEST(numeric_utils_test, low_word_max)
{
    EXPECT_EQ(lowWord(0xFFFFFFFF), 0xFFFF);
    EXPECT_EQ(lowWord(0x0000FFFF), 0xFFFF);
}

// ============================================================================
// Roundtrip: makeUint32 <-> highWord / lowWord
// ============================================================================

TEST(numeric_utils_test, roundtrip_make_then_split)
{
    const quint16 high = 0x1234;
    const quint16 low = 0x5678;
    const quint32 combined = makeUint32(high, low);

    EXPECT_EQ(highWord(combined), high);
    EXPECT_EQ(lowWord(combined), low);
}

TEST(numeric_utils_test, roundtrip_split_then_make)
{
    const quint32 original = 0xDEADBEEF;
    const quint32 restored = makeUint32(highWord(original), lowWord(original));

    EXPECT_EQ(restored, original);
}

TEST(numeric_utils_test, roundtrip_boundary_values)
{
    const quint32 values[] = { 0x00000000, 0x00000001, 0x00010000, 0x80008000, 0xFFFFFFFF };
    for (quint32 v : values)
    {
        EXPECT_EQ(makeUint32(highWord(v), lowWord(v)), v) << "failed for value 0x" << std::hex << v;
    }
}

} // namespace base
