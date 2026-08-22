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

#include "base/crypto/os_crypt.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QString>

TEST(OSCryptTest, BytesRoundTrip)
{
    const QByteArray plaintext("the bytes the wrap must return intact");

    QByteArray ciphertext;
    ASSERT_TRUE(OSCrypt::encryptBytes(plaintext, &ciphertext));
    EXPECT_NE(ciphertext, plaintext);

    QByteArray opened;
    ASSERT_TRUE(OSCrypt::decryptBytes(ciphertext, &opened));
    EXPECT_EQ(opened, plaintext);
}

TEST(OSCryptTest, StringRoundTrip)
{
    const QString plaintext = QString::fromUtf8("caf\xC3\xA9 \xE2\x82\xAC");

    QByteArray ciphertext;
    ASSERT_TRUE(OSCrypt::encryptString(plaintext, &ciphertext));

    QString opened;
    ASSERT_TRUE(OSCrypt::decryptString(ciphertext, &opened));
    EXPECT_EQ(opened, plaintext);
}

// Empty input is a stored "nothing", not a failure, and it must come back as nothing.
TEST(OSCryptTest, EmptyInputStaysEmpty)
{
    QByteArray ciphertext("stale");
    ASSERT_TRUE(OSCrypt::encryptBytes(QByteArray(), &ciphertext));
    EXPECT_TRUE(ciphertext.isEmpty());

    QByteArray plaintext("stale");
    ASSERT_TRUE(OSCrypt::decryptBytes(QByteArray(), &plaintext));
    EXPECT_TRUE(plaintext.isEmpty());
}

// Every seal is randomized, so equal plaintexts must not produce recognizable equal blobs.
TEST(OSCryptTest, TwoSealsOfTheSameBytesDiffer)
{
    const QByteArray plaintext("the bytes under the seal");

    QByteArray first;
    QByteArray second;
    ASSERT_TRUE(OSCrypt::encryptBytes(plaintext, &first));
    ASSERT_TRUE(OSCrypt::encryptBytes(plaintext, &second));

    EXPECT_NE(first, second);
}

TEST(OSCryptTest, TamperedBlobIsRefused)
{
    QByteArray ciphertext;
    ASSERT_TRUE(OSCrypt::encryptBytes(QByteArray("the bytes under the seal"), &ciphertext));
    ASSERT_FALSE(ciphertext.isEmpty());

    const qsizetype pos = ciphertext.size() / 2;
    ciphertext[pos] = static_cast<char>(~ciphertext.at(pos));

    QByteArray opened;
    EXPECT_FALSE(OSCrypt::decryptBytes(ciphertext, &opened));
}

TEST(OSCryptTest, GarbageIsRefused)
{
    QByteArray opened;
    EXPECT_FALSE(OSCrypt::decryptBytes(QByteArray("not a sealed blob"), &opened));
}
