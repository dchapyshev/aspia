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

#include "base/crypto/generic_hash.h"

#include <gtest/gtest.h>

namespace base {

TEST(GenericHashTest, Blake2b512)
{
    struct TestData
    {
        std::string data1;
        std::string data2;
        std::string expected;
    } const kTestTable[] =
    {
        {
            "535f08970d17ee878dae4d20444dac",
            "fb1c50ac4803c1591b6cfb1420f561",
            "9c66da0b7410b12e1b1daea576d49b924988304848184805f0716714646795c788452568892c2330cff4405aa1490752b9553136ee83578a20c78b436c066416"
        },

        {
            "fb1c50ac4803c1591b6cfb1420f561",
            "535f08970d17ee878dae4d20444dac",
            "afc17e0ed2cfaa23ab62f55738b16a811da05a7c0f130e3b020bd633c38a8c28f2266d05fe363b14026a1f5dbcb5800345cb0f801a2fd1cc38bf48d33202e4da"
        }
    };

    int count = 3;

    while (count-- >= 0)
    {
        for (size_t i = 0; i < std::size(kTestTable); ++i)
        {
            GenericHash hash(GenericHash::BLAKE2b512);

            hash.addData(QByteArray::fromHex(QByteArray::fromStdString(kTestTable[i].data1)));
            hash.addData(QByteArray::fromHex(QByteArray::fromStdString(kTestTable[i].data2)));

            QByteArray expected = QByteArray::fromHex(QByteArray::fromStdString(kTestTable[i].expected));
            QByteArray result = hash.result();

            EXPECT_EQ(expected.size(), 64);
            EXPECT_EQ(result.size(), 64);
            EXPECT_EQ(result, expected);

            hash.reset();
        }
    }
}

TEST(GenericHashTest, Blake2s256)
{
    struct TestData
    {
        std::string data1;
        std::string data2;
        std::string expected;
    } const kTestTable[] =
    {
        {
            "535f08970d17ee878dae4d20444dac",
            "fb1c50ac4803c1591b6cfb1420f561",
            "ee7eb0e6fb24d445597f3e6f1e0cdd649c71f7cd5c9699e270d5dc69f2d6baa5"
        },

        {
            "fb1c50ac4803c1591b6cfb1420f561",
            "535f08970d17ee878dae4d20444dac",
            "a180f6b02ceba827b430e88eeff0c3cb7f583ca99e6537970f30efb15a66c762"
        }
    };

    int count = 3;

    while (count-- >= 0)
    {
        for (size_t i = 0; i < std::size(kTestTable); ++i)
        {
            GenericHash hash(GenericHash::BLAKE2s256);

            hash.addData(QByteArray::fromHex(QByteArray::fromStdString(kTestTable[i].data1)));
            hash.addData(QByteArray::fromHex(QByteArray::fromStdString(kTestTable[i].data2)));

            QByteArray expected = QByteArray::fromHex(QByteArray::fromStdString(kTestTable[i].expected));
            QByteArray result = hash.result();

            EXPECT_EQ(expected.size(), 32);
            EXPECT_EQ(result.size(), 32);
            EXPECT_EQ(result, expected);

            hash.reset();
        }
    }
}

} // namespace base
