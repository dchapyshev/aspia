//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/base64.h"

#include <gtest/gtest.h>

namespace base {

namespace {

const char kTestData1[] = "0123456789abcdeffedcba98765432100123456789abcdeffedcba9876543210"
                          "0123456789abcdeffedcba98765432100123456789abcdeffedcba9876543210";
const char kTestData2[] = "0000000000000000000000000000000000000000000000000000000000000000"
                          "0000000000000000000000000000000000000000000000000000000000000000";
const int kIterationCount = 100000;

} // namespace

TEST(base64_test, basic)
{
    const std::string kText = "hello world";
    const std::string kBase64Text = "aGVsbG8gd29ybGQ=";

    std::string encoded;
    std::string decoded;

    Base64::encode(kText, &encoded);
    EXPECT_EQ(kBase64Text, encoded);

    bool ok = Base64::decode(encoded, &decoded);
    EXPECT_TRUE(ok);
    EXPECT_EQ(kText, decoded);
}

TEST(base64_test, in_place)
{
    const std::string kText = "hello world";
    const std::string kBase64Text = "aGVsbG8gd29ybGQ=";
    std::string text(kText);

    Base64::encode(text, &text);
    EXPECT_EQ(kBase64Text, text);

    bool ok = Base64::decode(text, &text);
    EXPECT_TRUE(ok);
    EXPECT_EQ(text, kText);
}

TEST(base64_test, empty)
{
    const std::string kEmptyString;
    std::string encoded;
    std::string decoded;

    Base64::encode(kEmptyString, &encoded);
    EXPECT_TRUE(encoded.empty());

    bool ok = Base64::decode(kEmptyString, &decoded);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(decoded.empty());
}

TEST(base64_test, DISABLED_benchmark)
{
    for (int i = 0; i < kIterationCount; ++i)
    {
        std::string source_1(kTestData1);
        std::string encoded_1 = Base64::encode(source_1);

        std::string decoded_1;
        Base64::decode(encoded_1, &decoded_1);

        EXPECT_EQ(source_1, decoded_1);

        std::string source_2(kTestData2);
        std::string encoded_2 = Base64::encode(source_2);

        std::string decoded_2;
        Base64::decode(encoded_2, &decoded_2);

        EXPECT_EQ(source_2, decoded_2);
    }
}

} // namespace base
