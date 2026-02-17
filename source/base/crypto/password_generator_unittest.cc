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

#include "base/crypto/password_generator.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace base {

namespace {

bool hasLower(const QString& str)
{
    for (int i = 0; i < str.size(); ++i)
    {
        if (str.at(i).isLower())
            return true;
    }
    return false;
}

bool hasUpper(const QString& str)
{
    for (int i = 0; i < str.size(); ++i)
    {
        if (str.at(i).isUpper())
            return true;
    }
    return false;
}

bool hasDigit(const QString& str)
{
    for (int i = 0; i < str.size(); ++i)
    {
        if (str.at(i).isDigit())
            return true;
    }
    return false;
}

} // namespace

TEST(password_generator_test, default_params)
{
    PasswordGenerator gen;

    EXPECT_EQ(gen.length(), PasswordGenerator::kDefaultLength);
    EXPECT_EQ(gen.characters(), PasswordGenerator::kDefaultCharacters);

    gen.setLength(0);
    gen.setCharacters(0);

    EXPECT_EQ(gen.length(), PasswordGenerator::kDefaultLength);
    EXPECT_EQ(gen.characters(), PasswordGenerator::kDefaultCharacters);

    QString res1 = gen.result();
    QString res2 = gen.result();

    EXPECT_EQ(res1.length(), PasswordGenerator::kDefaultLength);
    EXPECT_EQ(res2.length(), PasswordGenerator::kDefaultLength);

    EXPECT_NE(res1, res2);
}

TEST(password_generator_test, only_lower_case)
{
    PasswordGenerator gen;

    gen.setLength(5);
    gen.setCharacters(PasswordGenerator::LOWER_CASE);

    for (int i = 0; i < 10; ++i)
    {
        QString res = gen.result();

        EXPECT_EQ(res.length(), 5);
        EXPECT_TRUE(hasLower(res));
        EXPECT_FALSE(hasUpper(res));
        EXPECT_FALSE(hasDigit(res));
    }
}

TEST(password_generator_test, only_upper_case)
{
    PasswordGenerator gen;

    gen.setLength(5);
    gen.setCharacters(PasswordGenerator::UPPER_CASE);

    for (int i = 0; i < 10; ++i)
    {
        QString res = gen.result();

        EXPECT_EQ(res.length(), 5);
        EXPECT_TRUE(hasUpper(res));
        EXPECT_FALSE(hasLower(res));
        EXPECT_FALSE(hasDigit(res));
    }
}

TEST(password_generator_test, only_digits)
{
    PasswordGenerator gen;

    gen.setLength(5);
    gen.setCharacters(PasswordGenerator::DIGITS);

    for (int i = 0; i < 10; ++i)
    {
        QString res = gen.result();

        EXPECT_EQ(res.length(), 5);
        EXPECT_TRUE(hasDigit(res));
        EXPECT_FALSE(hasLower(res));
        EXPECT_FALSE(hasUpper(res));
    }
}

TEST(password_generator_test, DISABLED_benchmark)
{
    const int kIterationCount = 100000;

    PasswordGenerator gen1;
    gen1.setLength(5);

    for (int i = 0; i < kIterationCount; ++i)
    {
        QString res = gen1.result();
        EXPECT_EQ(res.length(), 5);
    }

    PasswordGenerator gen2;
    gen2.setLength(10);

    for (int i = 0; i < kIterationCount; ++i)
    {
        QString res = gen2.result();
        EXPECT_EQ(res.length(), 10);
    }

    PasswordGenerator gen3;
    gen3.setLength(15);

    for (int i = 0; i < kIterationCount; ++i)
    {
        QString res = gen3.result();
        EXPECT_EQ(res.length(), 15);
    }
}

} // namespace base
