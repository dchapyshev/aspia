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

#include "base/crypto/password_hash.h"

#include <gtest/gtest.h>
#include <QString>

namespace base {

TEST(PasswordHashTest, Scrypt)
{
    struct TestData
    {
        QString password;
        std::string salt;
        std::string expected;
    } const kTestTable[] =
    {
        {
            "MyPassword",
            "ee7eb0e6fb24d445597f3e6f1e0cdd649c71f7cd5c9699e270d5dc69f2d6baa5",
            "5ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014"
        },

        {
            "OtherMyPassword",
            "a180f6b02ceba827b430e88eeff0c3cb7f583ca99e6537970f30efb15a66c762",
            "aab43b47f5906b75fc87a5952ebb5febcb33e53ce26a5a487a3fe909128489e0"
        }
    };

    int count = 3;

    while (count-- >= 0)
    {
        for (size_t i = 0; i < std::size(kTestTable); ++i)
        {
            QByteArray result = PasswordHash::hash(
                PasswordHash::Type::SCRYPT,
                kTestTable[i].password,
                QByteArray::fromHex(QByteArray::fromStdString(kTestTable[i].salt)));

            QByteArray expected = QByteArray::fromHex(QByteArray::fromStdString(kTestTable[i].expected));

            EXPECT_EQ(expected.size(), 32);
            EXPECT_EQ(result.size(), 32);
            EXPECT_EQ(result, expected);
        }
    }
}

} // namespace base
