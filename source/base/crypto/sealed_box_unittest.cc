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

#include "base/crypto/sealed_box.h"

#include "base/crypto/key_pair.h"

#include <gtest/gtest.h>

#include <QByteArray>

TEST(SealedBoxTest, RoundTrip)
{
    KeyPair recipient = KeyPair::create(KeyPair::Type::X25519);
    ASSERT_TRUE(recipient.isValid());

    const QByteArray message = QByteArray::fromHex(
        "5ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014");

    QByteArray sealed = SealedBox::seal(message, recipient.publicKey());
    ASSERT_FALSE(sealed.isEmpty());
    ASSERT_GE(sealed.size(), SealedBox::kMinSealedSize);
    ASSERT_EQ(sealed.size(), SealedBox::kPublicKeySize + message.size() + 12 + 16);

    std::optional<QByteArray> opened = SealedBox::open(sealed, recipient);
    ASSERT_TRUE(opened.has_value());
    ASSERT_EQ(*opened, message);
}

TEST(SealedBoxTest, WrongRecipientKey)
{
    KeyPair recipient = KeyPair::create(KeyPair::Type::X25519);
    KeyPair attacker = KeyPair::create(KeyPair::Type::X25519);
    ASSERT_TRUE(recipient.isValid());
    ASSERT_TRUE(attacker.isValid());

    const QByteArray message = QByteArray::fromHex("deadbeefcafe");

    QByteArray sealed = SealedBox::seal(message, recipient.publicKey());
    ASSERT_FALSE(sealed.isEmpty());

    std::optional<QByteArray> opened = SealedBox::open(sealed, attacker);
    ASSERT_FALSE(opened.has_value());
}

TEST(SealedBoxTest, TamperedCiphertext)
{
    KeyPair recipient = KeyPair::create(KeyPair::Type::X25519);
    ASSERT_TRUE(recipient.isValid());

    const QByteArray message = QByteArray::fromHex("0123456789abcdef0123456789abcdef");

    QByteArray sealed = SealedBox::seal(message, recipient.publicKey());
    ASSERT_FALSE(sealed.isEmpty());

    // Flip a byte inside ciphertext (after ephemeral public key).
    sealed[SealedBox::kPublicKeySize + 12] ^= 0x01;

    std::optional<QByteArray> opened = SealedBox::open(sealed, recipient);
    ASSERT_FALSE(opened.has_value());
}

TEST(SealedBoxTest, TamperedEphemeralKey)
{
    KeyPair recipient = KeyPair::create(KeyPair::Type::X25519);
    ASSERT_TRUE(recipient.isValid());

    const QByteArray message = QByteArray::fromHex("0123456789abcdef0123456789abcdef");

    QByteArray sealed = SealedBox::seal(message, recipient.publicKey());
    ASSERT_FALSE(sealed.isEmpty());

    // Flip a byte inside ephemeral public key.
    sealed[0] ^= 0x01;

    std::optional<QByteArray> opened = SealedBox::open(sealed, recipient);
    ASSERT_FALSE(opened.has_value());
}

TEST(SealedBoxTest, TooSmall)
{
    KeyPair recipient = KeyPair::create(KeyPair::Type::X25519);
    ASSERT_TRUE(recipient.isValid());

    const QByteArray short_sealed(SealedBox::kMinSealedSize - 1, 0);
    std::optional<QByteArray> opened = SealedBox::open(short_sealed, recipient);
    ASSERT_FALSE(opened.has_value());
}

TEST(SealedBoxTest, InvalidRecipientPublicKey)
{
    const QByteArray wrong_size_key(16, 0);
    const QByteArray message = QByteArray::fromHex("deadbeef");

    QByteArray sealed = SealedBox::seal(message, wrong_size_key);
    ASSERT_TRUE(sealed.isEmpty());
}

TEST(SealedBoxTest, NonDeterministic)
{
    KeyPair recipient = KeyPair::create(KeyPair::Type::X25519);
    ASSERT_TRUE(recipient.isValid());

    const QByteArray message = QByteArray::fromHex("deadbeefcafebabe");

    QByteArray sealed1 = SealedBox::seal(message, recipient.publicKey());
    QByteArray sealed2 = SealedBox::seal(message, recipient.publicKey());
    ASSERT_FALSE(sealed1.isEmpty());
    ASSERT_FALSE(sealed2.isEmpty());
    ASSERT_NE(sealed1, sealed2);

    std::optional<QByteArray> opened1 = SealedBox::open(sealed1, recipient);
    std::optional<QByteArray> opened2 = SealedBox::open(sealed2, recipient);
    ASSERT_TRUE(opened1.has_value());
    ASSERT_TRUE(opened2.has_value());
    ASSERT_EQ(*opened1, message);
    ASSERT_EQ(*opened2, message);
}

TEST(SealedBoxTest, EmptyPlaintext)
{
    KeyPair recipient = KeyPair::create(KeyPair::Type::X25519);
    ASSERT_TRUE(recipient.isValid());

    QByteArray sealed = SealedBox::seal(QByteArray(), recipient.publicKey());
    // Empty input is rejected by DataCryptor::encrypt.
    ASSERT_TRUE(sealed.isEmpty());
}
