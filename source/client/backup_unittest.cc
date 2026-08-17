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

#include "client/backup.h"

#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "client/database.h"
#include "proto/storage.h"

class BackupTest : public testing::Test
{
protected:
    void SetUp() override
    {
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

        ASSERT_TRUE(dir_.isValid());
        ASSERT_TRUE(source_.open(dir_.filePath("source.db3")));
        ASSERT_TRUE(target_.open(dir_.filePath("target.db3")));
    }

    static SecureString password() { return SecureString(QString("Password123")); }

    QString backupPath() const { return dir_.filePath("book.aspia-backup"); }

    static qint64 addGroup(Database& db, const QString& name, qint64 parent_id)
    {
        LocalGroupConfig group;
        group.setName(name);
        group.setParentId(parent_id);

        EXPECT_TRUE(db.addGroup(group));
        return group.id();
    }

    static qint64 addHost(Database& db, const QString& name, qint64 group_id)
    {
        LocalHostConfig host;
        host.setName(name);
        host.setAddress("192.168.0.1");
        host.setGroupId(group_id);

        EXPECT_TRUE(db.addHost(host));
        return host.id();
    }

    static QStringList groupNames(Database& db)
    {
        QStringList names;
        for (const LocalGroupConfig& group : db.allGroups())
            names.append(group.name());
        names.sort();
        return names;
    }

    // The record of the group with this name, whatever id it was given on the way in.
    static std::optional<LocalGroupConfig> groupByName(Database& db, const QString& name)
    {
        for (const LocalGroupConfig& group : db.allGroups())
        {
            if (group.name() == name)
                return group;
        }

        return std::nullopt;
    }

    // Hands the import a book with a link that names a group the file does not carry. The whole
    // address book is sealed, so getting at that link means opening the file with the password of
    // the backup, the same way the import does.
    void repointGroupParent(qint64 group_id, qint64 new_parent_id)
    {
        proto::storage::BackupFile file_message;

        {
            QFile file(backupPath());
            ASSERT_TRUE(file.open(QIODevice::ReadOnly));
            ASSERT_TRUE(parse(file.readAll(), &file_message));
        }

        SecureByteArray key(PasswordHash::hash(
            PasswordHash::ARGON2ID, password(),
            QByteArray::fromStdString(file_message.salt())));
        DataCryptor cryptor(CipherType::AES256_GCM, key);

        std::optional<QByteArray> decrypted =
            cryptor.decrypt(QByteArray::fromStdString(file_message.data()));
        ASSERT_TRUE(decrypted.has_value());

        proto::storage::BackupFile::Content data;
        ASSERT_TRUE(parse(*decrypted, &data));

        bool found = false;
        for (proto::storage::BackupFile::Group& group : *data.mutable_groups())
        {
            if (group.id() != group_id)
                continue;

            group.set_parent_id(new_parent_id);
            found = true;
        }
        ASSERT_TRUE(found);

        std::optional<QByteArray> sealed = cryptor.encrypt(serialize(data));
        ASSERT_TRUE(sealed.has_value());

        file_message.set_data(sealed->toStdString());

        QFile file(backupPath());
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(serialize(file_message));
    }

    Database source_;
    Database target_;

private:
    QTemporaryDir dir_;
};

//--------------------------------------------------------------------------------------------------
// What was exported is what comes back, with the tree it was written in.
TEST_F(BackupTest, ExportedBookIsImportedBackWithItsTree)
{
    const qint64 parent = addGroup(source_, "parent", 0);
    const qint64 child = addGroup(source_, "child", parent);
    addHost(source_, "host", child);

    ASSERT_EQ(Backup::exportToFile(source_, backupPath(), password()),
              Backup::Result::SUCCESS);

    Backup::ImportCounts counts;
    ASSERT_EQ(Backup::importFromFile(target_, backupPath(), password(), &counts),
              Backup::Result::SUCCESS);

    EXPECT_EQ(groupNames(target_), QStringList({ "child", "parent" }));
    EXPECT_EQ(counts.groups, 2);
    EXPECT_EQ(counts.hosts, 1);

    const std::optional<LocalGroupConfig> new_parent = groupByName(target_, "parent");
    const std::optional<LocalGroupConfig> new_child = groupByName(target_, "child");
    ASSERT_TRUE(new_parent.has_value() && new_child.has_value());

    EXPECT_EQ(new_parent->parentId(), 0);
    EXPECT_EQ(new_child->parentId(), new_parent->id());
    EXPECT_EQ(target_.hostList(new_child->id()).size(), 1);
}

//--------------------------------------------------------------------------------------------------
// A host whose group the file does not carry is kept, at the root. A group whose parent the file
// does not carry has to be kept the same way: dropping it takes everything below it as well, and
// the tally the user is shown says nothing about it.
TEST_F(BackupTest, GroupWhoseParentIsMissingFromTheFileGoesToTheRoot)
{
    const qint64 parent = addGroup(source_, "parent", 0);
    const qint64 child = addGroup(source_, "child", parent);
    addHost(source_, "host", child);

    ASSERT_EQ(Backup::exportToFile(source_, backupPath(), password()),
              Backup::Result::SUCCESS);

    repointGroupParent(parent, 99999);

    Backup::ImportCounts counts;
    ASSERT_EQ(Backup::importFromFile(target_, backupPath(), password(), &counts),
              Backup::Result::SUCCESS);

    EXPECT_EQ(groupNames(target_), QStringList({ "child", "parent" }));
    EXPECT_EQ(counts.groups, 2);

    const std::optional<LocalGroupConfig> new_parent = groupByName(target_, "parent");
    const std::optional<LocalGroupConfig> new_child = groupByName(target_, "child");
    ASSERT_TRUE(new_parent.has_value() && new_child.has_value());

    // The link that was there is kept; only the one the file could not name is replaced by the root.
    EXPECT_EQ(new_parent->parentId(), 0);
    EXPECT_EQ(new_child->parentId(), new_parent->id());
    EXPECT_EQ(target_.hostList(new_child->id()).size(), 1);
}

//--------------------------------------------------------------------------------------------------
// Groups that name each other cannot be a tree, so they are not imported - and the tally says so
// instead of leaving the user to count the rows.
TEST_F(BackupTest, GroupsThatNameEachOtherAreCountedAsSkipped)
{
    const qint64 first = addGroup(source_, "first", 0);
    const qint64 second = addGroup(source_, "second", first);

    ASSERT_EQ(Backup::exportToFile(source_, backupPath(), password()),
              Backup::Result::SUCCESS);

    repointGroupParent(first, second);

    Backup::ImportCounts counts;
    EXPECT_EQ(Backup::importFromFile(target_, backupPath(), password(), &counts),
              Backup::Result::NOTHING_IMPORTED);

    EXPECT_EQ(counts.groups, 0);
    EXPECT_EQ(counts.groups_skipped, 2);
}

//--------------------------------------------------------------------------------------------------
// The password is what the file is locked with, and nothing is written without it.
TEST_F(BackupTest, BookIsNotImportedWithAnotherPassword)
{
    addGroup(source_, "group", 0);

    ASSERT_EQ(Backup::exportToFile(source_, backupPath(), password()),
              Backup::Result::SUCCESS);

    EXPECT_EQ(Backup::importFromFile(target_, backupPath(), SecureString(QString("Other123"))),
              Backup::Result::WRONG_PASSWORD);

    EXPECT_TRUE(groupNames(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// What the format is for: the book is sealed whole. Neither a name the user gave a record nor the
// address it points at is anywhere in the bytes of the file.
TEST_F(BackupTest, NothingOfTheBookIsReadableInTheFile)
{
    const qint64 group = addGroup(source_, "accounting-department", 0);
    addHost(source_, "prod-database-server", group);

    ASSERT_EQ(Backup::exportToFile(source_, backupPath(), password()),
              Backup::Result::SUCCESS);

    QFile file(backupPath());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));

    const QByteArray bytes = file.readAll();
    ASSERT_FALSE(bytes.isEmpty());

    EXPECT_FALSE(bytes.contains("accounting-department"));
    EXPECT_FALSE(bytes.contains("prod-database-server"));
    EXPECT_FALSE(bytes.contains("192.168.0.1"));
}
