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

#include "base/crypto/private_key_cryptor.h"

#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_string.h"

#include <gtest/gtest.h>

#include <QByteArray>

TEST(PrivateKeyCryptorTest, RoundTrip)
{
    const SecureByteArray private_key(QByteArray::fromHex(
        "5ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014"));
    const SecureString password(QStringLiteral("hunter2"));
    const QByteArray salt = PrivateKeyCryptor::generateSalt();

    QByteArray encrypted = PrivateKeyCryptor::encrypt(private_key, password, salt);
    ASSERT_FALSE(encrypted.isEmpty());

    SecureByteArray decrypted = PrivateKeyCryptor::decrypt(encrypted, password, salt);
    ASSERT_FALSE(decrypted.isEmpty());
    ASSERT_EQ(decrypted, private_key);
}

TEST(PrivateKeyCryptorTest, WrongPassword)
{
    const SecureByteArray private_key(QByteArray::fromHex("deadbeefcafebabe"));
    const SecureString password(QStringLiteral("correct"));
    const SecureString wrong_password(QStringLiteral("incorrect"));
    const QByteArray salt = PrivateKeyCryptor::generateSalt();

    QByteArray encrypted = PrivateKeyCryptor::encrypt(private_key, password, salt);
    ASSERT_FALSE(encrypted.isEmpty());

    SecureByteArray decrypted = PrivateKeyCryptor::decrypt(encrypted, wrong_password, salt);
    ASSERT_TRUE(decrypted.isEmpty());
}

TEST(PrivateKeyCryptorTest, WrongSalt)
{
    const SecureByteArray private_key(QByteArray::fromHex("deadbeefcafebabe"));
    const SecureString password(QStringLiteral("hunter2"));
    const QByteArray salt1 = PrivateKeyCryptor::generateSalt();
    const QByteArray salt2 = PrivateKeyCryptor::generateSalt();
    ASSERT_NE(salt1, salt2);

    QByteArray encrypted = PrivateKeyCryptor::encrypt(private_key, password, salt1);
    ASSERT_FALSE(encrypted.isEmpty());

    SecureByteArray decrypted = PrivateKeyCryptor::decrypt(encrypted, password, salt2);
    ASSERT_TRUE(decrypted.isEmpty());
}

TEST(PrivateKeyCryptorTest, InvalidSaltSize)
{
    const SecureByteArray private_key(QByteArray::fromHex("deadbeef"));
    const SecureString password(QStringLiteral("hunter2"));
    const QByteArray short_salt(PrivateKeyCryptor::kSaltSize - 1, 0);

    QByteArray encrypted = PrivateKeyCryptor::encrypt(private_key, password, short_salt);
    ASSERT_TRUE(encrypted.isEmpty());

    SecureByteArray decrypted = PrivateKeyCryptor::decrypt(
        QByteArray(64, 0), password, short_salt);
    ASSERT_TRUE(decrypted.isEmpty());
}

TEST(PrivateKeyCryptorTest, EmptyInputs)
{
    const SecureString password(QStringLiteral("hunter2"));
    const QByteArray salt = PrivateKeyCryptor::generateSalt();

    ASSERT_TRUE(PrivateKeyCryptor::encrypt(SecureByteArray(), password, salt).isEmpty());
    ASSERT_TRUE(PrivateKeyCryptor::encrypt(
        SecureByteArray(QByteArrayLiteral("data")), SecureString(), salt).isEmpty());

    ASSERT_TRUE(PrivateKeyCryptor::decrypt(QByteArray(), password, salt).isEmpty());
    ASSERT_TRUE(PrivateKeyCryptor::decrypt(
        QByteArray(64, 0), SecureString(), salt).isEmpty());
}

TEST(PrivateKeyCryptorTest, GeneratedSaltSize)
{
    const QByteArray salt = PrivateKeyCryptor::generateSalt();
    ASSERT_EQ(salt.size(), PrivateKeyCryptor::kSaltSize);
}

TEST(PrivateKeyCryptorTest, NonDeterministic)
{
    const SecureByteArray private_key(QByteArray::fromHex("deadbeefcafebabe"));
    const SecureString password(QStringLiteral("hunter2"));
    const QByteArray salt = PrivateKeyCryptor::generateSalt();

    QByteArray encrypted1 = PrivateKeyCryptor::encrypt(private_key, password, salt);
    QByteArray encrypted2 = PrivateKeyCryptor::encrypt(private_key, password, salt);
    ASSERT_FALSE(encrypted1.isEmpty());
    ASSERT_FALSE(encrypted2.isEmpty());
    // Same key + password + salt should produce different ciphertexts due to random IV
    // inside DataCryptor.
    ASSERT_NE(encrypted1, encrypted2);

    SecureByteArray decrypted1 = PrivateKeyCryptor::decrypt(encrypted1, password, salt);
    SecureByteArray decrypted2 = PrivateKeyCryptor::decrypt(encrypted2, password, salt);
    ASSERT_EQ(decrypted1, private_key);
    ASSERT_EQ(decrypted2, private_key);
}
