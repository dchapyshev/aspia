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
#include <QUuid>

#include <gtest/gtest.h>

#include <functional>

#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/peer/host_id.h"
#include "base/sql/sql_database.h"
#include "client/database.h"
#include "proto/router.h"
#include "proto/storage.h"

class BackupTest : public testing::Test
{
protected:
    void SetUp() override
    {
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

        ASSERT_TRUE(dir_.isValid());
        ASSERT_TRUE(source_.open(dir_.filePath("source.db3")));
        ASSERT_TRUE(target_.open(targetPath()));

        // The file is sealed with the key of the book it is saved from, so the book has to be one
        // with a master password set. reencryptAll() with nothing to re-encrypt is how a book gets
        // one without going through the singleton MasterPassword talks to.
        const QByteArray salt = Random::byteArray(32);
        const SecureByteArray key(PasswordHash::hash(PasswordHash::ARGON2ID, password(), salt));

        const std::optional<QByteArray> verifier =
            DataCryptor(CipherType::AES256_GCM, key).encrypt(Random::byteArray(32));
        ASSERT_TRUE(verifier.has_value());

        ASSERT_TRUE(source_.reencryptAll({}, {}, {}, salt, *verifier, 1));

        DataCryptor::instance().setKey(key);
    }

    // The master password of the source book, which is what its files are sealed with.
    static SecureString password() { return SecureString(QString("Password123")); }

    QString backupPath() const { return dir_.filePath("book.aspia-backup"); }
    QString targetPath() const { return dir_.filePath("target.db3"); }

    // Runs a statement on the target book behind its back. Taking a table out this way makes the
    // write of an import fail after the records of the file have been counted.
    bool execOnTarget(const char* sql)
    {
        SqlDatabase raw;
        if (!raw.open(targetPath()))
            return false;

        return raw.exec(sql);
    }

    static qint64 addGroup(Database& db, const QString& name, qint64 parent_id)
    {
        LocalGroupConfig group;
        group.setName(name);
        group.setParentId(parent_id);

        EXPECT_TRUE(db.addLocalGroup(group));
        return group.id();
    }

    static qint64 addHost(Database& db, const QString& name, qint64 group_id)
    {
        LocalHostConfig host;
        host.setName(name);
        host.setAddress("192.168.0.1");
        host.setGroupId(group_id);

        EXPECT_TRUE(db.addLocalHost(host));
        return host.id();
    }

    // The guid is not passed in: the database issues one to every router it adds, the same way it
    // does for groups and hosts.
    static qint64 addRouter(Database& db, const QString& name, const QString& address)
    {
        RouterConfig router;
        router.setDisplayName(name);
        router.setAddress(address);
        router.setUsername("router-user");
        router.setPassword(SecureString(QString("router-secret")));

        EXPECT_TRUE(db.addRouter(router));
        return router.routerId();
    }

    static void addRouterHost(Database& db, qint64 router_id, HostId host_id,
                              const QString& username, const QString& password)
    {
        RouterHostConfig host;
        host.setRouterId(router_id);
        host.setHostId(host_id);
        host.setUsername(username);
        host.setPassword(SecureString(password));

        EXPECT_TRUE(db.addRouterHost(host));
    }

    static qint64 addRoutedHost(Database& db, const QString& name, qint64 group_id,
                                qint64 router_id, const QString& address)
    {
        LocalHostConfig host;
        host.setName(name);
        host.setAddress(address);
        host.setGroupId(group_id);
        host.setRouterId(router_id);

        EXPECT_TRUE(db.addLocalHost(host));
        return host.id();
    }

    // The list scans of the fixture: the read is expected to succeed, and the list comes back by
    // value the way the tests consume it.
    static QList<LocalHostConfig> allLocalHosts(Database& db)
    {
        QList<LocalHostConfig> hosts;
        EXPECT_TRUE(db.allLocalHosts(&hosts));
        return hosts;
    }

    static QList<LocalGroupConfig> allLocalGroups(Database& db)
    {
        QList<LocalGroupConfig> groups;
        EXPECT_TRUE(db.allLocalGroups(&groups));
        return groups;
    }

    static QList<RouterConfig> routerList(Database& db)
    {
        QList<RouterConfig> routers;
        EXPECT_TRUE(db.routerList(&routers));
        return routers;
    }

    static QList<RouterHostConfig> allRouterHosts(Database& db)
    {
        QList<RouterHostConfig> hosts;
        EXPECT_TRUE(db.allRouterHosts(&hosts));
        return hosts;
    }

    static QStringList hostNames(Database& db)
    {
        QStringList names;
        for (const LocalHostConfig& host : allLocalHosts(db))
            names.append(host.name());
        names.sort();
        return names;
    }

    Backup::Result exportBook(Backup::Report* report = nullptr)
    {
        return Backup::exportToFile(source_, backupPath(), report);
    }

    // Writes the file into the target database, in place of what it holds. The key of the book
    // opens the file, so no password is handed over.
    Backup::Result importBook(Backup::Report* report = nullptr)
    {
        return Backup::importFromFile(target_, backupPath(), SecureString(), report);
    }

    // The same for a file of another address book.
    Backup::Result importBookWithPassword(const SecureString& file_password)
    {
        return Backup::importFromFile(target_, backupPath(), file_password, nullptr);
    }

    static QStringList groupNames(Database& db)
    {
        QStringList names;
        for (const LocalGroupConfig& group : allLocalGroups(db))
            names.append(group.name());
        names.sort();
        return names;
    }

    // The record of the host with this name, whatever id it was given on the way in.
    static std::optional<LocalHostConfig> hostByName(Database& db, const QString& name)
    {
        for (const LocalHostConfig& host : allLocalHosts(db))
        {
            if (host.name() == name)
                return host;
        }

        return std::nullopt;
    }

    // The record of the group with this name, whatever id it was given on the way in.
    static std::optional<LocalGroupConfig> groupByName(Database& db, const QString& name)
    {
        for (const LocalGroupConfig& group : allLocalGroups(db))
        {
            if (group.name() == name)
                return group;
        }

        return std::nullopt;
    }

    // Opens the file the way the import does, hands the address book to |edit| and seals it back.
    // The tests write with it what the export never produces.
    void editFileContent(const std::function<void(proto::storage::BackupFile::Content*)>& edit)
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

        edit(&data);

        std::optional<QByteArray> sealed = cryptor.encrypt(serialize(data));
        ASSERT_TRUE(sealed.has_value());

        file_message.set_data(sealed->toStdString());

        QFile file(backupPath());
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(serialize(file_message));
    }

    // Seals |payload| into the file in place of the address book it holds.
    void sealPayload(const QByteArray& payload)
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

        std::optional<QByteArray> sealed =
            DataCryptor(CipherType::AES256_GCM, key).encrypt(payload);
        ASSERT_TRUE(sealed.has_value());

        file_message.set_data(sealed->toStdString());

        QFile file(backupPath());
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(serialize(file_message));
    }

    // The guid the source book gave the group, which is how the file names it.
    QString groupGuid(qint64 group_id)
    {
        const std::optional<LocalGroupConfig> group = source_.findLocalGroup(group_id);
        return group.has_value() ? group->guid() : QString();
    }

    // A guid no record of the file carries.
    static QString unknownGuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

    // Hands the import a book with a link that names a group the file does not carry.
    void repointGroupParent(const QString& group_guid, const QString& new_parent_guid)
    {
        editFileContent([&](proto::storage::BackupFile::Content* data)
        {
            bool found = false;
            for (proto::storage::BackupFile::LocalGroup& group : *data->mutable_local_groups())
            {
                if (group.guid() != group_guid.toStdString())
                    continue;

                group.set_parent_guid(new_parent_guid.toStdString());
                found = true;
            }
            EXPECT_TRUE(found);
        });
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

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    Backup::Report report;
    ASSERT_EQ(importBook(&report), Backup::Result::SUCCESS);

    EXPECT_EQ(groupNames(target_), QStringList({ "child", "parent" }));
    EXPECT_EQ(report.local_groups, 2);
    EXPECT_EQ(report.local_hosts, 1);

    const std::optional<LocalGroupConfig> new_parent = groupByName(target_, "parent");
    const std::optional<LocalGroupConfig> new_child = groupByName(target_, "child");
    ASSERT_TRUE(new_parent.has_value() && new_child.has_value());

    EXPECT_EQ(new_parent->parentId(), 0);
    EXPECT_EQ(new_child->parentId(), new_parent->id());
    QList<LocalHostConfig> child_hosts;
    EXPECT_TRUE(target_.localHostList(new_child->id(), &child_hosts));
    EXPECT_EQ(child_hosts.size(), 1);
}

//--------------------------------------------------------------------------------------------------
// The id of a group is good inside one database only, so the file carries the guid as well: the
// group that arrives is the same group the file was made from.
TEST_F(BackupTest, GroupKeepsItsGuidThroughTheFile)
{
    const qint64 group = addGroup(source_, "group", 0);

    const std::optional<LocalGroupConfig> original = source_.findLocalGroup(group);
    ASSERT_TRUE(original.has_value());
    ASSERT_FALSE(original->guid().isEmpty());

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    const std::optional<LocalGroupConfig> imported = groupByName(target_, "group");
    ASSERT_TRUE(imported.has_value());
    EXPECT_EQ(imported->guid(), original->guid());
}

//--------------------------------------------------------------------------------------------------
// Every group the file names is a group the file carries. One naming a parent that is not there is
// not a file this application wrote, and nothing of it is imported.
TEST_F(BackupTest, FileNamingAParentItDoesNotCarryIsNotImported)
{
    const qint64 parent = addGroup(source_, "parent", 0);
    addGroup(source_, "child", parent);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    repointGroupParent(groupGuid(parent), unknownGuid());

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_TRUE(groupNames(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A guid is what names a router across databases, so a router of the file carries one. A record
// without it names nothing, and the hosts of the file reaching that router would arrive reaching
// none.
TEST_F(BackupTest, FileWithARouterWithoutAGuidIsNotImported)
{
    addRouter(source_, "router", "router.example.com");
    addHost(source_, "direct", 0);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->routers_size(), 1);
        data->mutable_routers(0)->clear_guid();
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);

    EXPECT_TRUE(routerList(target_).isEmpty());
    EXPECT_TRUE(allLocalHosts(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A guid names one group in any database, so a file carrying two of them under one guid is not a
// file this application wrote.
TEST_F(BackupTest, FileWithTwoGroupsUnderOneGuidIsNotImported)
{
    addGroup(source_, "first", 0);
    addGroup(source_, "second", 0);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->local_groups_size(), 2);
        data->mutable_local_groups(1)->set_guid(data->local_groups(0).guid());
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_TRUE(groupNames(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Groups that name each other are not a tree, and an address book is one. Such a file is not one
// this application wrote, and nothing of it is imported.
TEST_F(BackupTest, FileWhoseGroupsNameEachOtherIsNotImported)
{
    const qint64 first = addGroup(source_, "first", 0);
    const qint64 second = addGroup(source_, "second", first);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    repointGroupParent(groupGuid(first), groupGuid(second));

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_TRUE(groupNames(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A record of the file carries what a record of the address book carries. One that does not takes
// the whole file with it, because the reader has no way to tell which half of a file to believe.
TEST_F(BackupTest, FileWithARouterWithoutAPasswordIsNotImported)
{
    addRouter(source_, "router", "router.example.com");

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->routers_size(), 1);
        data->mutable_routers(0)->clear_password();
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_TRUE(routerList(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(BackupTest, FileWithARouterOfAnUnknownSessionTypeIsNotImported)
{
    addRouter(source_, "router", "router.example.com");

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->routers_size(), 1);
        data->mutable_routers(0)->set_session_type(proto::router::SESSION_TYPE_HOST);
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_TRUE(routerList(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(BackupTest, FileWithAGroupWithoutANameIsNotImported)
{
    addGroup(source_, "group", 0);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->local_groups_size(), 1);
        data->mutable_local_groups(0)->clear_name();
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_TRUE(groupNames(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The bounds a record is held to are the bounds of the file as well. A name past them would arrive
// cut in half, and half a name is not what the user typed.
TEST_F(BackupTest, FileWithATooLongHostNameIsNotImported)
{
    const qint64 group = addGroup(source_, "group", 0);
    addHost(source_, "host", group);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->local_hosts_size(), 1);
        data->mutable_local_hosts(0)->set_name(
            std::string(LocalHostConfig::kMaxNameLength + 1, 'a'));
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_TRUE(hostNames(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// An address is what the host is reached at, and a host of the file carries one.
TEST_F(BackupTest, FileWithAHostWithoutAnAddressIsNotImported)
{
    const qint64 group = addGroup(source_, "group", 0);
    addHost(source_, "host", group);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->local_hosts_size(), 1);
        data->mutable_local_hosts(0)->clear_address();
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_TRUE(hostNames(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Credentials are kept as a pair, so half a pair says nothing at all. It says neither that the
// host is asked at every connection, nor what it is answered with.
TEST_F(BackupTest, FileWithHalfTheCredentialsOfAHostIsNotImported)
{
    const qint64 group = addGroup(source_, "group", 0);
    addHost(source_, "host", group);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->local_hosts_size(), 1);
        data->mutable_local_hosts(0)->set_username("user");
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_TRUE(hostNames(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A guid is what says that two records are one record, so a record of the file carries one and no
// two records of the file carry the same.
TEST_F(BackupTest, FileWithAHostWithoutAGuidIsNotImported)
{
    const qint64 group = addGroup(source_, "group", 0);
    addHost(source_, "host", group);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->local_hosts_size(), 1);
        data->mutable_local_hosts(0)->clear_guid();
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_TRUE(hostNames(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(BackupTest, FileWithTwoHostsUnderOneGuidIsNotImported)
{
    const qint64 group = addGroup(source_, "group", 0);
    addHost(source_, "first", group);
    addHost(source_, "second", group);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->local_hosts_size(), 2);
        data->mutable_local_hosts(1)->set_guid(data->local_hosts(0).guid());
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_TRUE(hostNames(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A host reaches the router named among the routers of the file, or no router at all.
TEST_F(BackupTest, FileWhereAHostReachesARouterItDoesNotCarryIsNotImported)
{
    const qint64 group = addGroup(source_, "group", 0);
    const qint64 router_id = addRouter(source_, "router", "router.example.com");
    addRoutedHost(source_, "through-router", group, router_id, "100500");

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->local_hosts_size(), 1);
        data->mutable_local_hosts(0)->set_router_guid(unknownGuid().toStdString());
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_TRUE(hostNames(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The same holds for the credentials saved for a host of a router.
TEST_F(BackupTest, FileWhereCredentialsNameARouterItDoesNotCarryIsNotImported)
{
    const qint64 router_id = addRouter(source_, "router", "router.example.com");
    addRouterHost(source_, router_id, 100500, "user", "secret");

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->router_hosts_size(), 1);
        data->mutable_router_hosts(0)->set_router_guid(unknownGuid().toStdString());
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_TRUE(allRouterHosts(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The file is sealed with the key of the book it came from, and nothing is written without it.
TEST_F(BackupTest, BookIsNotImportedWithAnotherPassword)
{
    addGroup(source_, "group", 0);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    EXPECT_EQ(importBookWithPassword(SecureString(QString("Other123"))),
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

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    QFile file(backupPath());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));

    const QByteArray bytes = file.readAll();
    ASSERT_FALSE(bytes.isEmpty());

    EXPECT_FALSE(bytes.contains("accounting-department"));
    EXPECT_FALSE(bytes.contains("prod-database-server"));
    EXPECT_FALSE(bytes.contains("192.168.0.1"));
}

//--------------------------------------------------------------------------------------------------
// The credentials saved for hosts of a router travel with that router. The id it has here belongs
// to the database it came from, so what they hang on is the router the import wrote.
TEST_F(BackupTest, SavedCredentialsFollowTheirRouter)
{
    const qint64 source_router = addRouter(source_, "router", "router.example.com");
    addRouterHost(source_, source_router, 100500, "user", "secret");
    addRouterHost(source_, source_router, 100501, "other-user", "other-secret");

    const std::optional<RouterConfig> original = source_.findRouter(source_router);
    ASSERT_TRUE(original.has_value());

    Backup::Report export_counts;
    ASSERT_EQ(exportBook(&export_counts), Backup::Result::SUCCESS);
    EXPECT_EQ(export_counts.router_hosts, 2);

    // A router of this database, which the file replaces along with everything else.
    addRouter(target_, "decoy", "decoy.example.com");

    Backup::Report report;
    ASSERT_EQ(importBook(&report), Backup::Result::SUCCESS);

    EXPECT_EQ(report.routers, 1);
    EXPECT_EQ(report.router_hosts, 2);

    const QList<RouterConfig> routers = routerList(target_);
    ASSERT_EQ(routers.size(), 1);
    EXPECT_EQ(routers.front().guid(), original->guid());

    const qint64 target_router = routers.front().routerId();

    const std::optional<RouterHostConfig> stored = target_.findRouterHost(target_router, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));

    EXPECT_TRUE(target_.findRouterHost(target_router, 100501).has_value());
}

//--------------------------------------------------------------------------------------------------
// The guid is what binds the saved credentials to a router across databases, so a router created
// by the import carries the guid of the file, and the credentials arrive bound to it.
TEST_F(BackupTest, ImportedRouterKeepsItsGuid)
{
    const qint64 router_id = addRouter(source_, "router", "router.example.com");
    addRouterHost(source_, router_id, 100500, "user", "secret");

    const std::optional<RouterConfig> original = source_.findRouter(router_id);
    ASSERT_TRUE(original.has_value());

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    const QList<RouterConfig> routers = routerList(target_);
    ASSERT_EQ(routers.size(), 1);
    EXPECT_EQ(routers.front().guid(), original->guid());

    const std::optional<RouterHostConfig> stored =
        target_.findRouterHost(routers.front().routerId(), 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->username(), "user");
    EXPECT_EQ(stored->password().toString(), "secret");
}

//--------------------------------------------------------------------------------------------------
// A name of its own is not required of a router, and the address book shows the address in place of
// it. Written into the record on the way in, the address would become a name the user never gave
// and would stay behind when the address changes.
TEST_F(BackupTest, RouterWithoutANameComesBackWithoutOne)
{
    addRouter(source_, QString(), "router.example.com");

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    const QList<RouterConfig> routers = routerList(target_);
    ASSERT_EQ(routers.size(), 1);

    EXPECT_TRUE(routers.front().displayName().isEmpty());
    EXPECT_EQ(routers.front().displayLabel(), "router.example.com");
}

//--------------------------------------------------------------------------------------------------
// A guid names one router, so a file naming two of them by the same guid is not an address book.
// Nothing of it is taken, not even the records that are fine on their own.
TEST_F(BackupTest, FileNamingOneRouterTwiceIsNotImported)
{
    addRouter(source_, "first", "first.example.com");
    addRouter(source_, "second", "second.example.com");
    addGroup(source_, "group", 0);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->routers_size(), 2);
        data->mutable_routers(1)->set_guid(data->routers(0).guid());
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);

    EXPECT_TRUE(routerList(target_).isEmpty());
    EXPECT_TRUE(groupNames(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A temporary id is handed out again once the host holding it is gone, so credentials saved under
// one say nothing about the host wearing it now. The address book does not keep such a row, and a
// file carrying one is not a file this application wrote.
TEST_F(BackupTest, SavedCredentialsOfATemporaryHostAreNotImported)
{
    const qint64 router_id = addRouter(source_, "router", "router.example.com");

    RouterHostConfig temporary;
    temporary.setRouterId(router_id);
    temporary.setHostId(kMinTempHostId);
    temporary.setUsername("user");
    temporary.setPassword(SecureString(QString("secret")));
    EXPECT_FALSE(source_.addRouterHost(temporary));

    addRouterHost(source_, router_id, 100500, "other-user", "other-secret");

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->router_hosts_size(), 1);
        data->mutable_router_hosts(0)->set_host_id(kMinTempHostId);
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_TRUE(allRouterHosts(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// An address book with nothing to write leaves no file behind, and the caller is told why instead
// of being handed a failure it can make nothing of.
TEST_F(BackupTest, EmptyBookIsNotWrittenToAFile)
{
    EXPECT_EQ(exportBook(), Backup::Result::NOTHING_EXPORTED);
    EXPECT_FALSE(QFile::exists(backupPath()));

    addGroup(source_, "group", 0);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    EXPECT_TRUE(QFile::exists(backupPath()));
}

//--------------------------------------------------------------------------------------------------
// A record whose sealed column does not open comes back with its address, user name and password
// empty. A file written out of such a book would be missing exactly the records it exists for, so
// no file is written at all and no half-empty file is left behind.
TEST_F(BackupTest, BookThatCannotBeReadWholeIsNotExported)
{
    const qint64 router_id = addRouter(source_, "router", "router.example.com");
    const qint64 group = addGroup(source_, "group", 0);

    addHost(source_, "host", group);
    addRoutedHost(source_, "through-router", group, router_id, "100500");
    addRouterHost(source_, router_id, 100500, "user", "secret");

    // The key of the process is what opens the sealed columns of the address book.
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

    Backup::Report report;
    EXPECT_EQ(exportBook(&report), Backup::Result::INTERNAL_ERROR);
    EXPECT_FALSE(QFile::exists(backupPath()));

    EXPECT_EQ(exportBook(&report), Backup::Result::INTERNAL_ERROR);
}

//--------------------------------------------------------------------------------------------------
// The same holds for a host of a book that is otherwise readable. One record that does not open
// is enough for the file not to be written.
TEST_F(BackupTest, BookWithOneUnreadableHostIsNotExported)
{
    const qint64 group = addGroup(source_, "group", 0);
    addHost(source_, "host", group);

    // The key of the process is what opens the sealed columns of the address book.
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

    Backup::Report report;
    EXPECT_EQ(exportBook(&report), Backup::Result::INTERNAL_ERROR);
    EXPECT_FALSE(QFile::exists(backupPath()));

    // The group was read and counted before the host refused to open. The report counts what went
    // into the file, and no file was written.
    EXPECT_EQ(report.total(), 0);
}

//--------------------------------------------------------------------------------------------------
// The whole book goes into the file, and the whole file comes back. Nothing is picked out on either
// side.
TEST_F(BackupTest, WholeBookTravels)
{
    const qint64 router_id = addRouter(source_, "router", "router.example.com");
    const qint64 group = addGroup(source_, "group", 0);

    addHost(source_, "direct", group);
    addRoutedHost(source_, "through-router", group, router_id, "100500");
    addRouterHost(source_, router_id, 100500, "user", "secret");

    Backup::Report exported;
    ASSERT_EQ(exportBook(&exported), Backup::Result::SUCCESS);

    EXPECT_EQ(exported.routers, 1);
    EXPECT_EQ(exported.local_groups, 1);
    EXPECT_EQ(exported.local_hosts, 2);
    EXPECT_EQ(exported.router_hosts, 1);

    Backup::Report imported;
    ASSERT_EQ(importBook(&imported), Backup::Result::SUCCESS);

    EXPECT_EQ(imported.routers, 1);
    EXPECT_EQ(imported.local_groups, 1);
    EXPECT_EQ(imported.local_hosts, 2);
    EXPECT_EQ(imported.router_hosts, 1);
}

//--------------------------------------------------------------------------------------------------
// A file of another address book does not open with the key of this one, and it takes the master
// password of the book it was saved from.
TEST_F(BackupTest, FileOfAnotherBookTakesItsOwnPassword)
{
    addGroup(source_, "group", 0);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    // What another machine is: an address book whose master password is not this one.
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

    EXPECT_EQ(importBook(), Backup::Result::WRONG_PASSWORD);

    ASSERT_EQ(Backup::importFromFile(target_, backupPath(), password(), nullptr),
              Backup::Result::SUCCESS);

    EXPECT_EQ(groupNames(target_), QStringList({ "group" }));
}

//--------------------------------------------------------------------------------------------------
// When a host was made, changed and last connected to are columns of the list the user reads and
// sorts by. Stamped with the moment of the import, every host of a restored book would claim it was
// made then.
TEST_F(BackupTest, MomentsOfAHostTravelWithIt)
{
    const qint64 entry_id = addHost(source_, "host", 0);
    ASSERT_TRUE(source_.setLocalHostConnectTime(entry_id, 1500000000));

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    // The moments of a record made now are the moments the import would write by itself, so the
    // file says something else and the answer tells the two apart.
    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->local_hosts_size(), 1);
        EXPECT_EQ(data->local_hosts(0).connect_time(), 1500000000);

        data->mutable_local_hosts(0)->set_create_time(1400000000);
        data->mutable_local_hosts(0)->set_modify_time(1450000000);
    });

    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    const QList<LocalHostConfig> hosts = allLocalHosts(target_);
    ASSERT_EQ(hosts.size(), 1);

    EXPECT_EQ(hosts.front().createTime(), 1400000000);
    EXPECT_EQ(hosts.front().modifyTime(), 1450000000);
    EXPECT_EQ(hosts.front().connectTime(), 1500000000);
}

//--------------------------------------------------------------------------------------------------
// A host whose router the book no longer has is not a host anyone left out, so it travels. The
// router it named is gone, so in the file the host stands on its own.
TEST_F(BackupTest, HostOfARemovedRouterTravels)
{
    const qint64 router_id = addRouter(source_, "router", "router.example.com");

    addRoutedHost(source_, "through-router", 0, router_id, "100500");
    ASSERT_TRUE(source_.removeRouter(router_id));

    Backup::Report report;
    ASSERT_EQ(exportBook(&report), Backup::Result::SUCCESS);

    EXPECT_EQ(report.local_hosts, 1);

    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    ASSERT_EQ(allLocalHosts(target_).size(), 1);
    EXPECT_EQ(allLocalHosts(target_).first().name(), QString("through-router"));
    EXPECT_EQ(allLocalHosts(target_).first().routerId(), 0);
}

//--------------------------------------------------------------------------------------------------
// An import is a restore: what the address book held before is gone, and what the file carries
// stands in its place.
TEST_F(BackupTest, ImportReplacesTheWholeBook)
{
    const qint64 group = addGroup(source_, "from the file", 0);
    addHost(source_, "host of the file", group);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    // What the target machine held before the file was read.
    const qint64 own_router = addRouter(target_, "own router", "other.example.com");
    const qint64 own_group = addGroup(target_, "own group", 0);
    addRoutedHost(target_, "own host", own_group, own_router, "100500");
    addRouterHost(target_, own_router, 100500, "user", "secret");

    Backup::Report report;
    ASSERT_EQ(importBook(&report), Backup::Result::SUCCESS);

    EXPECT_EQ(report.local_groups, 1);
    EXPECT_EQ(report.local_hosts, 1);

    EXPECT_EQ(groupNames(target_), QStringList({ "from the file" }));
    EXPECT_EQ(hostNames(target_), QStringList({ "host of the file" }));
    EXPECT_TRUE(routerList(target_).isEmpty());
    EXPECT_TRUE(allRouterHosts(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The same file read twice leaves the same address book, and no copy of anything appears.
TEST_F(BackupTest, ImportedTwiceLeavesOneBook)
{
    const qint64 router_id = addRouter(source_, "router", "router.example.com");
    const qint64 group = addGroup(source_, "group", 0);

    addRoutedHost(source_, "through-router", group, router_id, "100500");
    addRouterHost(source_, router_id, 100500, "user", "secret");

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);
    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    EXPECT_EQ(routerList(target_).size(), 1);
    EXPECT_EQ(allLocalGroups(target_).size(), 1);
    EXPECT_EQ(allLocalHosts(target_).size(), 1);
    EXPECT_EQ(allRouterHosts(target_).size(), 1);
}

//--------------------------------------------------------------------------------------------------
// A file of some other kind is not a backup written by a version nobody has yet. Read by its
// version alone, it would be answered as one, and the user would go looking for a newer build.
TEST_F(BackupTest, FileOfSomeOtherKindIsNotOneOfAnotherVersion)
{
    // One field this application does not know. Such bytes parse into a message with none of the
    // fields a backup carries, which is what anything that is not a backup looks like here.
    QFile file(backupPath());
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(QByteArray("\x38\x01", 2)), 2);
    file.close();

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
}

//--------------------------------------------------------------------------------------------------
// A file that is ours and carries a version this build does not know is answered by its version.
TEST_F(BackupTest, FileOfAnotherVersionIsNamedByItsVersion)
{
    addGroup(source_, "group", 0);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    proto::storage::BackupFile file_message;

    {
        QFile file(backupPath());
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        ASSERT_TRUE(parse(file.readAll(), &file_message));
    }

    file_message.set_version(file_message.version() + 1);

    const QByteArray payload = serialize(file_message);

    QFile file(backupPath());
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    ASSERT_EQ(file.write(payload), payload.size());
    file.close();

    EXPECT_EQ(importBook(), Backup::Result::UNSUPPORTED_VERSION);
}

//--------------------------------------------------------------------------------------------------
// A file that carries no address book says nothing about what the book should hold, so the book is
// left as it is instead of being emptied. Our own export never writes such a file, but a file comes
// from wherever the user got it.
TEST_F(BackupTest, FileWithoutAnAddressBookLeavesTheBookAlone)
{
    addGroup(source_, "group", 0);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    // A payload of one field this application does not know. It opens, it parses, and it carries
    // no record at all.
    sealPayload(QByteArray("\x38\x01", 2));

    addGroup(target_, "own group", 0);

    EXPECT_EQ(importBook(), Backup::Result::NOTHING_IMPORTED);
    EXPECT_EQ(groupNames(target_), QStringList({ "own group" }));
}

//--------------------------------------------------------------------------------------------------
// The batch of an import goes into the book whole or not at all. A batch that was rolled back left
// nothing behind, and the report says the same.
TEST_F(BackupTest, ReportOfARefusedImportCountsNothing)
{
    const qint64 group = addGroup(source_, "group", 0);
    addHost(source_, "host", group);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    ASSERT_TRUE(execOnTarget("DROP TABLE local_hosts"));

    Backup::Report report;
    EXPECT_EQ(importBook(&report), Backup::Result::INTERNAL_ERROR);
    EXPECT_EQ(report.total(), 0);
}

//--------------------------------------------------------------------------------------------------
// A host reached through a router keeps naming it across the file. The router arrives in the book
// as a new record with an id of its own, so a host left with the id the file carries would name
// nothing, and the book would show it as a direct connection to an address that is a host id.
TEST_F(BackupTest, HostFollowsItsRouter)
{
    const qint64 router_id = addRouter(source_, "router", "router.example.com");
    const qint64 group = addGroup(source_, "group", 0);

    addHost(source_, "direct", group);
    addRoutedHost(source_, "through-router", group, router_id, "100500");

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    // A router of this book, which the import replaces along with everything else. Its id is spent,
    // so the record that arrives cannot get the id the file names it by.
    addRouter(target_, "decoy", "decoy.example.com");

    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    const QList<RouterConfig> routers = routerList(target_);
    ASSERT_EQ(routers.size(), 1);
    ASSERT_NE(routers.front().routerId(), router_id);

    const std::optional<LocalHostConfig> routed = hostByName(target_, "through-router");
    ASSERT_TRUE(routed.has_value());
    EXPECT_EQ(routed->routerId(), routers.front().routerId());

    // A host of the file that reaches no router keeps reaching none.
    const std::optional<LocalHostConfig> direct = hostByName(target_, "direct");
    ASSERT_TRUE(direct.has_value());
    EXPECT_EQ(direct->routerId(), 0);
}
