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

#include "base/crypto/datagram_encryptor.h"
#include "base/crypto/datagram_decryptor.h"

#include <gtest/gtest.h>

namespace base {

namespace {

const QByteArray kKey =
    QByteArray::fromHex("5ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");
const QByteArray kIV = QByteArray::fromHex("ee7eb0e6fb24d445597f3e6f");
const QByteArray kMessage = QByteArray::fromHex(
    "6006ee8029610876ec2facd5fc9ce6bd6dc03d4a5ddb4d6c28f2ff048d4f7eb7bcf5048c901a4adaa7fd");

} // namespace

TEST(DatagramCryptorAes256GcmTest, EncryptDecrypt)
{
    auto encryptor = DatagramEncryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(decryptor, nullptr);

    QByteArray encrypted;
    encrypted.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    ASSERT_EQ(encrypted.size(), kMessage.size() + 16);

    ASSERT_TRUE(encryptor->encrypt(1, kMessage.data(), kMessage.size(), encrypted.data()));

    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(encrypted.size())));
    ASSERT_EQ(decrypted.size(), kMessage.size());

    ASSERT_TRUE(decryptor->decrypt(1, encrypted.data(), encrypted.size(), decrypted.data()));
    EXPECT_EQ(decrypted, kMessage);
}

TEST(DatagramCryptorAes256GcmTest, OutOfOrderDecrypt)
{
    auto encryptor = DatagramEncryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(decryptor, nullptr);

    // Encrypt three packets with counters 1, 2, 3.
    QByteArray enc1, enc2, enc3;
    enc1.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    enc2.resize(enc1.size());
    enc3.resize(enc1.size());

    ASSERT_TRUE(encryptor->encrypt(1, kMessage.data(), kMessage.size(), enc1.data()));
    ASSERT_TRUE(encryptor->encrypt(2, kMessage.data(), kMessage.size(), enc2.data()));
    ASSERT_TRUE(encryptor->encrypt(3, kMessage.data(), kMessage.size(), enc3.data()));

    // Decrypt out of order: 3, 1, 2.
    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(enc3.size())));

    ASSERT_TRUE(decryptor->decrypt(3, enc3.data(), enc3.size(), decrypted.data()));
    EXPECT_EQ(decrypted, kMessage);

    ASSERT_TRUE(decryptor->decrypt(1, enc1.data(), enc1.size(), decrypted.data()));
    EXPECT_EQ(decrypted, kMessage);

    ASSERT_TRUE(decryptor->decrypt(2, enc2.data(), enc2.size(), decrypted.data()));
    EXPECT_EQ(decrypted, kMessage);
}

TEST(DatagramCryptorAes256GcmTest, WrongCounterFails)
{
    auto encryptor = DatagramEncryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(decryptor, nullptr);

    QByteArray encrypted;
    encrypted.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    ASSERT_TRUE(encryptor->encrypt(1, kMessage.data(), kMessage.size(), encrypted.data()));

    // Decrypt with wrong counter - should fail (AEAD tag mismatch).
    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(encrypted.size())));
    EXPECT_FALSE(decryptor->decrypt(2, encrypted.data(), encrypted.size(), decrypted.data()));
}

TEST(DatagramCryptorAes256GcmTest, WrongKeyFails)
{
    const QByteArray other_key =
        QByteArray::fromHex("1ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");

    auto encryptor = DatagramEncryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForAes256Gcm(other_key, kIV);
    ASSERT_NE(decryptor, nullptr);

    QByteArray encrypted;
    encrypted.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    ASSERT_TRUE(encryptor->encrypt(1, kMessage.data(), kMessage.size(), encrypted.data()));

    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(encrypted.size())));
    EXPECT_FALSE(decryptor->decrypt(1, encrypted.data(), encrypted.size(), decrypted.data()));
}

TEST(DatagramCryptorAes256GcmTest, WithAad)
{
    auto encryptor = DatagramEncryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(decryptor, nullptr);

    const QByteArray aad = QByteArray::fromHex("0307000000000040");

    QByteArray encrypted;
    encrypted.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    ASSERT_TRUE(encryptor->encrypt(1, kMessage.data(), kMessage.size(),
                                   aad.data(), aad.size(), encrypted.data()));

    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(encrypted.size())));
    ASSERT_TRUE(decryptor->decrypt(1, encrypted.data(), encrypted.size(),
                                   aad.data(), aad.size(), decrypted.data()));
    EXPECT_EQ(decrypted, kMessage);
}

TEST(DatagramCryptorAes256GcmTest, WrongAadFails)
{
    auto encryptor = DatagramEncryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(decryptor, nullptr);

    const QByteArray aad = QByteArray::fromHex("0307000000000040");
    QByteArray wrong_aad = aad;
    wrong_aad[1] = '\x08';

    QByteArray encrypted;
    encrypted.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    ASSERT_TRUE(encryptor->encrypt(1, kMessage.data(), kMessage.size(),
                                   aad.data(), aad.size(), encrypted.data()));

    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(encrypted.size())));
    EXPECT_FALSE(decryptor->decrypt(1, encrypted.data(), encrypted.size(),
                                    wrong_aad.data(), wrong_aad.size(), decrypted.data()));
}

TEST(DatagramCryptorAes256GcmTest, SameCounterProducesSameCiphertext)
{
    auto encryptor = DatagramEncryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    QByteArray enc1, enc2;
    enc1.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    enc2.resize(enc1.size());

    ASSERT_TRUE(encryptor->encrypt(42, kMessage.data(), kMessage.size(), enc1.data()));
    ASSERT_TRUE(encryptor->encrypt(42, kMessage.data(), kMessage.size(), enc2.data()));

    // Same counter + same key + same IV + same plaintext = same ciphertext.
    EXPECT_EQ(enc1, enc2);
}

TEST(DatagramCryptorAes256GcmTest, DifferentCounterProducesDifferentCiphertext)
{
    auto encryptor = DatagramEncryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    QByteArray enc1, enc2;
    enc1.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    enc2.resize(enc1.size());

    ASSERT_TRUE(encryptor->encrypt(1, kMessage.data(), kMessage.size(), enc1.data()));
    ASSERT_TRUE(encryptor->encrypt(2, kMessage.data(), kMessage.size(), enc2.data()));

    EXPECT_NE(enc1, enc2);
}

TEST(DatagramCryptorAes256GcmTest, ManyCounters)
{
    auto encryptor = DatagramEncryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(decryptor, nullptr);

    QByteArray encrypted;
    encrypted.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));

    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(encrypted.size())));

    for (quint64 i = 1; i <= 1000; ++i)
    {
        ASSERT_TRUE(encryptor->encrypt(i, kMessage.data(), kMessage.size(), encrypted.data()));
        ASSERT_TRUE(decryptor->decrypt(i, encrypted.data(), encrypted.size(), decrypted.data()));
        ASSERT_EQ(decrypted, kMessage) << "Failed at counter " << i;
    }
}

TEST(DatagramCryptorAes256GcmTest, TamperedCiphertextFails)
{
    auto encryptor = DatagramEncryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForAes256Gcm(kKey, kIV);
    ASSERT_NE(decryptor, nullptr);

    QByteArray encrypted;
    encrypted.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    ASSERT_TRUE(encryptor->encrypt(1, kMessage.data(), kMessage.size(), encrypted.data()));

    // Flip a bit in ciphertext (after the 16-byte tag).
    encrypted[16] = static_cast<char>(encrypted[16] ^ 0x01);

    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(encrypted.size())));
    EXPECT_FALSE(decryptor->decrypt(1, encrypted.data(), encrypted.size(), decrypted.data()));
}

// ChaCha20-Poly1305 tests.

TEST(DatagramCryptorChaCha20Poly1305Test, EncryptDecrypt)
{
    auto encryptor = DatagramEncryptor::createForChaCha20Poly1305(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForChaCha20Poly1305(kKey, kIV);
    ASSERT_NE(decryptor, nullptr);

    QByteArray encrypted;
    encrypted.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    ASSERT_TRUE(encryptor->encrypt(1, kMessage.data(), kMessage.size(), encrypted.data()));

    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(encrypted.size())));
    ASSERT_TRUE(decryptor->decrypt(1, encrypted.data(), encrypted.size(), decrypted.data()));
    EXPECT_EQ(decrypted, kMessage);
}

TEST(DatagramCryptorChaCha20Poly1305Test, OutOfOrderDecrypt)
{
    auto encryptor = DatagramEncryptor::createForChaCha20Poly1305(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForChaCha20Poly1305(kKey, kIV);
    ASSERT_NE(decryptor, nullptr);

    QByteArray enc1, enc2, enc3;
    enc1.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    enc2.resize(enc1.size());
    enc3.resize(enc1.size());

    ASSERT_TRUE(encryptor->encrypt(1, kMessage.data(), kMessage.size(), enc1.data()));
    ASSERT_TRUE(encryptor->encrypt(2, kMessage.data(), kMessage.size(), enc2.data()));
    ASSERT_TRUE(encryptor->encrypt(3, kMessage.data(), kMessage.size(), enc3.data()));

    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(enc3.size())));

    ASSERT_TRUE(decryptor->decrypt(3, enc3.data(), enc3.size(), decrypted.data()));
    EXPECT_EQ(decrypted, kMessage);

    ASSERT_TRUE(decryptor->decrypt(1, enc1.data(), enc1.size(), decrypted.data()));
    EXPECT_EQ(decrypted, kMessage);

    ASSERT_TRUE(decryptor->decrypt(2, enc2.data(), enc2.size(), decrypted.data()));
    EXPECT_EQ(decrypted, kMessage);
}

TEST(DatagramCryptorChaCha20Poly1305Test, WrongCounterFails)
{
    auto encryptor = DatagramEncryptor::createForChaCha20Poly1305(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForChaCha20Poly1305(kKey, kIV);
    ASSERT_NE(decryptor, nullptr);

    QByteArray encrypted;
    encrypted.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    ASSERT_TRUE(encryptor->encrypt(1, kMessage.data(), kMessage.size(), encrypted.data()));

    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(encrypted.size())));
    EXPECT_FALSE(decryptor->decrypt(2, encrypted.data(), encrypted.size(), decrypted.data()));
}

TEST(DatagramCryptorChaCha20Poly1305Test, WrongKeyFails)
{
    const QByteArray other_key =
        QByteArray::fromHex("1ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");

    auto encryptor = DatagramEncryptor::createForChaCha20Poly1305(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForChaCha20Poly1305(other_key, kIV);
    ASSERT_NE(decryptor, nullptr);

    QByteArray encrypted;
    encrypted.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    ASSERT_TRUE(encryptor->encrypt(1, kMessage.data(), kMessage.size(), encrypted.data()));

    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(encrypted.size())));
    EXPECT_FALSE(decryptor->decrypt(1, encrypted.data(), encrypted.size(), decrypted.data()));
}

TEST(DatagramCryptorChaCha20Poly1305Test, WithAad)
{
    auto encryptor = DatagramEncryptor::createForChaCha20Poly1305(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForChaCha20Poly1305(kKey, kIV);
    ASSERT_NE(decryptor, nullptr);

    const QByteArray aad = QByteArray::fromHex("0307000000000040");

    QByteArray encrypted;
    encrypted.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    ASSERT_TRUE(encryptor->encrypt(1, kMessage.data(), kMessage.size(),
                                   aad.data(), aad.size(), encrypted.data()));

    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(encrypted.size())));
    ASSERT_TRUE(decryptor->decrypt(1, encrypted.data(), encrypted.size(),
                                   aad.data(), aad.size(), decrypted.data()));
    EXPECT_EQ(decrypted, kMessage);
}

TEST(DatagramCryptorChaCha20Poly1305Test, ManyCounters)
{
    auto encryptor = DatagramEncryptor::createForChaCha20Poly1305(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForChaCha20Poly1305(kKey, kIV);
    ASSERT_NE(decryptor, nullptr);

    QByteArray encrypted;
    encrypted.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));

    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(encrypted.size())));

    for (quint64 i = 1; i <= 1000; ++i)
    {
        ASSERT_TRUE(encryptor->encrypt(i, kMessage.data(), kMessage.size(), encrypted.data()));
        ASSERT_TRUE(decryptor->decrypt(i, encrypted.data(), encrypted.size(), decrypted.data()));
        ASSERT_EQ(decrypted, kMessage) << "Failed at counter " << i;
    }
}

TEST(DatagramCryptorChaCha20Poly1305Test, TamperedCiphertextFails)
{
    auto encryptor = DatagramEncryptor::createForChaCha20Poly1305(kKey, kIV);
    ASSERT_NE(encryptor, nullptr);

    auto decryptor = DatagramDecryptor::createForChaCha20Poly1305(kKey, kIV);
    ASSERT_NE(decryptor, nullptr);

    QByteArray encrypted;
    encrypted.resize(static_cast<qsizetype>(encryptor->encryptedDataSize(kMessage.size())));
    ASSERT_TRUE(encryptor->encrypt(1, kMessage.data(), kMessage.size(), encrypted.data()));

    encrypted[16] = static_cast<char>(encrypted[16] ^ 0x01);

    QByteArray decrypted;
    decrypted.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(encrypted.size())));
    EXPECT_FALSE(decryptor->decrypt(1, encrypted.data(), encrypted.size(), decrypted.data()));
}

// Factory validation tests.

TEST(DatagramCryptorTest, InvalidKeySize)
{
    const QByteArray short_key = QByteArray::fromHex("5ce26794165a808ec425684e9384c27c");
    EXPECT_EQ(DatagramEncryptor::createForAes256Gcm(short_key, kIV), nullptr);
    EXPECT_EQ(DatagramDecryptor::createForAes256Gcm(short_key, kIV), nullptr);
    EXPECT_EQ(DatagramEncryptor::createForChaCha20Poly1305(short_key, kIV), nullptr);
    EXPECT_EQ(DatagramDecryptor::createForChaCha20Poly1305(short_key, kIV), nullptr);
}

TEST(DatagramCryptorTest, InvalidIvSize)
{
    const QByteArray short_iv = QByteArray::fromHex("ee7eb0e6fb24d445");
    EXPECT_EQ(DatagramEncryptor::createForAes256Gcm(kKey, short_iv), nullptr);
    EXPECT_EQ(DatagramDecryptor::createForAes256Gcm(kKey, short_iv), nullptr);
    EXPECT_EQ(DatagramEncryptor::createForChaCha20Poly1305(kKey, short_iv), nullptr);
    EXPECT_EQ(DatagramDecryptor::createForChaCha20Poly1305(kKey, short_iv), nullptr);
}

} // namespace base
