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

#if !defined(Q_OS_WINDOWS) && !defined(Q_OS_APPLE)

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace {

class OSCryptKeyFileTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(dir_.isValid());
        key_path_ = dir_.path() + QStringLiteral("/aspia/os_crypt.key");
        OSCrypt::setKeyFilePathForTesting(key_path_);
    }

    void TearDown() override { OSCrypt::setKeyFilePathForTesting(QString()); }

    QTemporaryDir dir_;
    QString key_path_;
};

} // namespace

// The key is a private random file, not something derived from public machine data.
TEST_F(OSCryptKeyFileTest, KeyFileIsCreatedPrivate)
{
    QByteArray ciphertext;
    ASSERT_TRUE(OSCrypt::encryptBytes(QByteArray("secret"), &ciphertext));

    QFileInfo key(key_path_);
    ASSERT_TRUE(key.exists());
    EXPECT_EQ(key.size(), 32);
    EXPECT_EQ(key.permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther |
                                   QFileDevice::WriteGroup | QFileDevice::WriteOther),
              QFileDevice::Permissions());
    EXPECT_EQ(QFileInfo(key.absolutePath()).permissions() &
                  (QFileDevice::ReadOther | QFileDevice::ExeOther | QFileDevice::ReadGroup),
              QFileDevice::Permissions());

    // Without the key file the seal does not open (it is not the machine-id key).
    ASSERT_TRUE(QFile::remove(key_path_));
    QByteArray opened;
    EXPECT_FALSE(OSCrypt::decryptBytes(ciphertext, &opened));
}

// Seals of older versions (machine-id key) still open and are reported for re-encryption; the
// new seal opens without the report.
TEST_F(OSCryptKeyFileTest, LegacySealMigrates)
{
    QByteArray legacy;
    ASSERT_TRUE(OSCrypt::encryptBytesLegacyForTesting(QByteArray("old secret"), &legacy));

    QByteArray opened;
    bool needs_reencrypt = false;
    ASSERT_TRUE(OSCrypt::decryptBytes(legacy, &opened, &needs_reencrypt));
    EXPECT_EQ(opened, QByteArray("old secret"));
    EXPECT_TRUE(needs_reencrypt);

    QByteArray fresh;
    ASSERT_TRUE(OSCrypt::encryptBytes(opened, &fresh));
    ASSERT_TRUE(OSCrypt::decryptBytes(fresh, &opened, &needs_reencrypt));
    EXPECT_EQ(opened, QByteArray("old secret"));
    EXPECT_FALSE(needs_reencrypt);
}

// A key file readable by others is taken back to 0600; a key file of the wrong size is refused.
TEST_F(OSCryptKeyFileTest, LooseKeyFileIsTightenedAndBadOneRefused)
{
    QByteArray ciphertext;
    ASSERT_TRUE(OSCrypt::encryptBytes(QByteArray("secret"), &ciphertext));
    ASSERT_TRUE(QFile::setPermissions(key_path_, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                                 QFileDevice::ReadOther));

    QByteArray opened;
    ASSERT_TRUE(OSCrypt::decryptBytes(ciphertext, &opened));
    EXPECT_FALSE(QFileInfo(key_path_).permissions() & QFileDevice::ReadOther);

    QFile file(key_path_);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("short");
    file.close();
    EXPECT_FALSE(OSCrypt::encryptBytes(QByteArray("secret"), &ciphertext));
}

#endif
