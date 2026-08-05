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

#include "client/json_backup.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "client/database.h"

class JsonBackupTest : public testing::Test
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

    QString backupPath() const { return dir_.filePath("book.json"); }

    static qint64 addGroup(Database& db, const QString& name, qint64 parent_id)
    {
        GroupConfig group;
        group.setName(name);
        group.setParentId(parent_id);

        EXPECT_TRUE(db.addGroup(group));
        return group.id();
    }

    static qint64 addHost(Database& db, const QString& name, qint64 group_id)
    {
        HostConfig host;
        host.setName(name);
        host.setAddress("192.168.0.1");
        host.setGroupId(group_id);

        EXPECT_TRUE(db.addHost(host));
        return host.id();
    }

    static QStringList groupNames(Database& db)
    {
        QStringList names;
        for (const GroupConfig& group : db.allGroups())
            names.append(group.name());
        names.sort();
        return names;
    }

    // The record of the group with this name, whatever id it was given on the way in.
    static std::optional<GroupConfig> groupByName(Database& db, const QString& name)
    {
        for (const GroupConfig& group : db.allGroups())
        {
            if (group.name() == name)
                return group;
        }

        return std::nullopt;
    }

    // The ids of the file are in the clear, so a book can be handed to the import with a link that
    // names a group the file does not carry.
    void repointGroupParent(qint64 group_id, qint64 new_parent_id)
    {
        QFile file(backupPath());
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();

        QJsonArray groups = root.value("groups").toArray();
        bool found = false;

        for (QJsonValueRef value : groups)
        {
            QJsonObject group = value.toObject();
            if (group.value("id").toInteger() != group_id)
                continue;

            group.insert("parent_id", new_parent_id);
            value = group;
            found = true;
        }

        ASSERT_TRUE(found);
        root.insert("groups", groups);

        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(root).toJson());
    }

    Database source_;
    Database target_;

private:
    QTemporaryDir dir_;
};

//--------------------------------------------------------------------------------------------------
// What was exported is what comes back, with the tree it was written in.
TEST_F(JsonBackupTest, ExportedBookIsImportedBackWithItsTree)
{
    const qint64 parent = addGroup(source_, "parent", 0);
    const qint64 child = addGroup(source_, "child", parent);
    addHost(source_, "host", child);

    ASSERT_EQ(JsonBackup::exportToFile(source_, backupPath(), password()),
              JsonBackup::Result::SUCCESS);

    JsonBackup::ImportCounts counts;
    ASSERT_EQ(JsonBackup::importFromFile(target_, backupPath(), password(), &counts),
              JsonBackup::Result::SUCCESS);

    EXPECT_EQ(groupNames(target_), QStringList({ "child", "parent" }));
    EXPECT_EQ(counts.groups, 2);
    EXPECT_EQ(counts.hosts, 1);

    const std::optional<GroupConfig> new_parent = groupByName(target_, "parent");
    const std::optional<GroupConfig> new_child = groupByName(target_, "child");
    ASSERT_TRUE(new_parent.has_value() && new_child.has_value());

    EXPECT_EQ(new_parent->parentId(), 0);
    EXPECT_EQ(new_child->parentId(), new_parent->id());
    EXPECT_EQ(target_.hostList(new_child->id()).size(), 1);
}

//--------------------------------------------------------------------------------------------------
// A host whose group the file does not carry is kept, at the root. A group whose parent the file
// does not carry has to be kept the same way: dropping it takes everything below it as well, and
// the tally the user is shown says nothing about it.
TEST_F(JsonBackupTest, GroupWhoseParentIsMissingFromTheFileGoesToTheRoot)
{
    const qint64 parent = addGroup(source_, "parent", 0);
    const qint64 child = addGroup(source_, "child", parent);
    addHost(source_, "host", child);

    ASSERT_EQ(JsonBackup::exportToFile(source_, backupPath(), password()),
              JsonBackup::Result::SUCCESS);

    repointGroupParent(parent, 99999);

    JsonBackup::ImportCounts counts;
    ASSERT_EQ(JsonBackup::importFromFile(target_, backupPath(), password(), &counts),
              JsonBackup::Result::SUCCESS);

    EXPECT_EQ(groupNames(target_), QStringList({ "child", "parent" }));
    EXPECT_EQ(counts.groups, 2);

    const std::optional<GroupConfig> new_parent = groupByName(target_, "parent");
    const std::optional<GroupConfig> new_child = groupByName(target_, "child");
    ASSERT_TRUE(new_parent.has_value() && new_child.has_value());

    // The link that was there is kept; only the one the file could not name is replaced by the root.
    EXPECT_EQ(new_parent->parentId(), 0);
    EXPECT_EQ(new_child->parentId(), new_parent->id());
    EXPECT_EQ(target_.hostList(new_child->id()).size(), 1);
}

//--------------------------------------------------------------------------------------------------
// Groups that name each other cannot be a tree, so they are not imported - and the tally says so
// instead of leaving the user to count the rows.
TEST_F(JsonBackupTest, GroupsThatNameEachOtherAreCountedAsSkipped)
{
    const qint64 first = addGroup(source_, "first", 0);
    const qint64 second = addGroup(source_, "second", first);

    ASSERT_EQ(JsonBackup::exportToFile(source_, backupPath(), password()),
              JsonBackup::Result::SUCCESS);

    repointGroupParent(first, second);

    JsonBackup::ImportCounts counts;
    EXPECT_EQ(JsonBackup::importFromFile(target_, backupPath(), password(), &counts),
              JsonBackup::Result::NOTHING_IMPORTED);

    EXPECT_EQ(counts.groups, 0);
    EXPECT_EQ(counts.groups_skipped, 2);
}

//--------------------------------------------------------------------------------------------------
// The password is what the file is locked with, and nothing is written without it.
TEST_F(JsonBackupTest, BookIsNotImportedWithAnotherPassword)
{
    addGroup(source_, "group", 0);

    ASSERT_EQ(JsonBackup::exportToFile(source_, backupPath(), password()),
              JsonBackup::Result::SUCCESS);

    EXPECT_EQ(JsonBackup::importFromFile(target_, backupPath(), SecureString(QString("Other123"))),
              JsonBackup::Result::WRONG_PASSWORD);

    EXPECT_TRUE(groupNames(target_).isEmpty());
}
