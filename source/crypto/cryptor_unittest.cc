//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "crypto/cryptor_aes256_gcm.h"
#include "crypto/cryptor_chacha20_poly1305.h"

#include <gtest/gtest.h>

namespace crypto {

void testVector(Cryptor* client_cryptor, Cryptor* host_cryptor)
{
    QByteArray message_for_host = QByteArray::fromHex(
        "6006ee8029610876ec2facd5fc9ce6bd6dc03d4a5ddb4d6c28f2ff048d4f7eb7bcf5048c901a4adaa7fd8aa65bc95ca1d9f21ced474a45e9c6e7344184d6d715");

    QByteArray encrypted_msg_for_host;

    encrypted_msg_for_host.resize(
        client_cryptor->encryptedDataSize(message_for_host.size()));
    ASSERT_EQ(encrypted_msg_for_host.size(), message_for_host.size() + 16);

    bool ret = client_cryptor->encrypt(message_for_host.constData(),
                                       message_for_host.size(),
                                       encrypted_msg_for_host.data());
    ASSERT_TRUE(ret);

    QByteArray decrypted_msg_for_host;

    decrypted_msg_for_host.resize(
        host_cryptor->decryptedDataSize(encrypted_msg_for_host.size()));
    ASSERT_EQ(decrypted_msg_for_host.size(), encrypted_msg_for_host.size() - 16);

    ret = host_cryptor->decrypt(encrypted_msg_for_host.constData(),
                                encrypted_msg_for_host.size(),
                                decrypted_msg_for_host.data());
    ASSERT_TRUE(ret);
    ASSERT_EQ(decrypted_msg_for_host, message_for_host);

    QByteArray message_for_client = QByteArray::fromHex(
        "1a600348762c4ec6f0353c868fec5e4db96ea78be88af74b4cfbb9da19687ab9fbf90f4d7ef4b6c993b9cd784cce46d100d17b88817d");

    QByteArray encrypted_msg_for_client;

    encrypted_msg_for_client.resize(
        host_cryptor->encryptedDataSize(message_for_client.size()));
    ASSERT_EQ(encrypted_msg_for_client.size(), message_for_client.size() + 16);

    ret = host_cryptor->encrypt(message_for_client.constData(),
                                message_for_client.size(),
                                encrypted_msg_for_client.data());
    ASSERT_TRUE(ret);

    QByteArray decrypted_msg_for_client;

    decrypted_msg_for_client.resize(
        client_cryptor->decryptedDataSize(encrypted_msg_for_client.size()));
    ASSERT_EQ(decrypted_msg_for_client.size(), encrypted_msg_for_client.size() - 16);

    ret = client_cryptor->decrypt(encrypted_msg_for_client.constData(),
                                  encrypted_msg_for_client.size(),
                                  decrypted_msg_for_client.data());
    ASSERT_TRUE(ret);
    ASSERT_EQ(decrypted_msg_for_client, message_for_client);
}

void wrongKey(Cryptor* client_cryptor, Cryptor* host_cryptor)
{
    QByteArray message_for_host = QByteArray::fromHex(
        "6006ee8029610876ec2facd5fc9ce6bd6dc03d4a5ddb4d6c28f2ff048d4f7eb7bcf5048c901a4adaa7fd");

    QByteArray encrypted_msg_for_host;

    encrypted_msg_for_host.resize(
        client_cryptor->encryptedDataSize(message_for_host.size()));
    ASSERT_EQ(encrypted_msg_for_host.size(), message_for_host.size() + 16);

    bool ret = client_cryptor->encrypt(message_for_host.constData(),
                                       message_for_host.size(),
                                       encrypted_msg_for_host.data());
    ASSERT_TRUE(ret);

    QByteArray decrypted_msg_for_host;

    decrypted_msg_for_host.resize(
        host_cryptor->decryptedDataSize(encrypted_msg_for_host.size()));
    ASSERT_EQ(decrypted_msg_for_host.size(), encrypted_msg_for_host.size() - 16);

    ret = host_cryptor->decrypt(encrypted_msg_for_host.constData(),
                                encrypted_msg_for_host.size(),
                                decrypted_msg_for_host.data());
    ASSERT_FALSE(ret);
}

TEST(CryptorAes256GcmTest, TestVector)
{
    const QByteArray key =
        QByteArray::fromHex("5ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");
    const QByteArray encrypt_iv = QByteArray::fromHex("ee7eb0e6fb24d445597f3e6f");
    const QByteArray decrypt_iv = QByteArray::fromHex("924988304848184805f07167");

    EXPECT_EQ(key.size(), 32);
    EXPECT_EQ(encrypt_iv.size(), 12);
    EXPECT_EQ(decrypt_iv.size(), 12);

    std::unique_ptr<Cryptor> client_cryptor(CryptorAes256Gcm::create(key, encrypt_iv, decrypt_iv));
    ASSERT_NE(client_cryptor, nullptr);

    std::unique_ptr<Cryptor> host_cryptor(CryptorAes256Gcm::create(key, decrypt_iv, encrypt_iv));
    ASSERT_NE(host_cryptor, nullptr);

    for (int i = 0; i < 100; ++i)
    {
        testVector(client_cryptor.get(), host_cryptor.get());
    }
}

TEST(CryptorAes256GcmTest, WrongKey)
{
    const QByteArray client_key =
        QByteArray::fromHex("5ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");
    const QByteArray host_key =
        QByteArray::fromHex("1ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");
    const QByteArray encrypt_iv = QByteArray::fromHex("ee7eb0e6fb24d445597f3e6f");
    const QByteArray decrypt_iv = QByteArray::fromHex("924988304848184805f07167");

    EXPECT_EQ(client_key.size(), 32);
    EXPECT_EQ(encrypt_iv.size(), 12);
    EXPECT_EQ(decrypt_iv.size(), 12);

    std::unique_ptr<Cryptor> client_cryptor(
        CryptorAes256Gcm::create(client_key, encrypt_iv, decrypt_iv));
    ASSERT_NE(client_cryptor, nullptr);

    std::unique_ptr<Cryptor> host_cryptor(
        CryptorAes256Gcm::create(host_key, decrypt_iv, encrypt_iv));
    ASSERT_NE(host_cryptor, nullptr);

    wrongKey(client_cryptor.get(), host_cryptor.get());
}

TEST(CryptorChaCha20Poly1305Test, TestVector)
{
    const QByteArray key =
        QByteArray::fromHex("5ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");
    const QByteArray encrypt_iv = QByteArray::fromHex("ee7eb0e6fb24d445597f3e6f");
    const QByteArray decrypt_iv = QByteArray::fromHex("924988304848184805f07167");

    EXPECT_EQ(key.size(), 32);
    EXPECT_EQ(encrypt_iv.size(), 12);
    EXPECT_EQ(decrypt_iv.size(), 12);

    std::unique_ptr<Cryptor> client_cryptor(
        CryptorChaCha20Poly1305::create(key, encrypt_iv, decrypt_iv));
    ASSERT_NE(client_cryptor, nullptr);

    std::unique_ptr<Cryptor> host_cryptor(
        CryptorChaCha20Poly1305::create(key, decrypt_iv, encrypt_iv));
    ASSERT_NE(host_cryptor, nullptr);

    for (int i = 0; i < 100; ++i)
    {
        testVector(client_cryptor.get(), host_cryptor.get());
    }
}

TEST(CryptorChaCha20Poly1305Test, WrongKey)
{
    const QByteArray client_key =
        QByteArray::fromHex("5ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");
    const QByteArray host_key =
        QByteArray::fromHex("1ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");
    const QByteArray encrypt_iv = QByteArray::fromHex("ee7eb0e6fb24d445597f3e6f");
    const QByteArray decrypt_iv = QByteArray::fromHex("924988304848184805f07167");

    EXPECT_EQ(client_key.size(), 32);
    EXPECT_EQ(encrypt_iv.size(), 12);
    EXPECT_EQ(decrypt_iv.size(), 12);

    std::unique_ptr<Cryptor> client_cryptor(
        CryptorChaCha20Poly1305::create(client_key, encrypt_iv, decrypt_iv));
    ASSERT_NE(client_cryptor, nullptr);

    std::unique_ptr<Cryptor> host_cryptor(
        CryptorChaCha20Poly1305::create(host_key, decrypt_iv, encrypt_iv));
    ASSERT_NE(host_cryptor, nullptr);

    wrongKey(client_cryptor.get(), host_cryptor.get());
}

} // namespace crypto
