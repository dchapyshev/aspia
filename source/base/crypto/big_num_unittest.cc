//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/crypto/big_num.h"

#include <gtest/gtest.h>

namespace base {

TEST(BigNumTest, Conversions)
{
    QByteArray hex = "00DF8CB233BD5EDF263CA842B91BC2B61AAEB92313B41CCBDEEB659EDDEAA53591D47EC559"
                      "E44F3B3A1202FFEA56EDCB11BD5D37824ACBB71E4316F3D5F63955";
    QByteArray salt = QByteArray::fromHex(hex);

    EXPECT_EQ(salt.size(), 64);

    base::BigNum salt_bn = base::BigNum::fromByteArray(salt);
    std::string salt_str = salt_bn.toStdString();

    EXPECT_EQ(salt_str.size(), 63);
}

} // namespace base
