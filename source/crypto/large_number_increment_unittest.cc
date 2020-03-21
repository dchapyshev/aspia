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

#include "crypto/large_number_increment.h"

#include <gtest/gtest.h>

namespace crypto {

TEST(LargeNumberIncrementTest, Test)
{
    base::ByteArray number1 = base::fromHex("0000000000000000");
    base::ByteArray result1 = base::fromHex("0000000000989680");

    for (size_t i = 0; i < 10000000; ++i)
    {
        largeNumberIncrement(&number1);
    }

    EXPECT_EQ(number1, result1);

    base::ByteArray number2 = base::fromHex("FFFFFFFFFFFFFFFA");
    base::ByteArray result2 = base::fromHex("0000000000000004");

    for (size_t i = 0; i < 10; ++i)
    {
        largeNumberIncrement(&number2);
    }

    EXPECT_EQ(number2, result2);
}

} // namespace crypto
