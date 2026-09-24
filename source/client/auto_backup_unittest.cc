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

#include "client/auto_backup.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "client/database.h"

class AutoBackupTest : public testing::Test
{
protected:
    void SetUp() override
    {
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

        ASSERT_TRUE(dir_.isValid());
        ASSERT_TRUE(db_.open(dir_.filePath("book.db3")));

        // A backup is sealed with the key of the database, so the database needs a master password.
        const QByteArray salt = Random::byteArray(32);
        const SecureByteArray key(
            PasswordHash::hash(PasswordHash::ARGON2ID, SecureString(QString("Password123")), salt));

        const std::optional<QByteArray> verifier =
            DataCryptor(CipherType::AES256_GCM, key).encrypt(Random::byteArray(32));
        ASSERT_TRUE(verifier.has_value());
        ASSERT_TRUE(db_.reencryptAll({}, {}, {}, {}, salt, *verifier, 1));

        DataCryptor::instance().setKey(key);
    }

    void addGroup()
    {
        LocalGroupConfig group;
        group.setName("Group");
        ASSERT_TRUE(db_.addLocalGroup(group));
    }

    QString backupDir() const { return dir_.filePath("backups"); }

    // The name a backup made |days| ago carries.
    static QString nameOfDaysAgo(int days)
    {
        return "aspia-backup-" + QDateTime::currentDateTime().addDays(-days).toString("yyyy-MM-dd-HHmmss") +
               ".aspia-backup";
    }

    void createFile(const QString& name)
    {
        ASSERT_TRUE(QDir().mkpath(backupDir()));

        QFile file(QDir(backupDir()).filePath(name));
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        ASSERT_EQ(file.write("data"), 4);
    }

    QStringList files() const
    {
        return QDir(backupDir()).entryList(QDir::Files, QDir::Name);
    }

    QTemporaryDir dir_;
    Database db_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(AutoBackupTest, WritesABackupIntoANewDirectory)
{
    addGroup();

    ASSERT_EQ(AutoBackup::run(db_, backupDir(), AutoBackup::Retention::ONE_MONTH),
              Backup::Result::SUCCESS);

    const QStringList names = files();
    ASSERT_EQ(names.size(), 1);
    EXPECT_TRUE(names.front().startsWith("aspia-backup-"));
    EXPECT_TRUE(names.front().endsWith(".aspia-backup"));

    Database restored;
    ASSERT_TRUE(restored.open(dir_.filePath("restored.db3")));

    Backup::Report report;
    ASSERT_EQ(Backup::importFromFile(restored, QDir(backupDir()).filePath(names.front()), SecureString(),
                                     &report),
              Backup::Result::SUCCESS);
    EXPECT_EQ(report.local_groups, 1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(AutoBackupTest, RemovesBackupsOlderThanTheRetention)
{
    addGroup();

    const QString expired = nameOfDaysAgo(10);
    const QString kept = nameOfDaysAgo(3);
    createFile(expired);
    createFile(kept);

    ASSERT_EQ(AutoBackup::run(db_, backupDir(), AutoBackup::Retention::ONE_WEEK), Backup::Result::SUCCESS);

    const QStringList names = files();
    EXPECT_EQ(names.size(), 2);
    EXPECT_FALSE(names.contains(expired));
    EXPECT_TRUE(names.contains(kept));
}

//--------------------------------------------------------------------------------------------------
TEST_F(AutoBackupTest, LongerRetentionKeepsTheSameBackup)
{
    addGroup();

    const QString backup = nameOfDaysAgo(10);
    createFile(backup);

    ASSERT_EQ(AutoBackup::run(db_, backupDir(), AutoBackup::Retention::TWO_WEEKS), Backup::Result::SUCCESS);

    EXPECT_EQ(files().size(), 2);
    EXPECT_TRUE(files().contains(backup));
}

//--------------------------------------------------------------------------------------------------
TEST_F(AutoBackupTest, FilesThatAreNotBackupsStay)
{
    addGroup();

    createFile("aspia-backup-notes.aspia-backup");
    createFile("book.aspia-backup");
    createFile("readme.txt");

    ASSERT_EQ(AutoBackup::run(db_, backupDir(), AutoBackup::Retention::ONE_WEEK), Backup::Result::SUCCESS);

    const QStringList names = files();
    EXPECT_EQ(names.size(), 4);
    EXPECT_TRUE(names.contains("aspia-backup-notes.aspia-backup"));
    EXPECT_TRUE(names.contains("book.aspia-backup"));
    EXPECT_TRUE(names.contains("readme.txt"));
}

//--------------------------------------------------------------------------------------------------
TEST_F(AutoBackupTest, FailedBackupRemovesNothing)
{
    // An empty database has nothing to save.
    const QString expired = nameOfDaysAgo(400);
    createFile(expired);

    EXPECT_EQ(AutoBackup::run(db_, backupDir(), AutoBackup::Retention::ONE_WEEK),
              Backup::Result::NOTHING_EXPORTED);

    EXPECT_EQ(files(), QStringList{ expired });
}

//--------------------------------------------------------------------------------------------------
TEST_F(AutoBackupTest, RelativeDirectoryIsRefused)
{
    addGroup();

    EXPECT_EQ(AutoBackup::run(db_, "backups", AutoBackup::Retention::ONE_WEEK), Backup::Result::FILE_ERROR);
    EXPECT_EQ(AutoBackup::run(db_, QString(), AutoBackup::Retention::ONE_WEEK), Backup::Result::FILE_ERROR);
}
