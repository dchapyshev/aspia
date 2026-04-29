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

#include "base/crypto/data_cryptor.h"

#include <gtest/gtest.h>

#include <QByteArray>

namespace base {

TEST(DataCryptorTest, EncryptDecrypt)
{
    const QByteArray key = QByteArray::fromHex("1ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");
    const QByteArray message =
        QByteArray::fromHex("5ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014"
                "5ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");

    DataCryptor cryptor(key);

    std::optional<QByteArray> encrypted_message = cryptor.encrypt(message);
    ASSERT_TRUE(encrypted_message.has_value());
    ASSERT_EQ(encrypted_message->size(), message.size() + 28);

    std::optional<QByteArray> decrypted_message = cryptor.decrypt(*encrypted_message);
    ASSERT_TRUE(decrypted_message.has_value());
    ASSERT_EQ(decrypted_message->size(), message.size());
    ASSERT_EQ(*decrypted_message, message);
}

TEST(DataCryptorTest, WrongKey)
{
    const QByteArray key = QByteArray::fromHex("1ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");
    const QByteArray wrong_key = QByteArray::fromHex("2ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");
    const QByteArray message = QByteArray::fromHex("3da8b455bd39746dc50145ce26794165a808ec425684e9384c27c2249951256812125683");

    DataCryptor cryptor1(key);
    DataCryptor cryptor2(wrong_key);

    std::optional<QByteArray> encrypted_message = cryptor1.encrypt(message);
    ASSERT_TRUE(encrypted_message.has_value());
    ASSERT_EQ(encrypted_message->size(), message.size() + 28);

    std::optional<QByteArray> decrypted_message = cryptor2.decrypt(*encrypted_message);
    ASSERT_FALSE(decrypted_message.has_value());
}

TEST(DataCryptorTest, Passthrough)
{
    DataCryptor cryptor;

    const QByteArray message = QByteArray::fromHex("5ce26794165a808ec425684e9384c27c");

    std::optional<QByteArray> encrypted = cryptor.encrypt(message);
    ASSERT_TRUE(encrypted.has_value());
    ASSERT_EQ(*encrypted, message);

    std::optional<QByteArray> decrypted = cryptor.decrypt(*encrypted);
    ASSERT_TRUE(decrypted.has_value());
    ASSERT_EQ(*decrypted, message);
}

} // namespace base
