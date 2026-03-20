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

#include "base/peer/host_id.h"

#include <gtest/gtest.h>

namespace base {

// isHostId tests.

TEST(host_id_test, is_host_id_valid_digits)
{
    EXPECT_TRUE(isHostId("123456"));
    EXPECT_TRUE(isHostId("0"));
    EXPECT_TRUE(isHostId("99999999999"));
}

TEST(host_id_test, is_host_id_empty_string)
{
    EXPECT_FALSE(isHostId(""));
    EXPECT_FALSE(isHostId(QString()));
}

TEST(host_id_test, is_host_id_with_letters)
{
    EXPECT_FALSE(isHostId("abc"));
    EXPECT_FALSE(isHostId("123abc"));
    EXPECT_FALSE(isHostId("abc123"));
}

TEST(host_id_test, is_host_id_with_special_chars)
{
    EXPECT_FALSE(isHostId("123-456"));
    EXPECT_FALSE(isHostId("123.456"));
    EXPECT_FALSE(isHostId(" 123"));
    EXPECT_FALSE(isHostId("123 "));
    EXPECT_FALSE(isHostId("+123"));
    EXPECT_FALSE(isHostId("-1"));
}

// stringToHostId tests.

TEST(host_id_test, string_to_host_id_valid)
{
    EXPECT_EQ(stringToHostId("123456"), 123456ULL);
    EXPECT_EQ(stringToHostId("1"), 1ULL);
    EXPECT_EQ(stringToHostId("18446744073709551615"), 18446744073709551615ULL); // max uint64
}

TEST(host_id_test, string_to_host_id_zero)
{
    // "0" converts to 0, but 0 == kInvalidHostId.
    // toULongLong succeeds and returns 0.
    EXPECT_EQ(stringToHostId("0"), kInvalidHostId);
}

TEST(host_id_test, string_to_host_id_empty)
{
    EXPECT_EQ(stringToHostId(""), kInvalidHostId);
    EXPECT_EQ(stringToHostId(QString()), kInvalidHostId);
}

TEST(host_id_test, string_to_host_id_invalid_chars)
{
    EXPECT_EQ(stringToHostId("abc"), kInvalidHostId);
    EXPECT_EQ(stringToHostId("12abc"), kInvalidHostId);
    EXPECT_EQ(stringToHostId("-1"), kInvalidHostId);
}

TEST(host_id_test, string_to_host_id_overflow)
{
    // Value larger than max uint64.
    EXPECT_EQ(stringToHostId("99999999999999999999999"), kInvalidHostId);
}

// hostIdToString tests.

TEST(host_id_test, host_id_to_string_valid)
{
    EXPECT_EQ(hostIdToString(123456), QString("123456"));
    EXPECT_EQ(hostIdToString(1), QString("1"));
    EXPECT_EQ(hostIdToString(18446744073709551615ULL), QString("18446744073709551615"));
}

TEST(host_id_test, host_id_to_string_invalid)
{
    EXPECT_TRUE(hostIdToString(kInvalidHostId).isEmpty());
}

// Round-trip tests.

TEST(host_id_test, round_trip)
{
    HostId id = 987654321ULL;
    EXPECT_EQ(stringToHostId(hostIdToString(id)), id);
}

TEST(host_id_test, round_trip_large)
{
    HostId id = 18446744073709551615ULL;
    EXPECT_EQ(stringToHostId(hostIdToString(id)), id);
}

TEST(host_id_test, invalid_host_id_is_zero)
{
    EXPECT_EQ(kInvalidHostId, 0ULL);
}

} // namespace base
