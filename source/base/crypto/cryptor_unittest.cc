//
// Aspia Project
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

#include "base/crypto/message_encryptor.h"
#include "base/crypto/message_decryptor.h"

#include <gtest/gtest.h>

namespace base {

void testVector(MessageEncryptor* client_encryptor, MessageDecryptor* client_decryptor,
                MessageEncryptor* host_encryptor, MessageDecryptor* host_decryptor)
{
    QByteArray message_for_host = QByteArray::fromHex(
        "6006ee8029610876ec2facd5fc9ce6bd6dc03d4a5ddb4d6c28f2ff048d4f7eb7bcf5048c901a4adaa7fd8aa65bc95ca1d9f21ced474a45e9c6e7344184d6d715");

    QByteArray encrypted_msg_for_host;

    encrypted_msg_for_host.resize(static_cast<QByteArray::size_type>(
        client_encryptor->encryptedDataSize(message_for_host.size())));
    ASSERT_EQ(encrypted_msg_for_host.size(), message_for_host.size() + 16);

    bool ret = client_encryptor->encrypt(message_for_host.data(),
                                         message_for_host.size(),
                                         encrypted_msg_for_host.data());
    ASSERT_TRUE(ret);

    QByteArray decrypted_msg_for_host;

    decrypted_msg_for_host.resize(static_cast<QByteArray::size_type>(
        host_decryptor->decryptedDataSize(encrypted_msg_for_host.size())));
    ASSERT_EQ(decrypted_msg_for_host.size(), encrypted_msg_for_host.size() - 16);

    ret = host_decryptor->decrypt(encrypted_msg_for_host.data(),
                                  encrypted_msg_for_host.size(),
                                  decrypted_msg_for_host.data());
    ASSERT_TRUE(ret);
    ASSERT_EQ(decrypted_msg_for_host, message_for_host);

    QByteArray message_for_client = QByteArray::fromHex(
        "1a600348762c4ec6f0353c868fec5e4db96ea78be88af74b4cfbb9da19687ab9fbf90f4d7ef4b6c993b9cd784cce46d100d17b88817d");

    QByteArray encrypted_msg_for_client;

    encrypted_msg_for_client.resize(static_cast<QByteArray::size_type>(
        host_encryptor->encryptedDataSize(message_for_client.size())));
    ASSERT_EQ(encrypted_msg_for_client.size(), message_for_client.size() + 16);

    ret = host_encryptor->encrypt(message_for_client.data(),
                                  message_for_client.size(),
                                  encrypted_msg_for_client.data());
    ASSERT_TRUE(ret);

    QByteArray decrypted_msg_for_client;

    decrypted_msg_for_client.resize(
        static_cast<QByteArray::size_type>(client_decryptor->decryptedDataSize(encrypted_msg_for_client.size())));
    ASSERT_EQ(decrypted_msg_for_client.size(), encrypted_msg_for_client.size() - 16);

    ret = client_decryptor->decrypt(encrypted_msg_for_client.data(),
                                    encrypted_msg_for_client.size(),
                                    decrypted_msg_for_client.data());
    ASSERT_TRUE(ret);
    ASSERT_EQ(decrypted_msg_for_client, message_for_client);
}

void wrongKey(MessageEncryptor* client_encryptor, MessageDecryptor* host_decryptor)
{
    QByteArray message_for_host = QByteArray::fromHex(
        "6006ee8029610876ec2facd5fc9ce6bd6dc03d4a5ddb4d6c28f2ff048d4f7eb7bcf5048c901a4adaa7fd");

    QByteArray encrypted_msg_for_host;

    encrypted_msg_for_host.resize(static_cast<QByteArray::size_type>(
        client_encryptor->encryptedDataSize(message_for_host.size())));
    ASSERT_EQ(encrypted_msg_for_host.size(), message_for_host.size() + 16);

    bool ret = client_encryptor->encrypt(message_for_host.data(),
                                         message_for_host.size(),
                                         encrypted_msg_for_host.data());
    ASSERT_TRUE(ret);

    QByteArray decrypted_msg_for_host;

    decrypted_msg_for_host.resize(static_cast<QByteArray::size_type>(
        host_decryptor->decryptedDataSize(encrypted_msg_for_host.size())));
    ASSERT_EQ(decrypted_msg_for_host.size(), encrypted_msg_for_host.size() - 16);

    ret = host_decryptor->decrypt(encrypted_msg_for_host.data(),
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

    std::unique_ptr<MessageEncryptor> client_encryptor =
        MessageEncryptor::createForAes256Gcm(key, encrypt_iv);
    ASSERT_NE(client_encryptor, nullptr);

    std::unique_ptr<MessageDecryptor> client_decryptor =
        MessageDecryptor::createForAes256Gcm(key, decrypt_iv);
    ASSERT_NE(client_decryptor, nullptr);

    std::unique_ptr<MessageEncryptor> host_encryptor =
        MessageEncryptor::createForAes256Gcm(key, decrypt_iv);
    ASSERT_NE(host_encryptor, nullptr);

    std::unique_ptr<MessageDecryptor> host_decryptor =
        MessageDecryptor::createForAes256Gcm(key, encrypt_iv);
    ASSERT_NE(host_encryptor, nullptr);

    for (int i = 0; i < 100; ++i)
    {
        testVector(client_encryptor.get(), client_decryptor.get(),
                   host_encryptor.get(), host_decryptor.get());
    }
}

TEST(CryptorAes256GcmTest, WrongKey)
{
    const QByteArray client_key =
        QByteArray::fromHex("5ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");
    const QByteArray host_key =
        QByteArray::fromHex("1ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");
    const QByteArray iv = QByteArray::fromHex("ee7eb0e6fb24d445597f3e6f");

    EXPECT_EQ(client_key.size(), 32);
    EXPECT_EQ(iv.size(), 12);

    std::unique_ptr<MessageEncryptor> client_encryptor =
        MessageEncryptor::createForAes256Gcm(client_key, iv);
    ASSERT_NE(client_encryptor, nullptr);

    std::unique_ptr<MessageDecryptor> host_decryptor =
        MessageDecryptor::createForAes256Gcm(host_key, iv);
    ASSERT_NE(host_decryptor, nullptr);

    wrongKey(client_encryptor.get(), host_decryptor.get());
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

    std::unique_ptr<MessageEncryptor> client_encryptor =
        MessageEncryptor::createForChaCha20Poly1305(key, encrypt_iv);
    ASSERT_NE(client_encryptor, nullptr);

    std::unique_ptr<MessageDecryptor> client_decryptor =
        MessageDecryptor::createForChaCha20Poly1305(key, decrypt_iv);
    ASSERT_NE(client_decryptor, nullptr);

    std::unique_ptr<MessageEncryptor> host_encryptor =
        MessageEncryptor::createForChaCha20Poly1305(key, decrypt_iv);
    ASSERT_NE(host_encryptor, nullptr);

    std::unique_ptr<MessageDecryptor> host_decryptor =
        MessageDecryptor::createForChaCha20Poly1305(key, encrypt_iv);
    ASSERT_NE(host_encryptor, nullptr);

    for (int i = 0; i < 100; ++i)
    {
        testVector(client_encryptor.get(), client_decryptor.get(),
                   host_encryptor.get(), host_decryptor.get());
    }
}

TEST(CryptorChaCha20Poly1305Test, WrongKey)
{
    const QByteArray client_key =
        QByteArray::fromHex("5ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");
    const QByteArray host_key =
        QByteArray::fromHex("1ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");
    const QByteArray iv = QByteArray::fromHex("ee7eb0e6fb24d445597f3e6f");

    EXPECT_EQ(client_key.size(), 32);
    EXPECT_EQ(iv.size(), 12);

    std::unique_ptr<MessageEncryptor> client_encryptor =
        MessageEncryptor::createForAes256Gcm(client_key, iv);
    ASSERT_NE(client_encryptor, nullptr);

    std::unique_ptr<MessageDecryptor> host_decryptor =
        MessageDecryptor::createForAes256Gcm(host_key, iv);
    ASSERT_NE(host_decryptor, nullptr);

    wrongKey(client_encryptor.get(), host_decryptor.get());
}

} // namespace base
