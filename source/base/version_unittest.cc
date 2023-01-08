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

#include "base/version.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace {

TEST(VersionTest, DefaultConstructor)
{
    base::Version v;
    EXPECT_FALSE(v.isValid());
}

TEST(VersionTest, ValueSemantics)
{
    base::Version v1("1.2.3.4");
    EXPECT_TRUE(v1.isValid());
    base::Version v3;
    EXPECT_FALSE(v3.isValid());
    {
        base::Version v2(v1);
        v3 = v2;
        EXPECT_TRUE(v2.isValid());
        EXPECT_EQ(v1, v2);
    }
    EXPECT_EQ(v3, v1);
}

TEST(VersionTest, MoveSemantics)
{
    const std::vector<uint32_t> components = { 1, 2, 3, 4 };
    base::Version v1(components);
    EXPECT_TRUE(v1.isValid());
    base::Version v2("1.2.3.4");
    EXPECT_EQ(v1, v2);
}

TEST(VersionTest, GetVersionFromString)
{
    static const struct version_string
    {
        const char* input;
        size_t parts;
        uint32_t firstpart;
        bool success;
    } cases[] =
    {
        { "", 0, 0, false },
        { " ", 0, 0, false },
        { "\t", 0, 0, false },
        { "\n", 0, 0, false },
        { "  ", 0, 0, false },
        { ".", 0, 0, false },
        { " . ", 0, 0, false },
        { "0", 1, 0, true },
        { "0.", 0, 0, false },
        { "0.0", 2, 0, true },
        { "4294967295.0", 2, 4294967295, true },
        { "4294967296.0", 0, 0, false },
        { "-1.0", 0, 0, false },
        { "1.-1.0", 0, 0, false },
        { "1,--1.0", 0, 0, false },
        { "+1.0", 0, 0, false },
        { "1.+1.0", 0, 0, false },
        { "1+1.0", 0, 0, false },
        { "++1.0", 0, 0, false },
        { "1.0a", 0, 0, false },
        { "1.2.3.4.5.6.7.8.9.0", 10, 1, true },
        { "02.1", 0, 0, false },
        { "0.01", 2, 0, true },
        { "f.1", 0, 0, false },
        { "15.007.20011", 3, 15, true },
        { "15.5.28.130162", 4, 15, true },
    };

    int index = 0;

    for (const auto& i : cases)
    {
        base::Version version(i.input);
        EXPECT_EQ(i.success, version.isValid());
        if (i.success)
        {
            EXPECT_EQ(i.parts, version.components().size());
            EXPECT_EQ(i.firstpart, version.components()[0]);
        }
        ++index;
    }
}

TEST(VersionTest, Compare)
{
    static const struct version_compare
    {
        const char* lhs;
        const char* rhs;
        int expected;
    } cases[] =
    {
        { "1.0", "1.0", 0 },
        { "1.0", "0.0", 1 },
        { "1.0", "2.0", -1 },
        { "1.0", "1.1", -1 },
        { "1.1", "1.0", 1 },
        { "1.0", "1.0.1", -1 },
        { "1.1", "1.0.1", 1 },
        { "1.1", "1.0.1", 1 },
        { "1.0.0", "1.0", 0 },
        { "1.0.3", "1.0.20", -1 },
        { "11.0.10", "15.007.20011", -1 },
        { "11.0.10", "15.5.28.130162", -1 },
        { "15.5.28.130162", "15.5.28.130162", 0 },
    };

    for (const auto& i : cases)
    {
        base::Version lhs(i.lhs);
        base::Version rhs(i.rhs);
        EXPECT_EQ(lhs.compareTo(rhs), i.expected) << i.lhs << " ? " << i.rhs;
        // CompareToWildcardString() should have same behavior as CompareTo() when
        // no wildcards are present.
        EXPECT_EQ(lhs.compareToWildcardString(i.rhs), i.expected)
            << i.lhs << " ? " << i.rhs;
        EXPECT_EQ(rhs.compareToWildcardString(i.lhs), -i.expected)
            << i.lhs << " ? " << i.rhs;

        // Test comparison operators
        switch (i.expected)
        {
            case -1:
                EXPECT_LT(lhs, rhs);
                EXPECT_LE(lhs, rhs);
                EXPECT_NE(lhs, rhs);
                EXPECT_FALSE(lhs == rhs);
                EXPECT_FALSE(lhs >= rhs);
                EXPECT_FALSE(lhs > rhs);
                break;
            case 0:
                EXPECT_FALSE(lhs < rhs);
                EXPECT_LE(lhs, rhs);
                EXPECT_FALSE(lhs != rhs);
                EXPECT_EQ(lhs, rhs);
                EXPECT_GE(lhs, rhs);
                EXPECT_FALSE(lhs > rhs);
                break;
            case 1:
                EXPECT_FALSE(lhs < rhs);
                EXPECT_FALSE(lhs <= rhs);
                EXPECT_NE(lhs, rhs);
                EXPECT_FALSE(lhs == rhs);
                EXPECT_GE(lhs, rhs);
                EXPECT_GT(lhs, rhs);
                break;
        }
    }
}

TEST(VersionTest, CompareToWildcardString)
{
    static const struct version_compare
    {
        const char* lhs;
        const char* rhs;
        int expected;
    } cases[] =
    {
        { "1.0", "1.*", 0 },
        { "1.0", "0.*", 1 },
        { "1.0", "2.*", -1 },
        { "1.2.3", "1.2.3.*", 0 },
        { "10.0", "1.0.*", 1 },
        { "1.0", "3.0.*", -1 },
        { "1.4", "1.3.0.*", 1 },
        { "1.3.9", "1.3.*", 0 },
        { "1.4.1", "1.3.*", 1 },
        { "1.3", "1.4.5.*", -1 },
        { "1.5", "1.4.5.*", 1 },
        { "1.3.9", "1.3.*", 0 },
        { "1.2.0.0.0.0", "1.2.*", 0 },
    };

    for (const auto& i : cases)
    {
        const base::Version version(i.lhs);
        const int result = version.compareToWildcardString(i.rhs);
        EXPECT_EQ(result, i.expected) << i.lhs << "?" << i.rhs;
    }
}

TEST(VersionTest, IsValidWildcardString)
{
    static const struct version_compare
    {
        const char* version;
        bool expected;
    } cases[] =
    {
        { "1.0", true },
        { "", false },
        { "1.2.3.4.5.6", true },
        { "1.2.3.*", true },
        { "1.2.3.5*", false },
        { "1.2.3.56*", false },
        { "1.*.3", false },
        { "20.*", true },
        { "+2.*", false },
        { "*", false },
        { "*.2", false },
    };
    for (const auto& i : cases)
    {
        EXPECT_EQ(base::Version::isValidWildcardString(i.version), i.expected)
            << i.version << "?" << i.expected;
    }
}

} // namespace
