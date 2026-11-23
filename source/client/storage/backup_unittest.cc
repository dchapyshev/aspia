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

#include "client/storage/backup.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUuid>

#include <functional>

#include "base/serialization.h"
#include "base/crypto/password_hash.h"
#include "base/net/udp_channel.h"
#include "base/peer/host_id.h"
#include "client/storage/database_test_base.h"
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
        // with a master password set. Database::Reencryption with nothing to re-encrypt is how a
        // book gets one without going through the singleton MasterPassword talks to.
        const QByteArray salt = Random::byteArray(32);
        const SecureByteArray key(PasswordHash::hash(PasswordHash::ARGON2ID, password(), salt));

        const std::optional<QByteArray> verifier =
            DataCryptor(key).encrypt(Random::byteArray(32));
        ASSERT_TRUE(verifier.has_value());

        Database::Reencryption reencryption(source_);
        ASSERT_EQ(reencryption.begin(), Database::ReencryptResult::DONE);
        DataCryptor::instance().setKey(key);
        ASSERT_TRUE(reencryption.commit(salt, *verifier, 1));
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

        EXPECT_TRUE(LocalGroupStore(db).add(group));
        return group.id();
    }

    static qint64 addHost(Database& db, const QString& name, qint64 group_id)
    {
        LocalHostConfig host;
        host.setName(name);
        host.setAddress("192.168.0.1");
        host.setGroupId(group_id);

        EXPECT_TRUE(LocalHostStore(db).add(host));
        return host.id();
    }

    static void addRouter(Database& db, const QString& address)
    {
        RouterConfig router;
        router.setAddress(address);
        router.setUsername("router-user");
        router.setPassword(SecureString(QString("router-secret")));

        EXPECT_TRUE(RouterStore(db).save(router));
    }

    static void addRouterHost(Database& db, HostId host_id, const QString& username, const QString& password)
    {
        RouterHostConfig host;
        host.setHostId(host_id);
        host.setUsername(username);
        host.setPassword(SecureString(password));

        EXPECT_TRUE(RouterHostStore(db).add(host));
    }

    static qint64 addCredential(Database& db, const QString& name, const QString& username,
                                const QString& password)
    {
        CredentialConfig credential;
        credential.setDisplayName(name);
        credential.setUsername(username);
        credential.setPassword(SecureString(password));

        EXPECT_TRUE(CredentialStore(db).add(credential));
        return credential.id();
    }

    static qint64 addRoutedHost(Database& db, const QString& name, qint64 group_id, const QString& address)
    {
        LocalHostConfig host;
        host.setName(name);
        host.setAddress(address);
        host.setGroupId(group_id);

        EXPECT_TRUE(LocalHostStore(db).add(host));
        return host.id();
    }

    // The list scans of the fixture: the read is expected to succeed, and the list comes back by
    // value the way the tests consume it.
    static QList<LocalHostConfig> allLocalHosts(Database& db)
    {
        QList<LocalHostConfig> hosts;
        EXPECT_EQ(LocalHostStore(db).all(LocalHostStore::Scope::VISIBLE, &hosts), Database::ReadResult::OK);
        return hosts;
    }

    static QList<LocalGroupConfig> allLocalGroups(Database& db)
    {
        QList<LocalGroupConfig> groups;
        EXPECT_TRUE(LocalGroupStore(db).all(&groups));
        return groups;
    }

    static QList<RouterHostConfig> allRouterHosts(Database& db)
    {
        QList<RouterHostConfig> hosts;
        EXPECT_EQ(RouterHostStore(db).list(&hosts), Database::ReadResult::OK);
        return hosts;
    }

    static QList<CredentialConfig> credentialList(Database& db)
    {
        QList<CredentialConfig> credentials;
        EXPECT_EQ(CredentialStore(db).list(&credentials), Database::ReadResult::OK);
        return credentials;
    }

    static qint64 addProfile(Database& db, const QString& name)
    {
        ProfileConfig profile;
        profile.setName(name);
        profile.setDesktopConfig(defaultDesktopConfig());

        EXPECT_TRUE(ProfileStore(db).add(profile));
        return profile.id();
    }

    // The profiles the user made, the default one left out.
    static QList<ProfileConfig> profileList(Database& db)
    {
        QList<ProfileConfig> profiles;
        EXPECT_EQ(ProfileStore(db).list(&profiles), Database::ReadResult::OK);
        EXPECT_TRUE(!profiles.isEmpty() && profiles.front().isDefault());
        profiles.removeFirst();
        return profiles;
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
        DataCryptor cryptor(key);

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
            DataCryptor(key).encrypt(payload);
        ASSERT_TRUE(sealed.has_value());

        file_message.set_data(sealed->toStdString());

        QFile file(backupPath());
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(serialize(file_message));
    }

    // Marks the file with |version| and leaves the rest of it as the export wrote it.
    void setFileVersion(quint32 version)
    {
        proto::storage::BackupFile file_message;

        {
            QFile file(backupPath());
            ASSERT_TRUE(file.open(QIODevice::ReadOnly));
            ASSERT_TRUE(parse(file.readAll(), &file_message));
        }

        file_message.set_version(version);

        const QByteArray payload = serialize(file_message);

        QFile file(backupPath());
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        ASSERT_EQ(file.write(payload), payload.size());
    }

    // The guid the source book gave the group, which is how the file names it.
    QString groupGuid(qint64 group_id)
    {
        const std::optional<LocalGroupConfig> group = findLocalGroup(source_, group_id);
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
    EXPECT_EQ(LocalHostStore(target_).list(new_child->id(), &child_hosts), Database::ReadResult::OK);
    EXPECT_EQ(child_hosts.size(), 1);
}

//--------------------------------------------------------------------------------------------------
// The id of a group is good inside one database only, so the file carries the guid as well: the
// group that arrives is the same group the file was made from.
TEST_F(BackupTest, GroupKeepsItsGuidThroughTheFile)
{
    const qint64 group = addGroup(source_, "group", 0);

    const std::optional<LocalGroupConfig> original = findLocalGroup(source_, group);
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
    addRouter(source_, "router.example.com");

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_TRUE(data->has_router());
        data->mutable_router()->clear_password();
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_FALSE(findRouter(target_).has_value());
}

//--------------------------------------------------------------------------------------------------
TEST_F(BackupTest, FileWithARouterOfAnUnknownSessionTypeIsNotImported)
{
    addRouter(source_, "router.example.com");

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_TRUE(data->has_router());
        data->mutable_router()->set_session_type(proto::router::SESSION_TYPE_HOST);
    });

    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);
    EXPECT_FALSE(findRouter(target_).has_value());
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
// The credentials saved for hosts of the router travel with the router. They are named by the id
// of the host on the router, so they come back for the router the import wrote and not for the one
// the database held before.
TEST_F(BackupTest, SavedCredentialsFollowTheirRouter)
{
    addRouter(source_, "router.example.com");
    addRouterHost(source_, 100500, "user", "secret");
    addRouterHost(source_, 100501, "other-user", "other-secret");

    Backup::Report export_counts;
    ASSERT_EQ(exportBook(&export_counts), Backup::Result::SUCCESS);
    EXPECT_EQ(export_counts.router_hosts, 2);

    // A router of this database, which the file replaces along with everything else.
    addRouter(target_, "decoy.example.com");

    Backup::Report report;
    ASSERT_EQ(importBook(&report), Backup::Result::SUCCESS);

    EXPECT_TRUE(report.router);
    EXPECT_EQ(report.router_hosts, 2);

    ASSERT_TRUE(findRouter(target_).has_value());

    const std::optional<RouterHostConfig> stored = findRouterHost(target_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));

    EXPECT_TRUE(findRouterHost(target_, 100501).has_value());
}

//--------------------------------------------------------------------------------------------------
// A router that was turned off comes back turned off.
TEST_F(BackupTest, RouterTurnedOffStaysTurnedOff)
{
    RouterConfig router;
    router.setAddress("router.example.com");
    router.setUsername("router-user");
    router.setPassword(SecureString(QString("router-secret")));
    router.setEnabled(false);
    ASSERT_TRUE(RouterStore(source_).save(router));

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    const std::optional<RouterConfig> imported = findRouter(target_);
    ASSERT_TRUE(imported.has_value());
    EXPECT_FALSE(imported->isEnabled());
}

//--------------------------------------------------------------------------------------------------
// A temporary id is handed out again once the host holding it is gone, so credentials saved under
// one say nothing about the host wearing it now. The address book does not keep such a row, and a
// file carrying one is not a file this application wrote.
TEST_F(BackupTest, SavedCredentialsOfATemporaryHostAreNotImported)
{
    addRouter(source_, "router.example.com");

    RouterHostConfig temporary;
    temporary.setHostId(kMinTempHostId);
    temporary.setUsername("user");
    temporary.setPassword(SecureString(QString("secret")));
    EXPECT_FALSE(RouterHostStore(source_).add(temporary));

    addRouterHost(source_, 100500, "other-user", "other-secret");

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
// A fresh address book is not empty: it has its default profile, and that is what its file carries.
TEST_F(BackupTest, FreshBookExportsItsDefaultProfile)
{
    Backup::Report exported;
    ASSERT_EQ(exportBook(&exported), Backup::Result::SUCCESS);
    EXPECT_TRUE(QFile::exists(backupPath()));
    EXPECT_EQ(exported.profiles, 1);
    EXPECT_EQ(exported.total(), 1);

    Backup::Report imported;
    ASSERT_EQ(importBook(&imported), Backup::Result::SUCCESS);
    EXPECT_EQ(imported.profiles, 1);
    EXPECT_EQ(imported.total(), 1);
    EXPECT_TRUE(profileList(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A record whose sealed column does not open comes back with its address, user name and password
// empty. A file written out of such a book would be missing exactly the records it exists for, so
// no file is written at all and no half-empty file is left behind.
TEST_F(BackupTest, BookThatCannotBeReadWholeIsNotExported)
{
    addRouter(source_, "router.example.com");
    const qint64 group = addGroup(source_, "group", 0);

    addHost(source_, "host", group);
    addRoutedHost(source_, "through-router", group, "100500");
    addRouterHost(source_, 100500, "user", "secret");

    // The key of the process is what opens the sealed columns of the address book.
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

    Backup::Report report;
    EXPECT_EQ(exportBook(&report), Backup::Result::UNREADABLE_RECORD);
    EXPECT_FALSE(QFile::exists(backupPath()));

    EXPECT_EQ(exportBook(&report), Backup::Result::UNREADABLE_RECORD);
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
    EXPECT_EQ(exportBook(&report), Backup::Result::UNREADABLE_RECORD);
    EXPECT_FALSE(QFile::exists(backupPath()));

    // The book is read before anything of it is built, so nothing was counted and no file
    // was written.
    EXPECT_EQ(report.total(), 0);
}

//--------------------------------------------------------------------------------------------------
// The whole book goes into the file, and the whole file comes back. Nothing is picked out on either
// side.
TEST_F(BackupTest, WholeBookTravels)
{
    addRouter(source_, "router.example.com");
    const qint64 group = addGroup(source_, "group", 0);

    addHost(source_, "direct", group);
    addRoutedHost(source_, "through-router", group, "100500");
    addRouterHost(source_, 100500, "user", "secret");
    addCredential(source_, "office", "user", "secret");

    Backup::Report exported;
    ASSERT_EQ(exportBook(&exported), Backup::Result::SUCCESS);

    EXPECT_TRUE(exported.router);
    EXPECT_EQ(exported.local_groups, 1);
    EXPECT_EQ(exported.local_hosts, 2);
    EXPECT_EQ(exported.router_hosts, 1);
    EXPECT_EQ(exported.credentials, 1);

    Backup::Report imported;
    ASSERT_EQ(importBook(&imported), Backup::Result::SUCCESS);

    EXPECT_TRUE(imported.router);
    EXPECT_EQ(imported.local_groups, 1);
    EXPECT_EQ(imported.local_hosts, 2);
    EXPECT_EQ(imported.router_hosts, 1);
    EXPECT_EQ(imported.credentials, 1);
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
    ASSERT_TRUE(LocalHostStore(source_).setConnectTime(entry_id, 1500000000));

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
// An import is a restore: what the address book held before is gone, and what the file carries
// stands in its place.
TEST_F(BackupTest, ImportReplacesTheWholeBook)
{
    const qint64 group = addGroup(source_, "from the file", 0);
    addHost(source_, "host of the file", group);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    // What the target machine held before the file was read.
    addRouter(target_, "other.example.com");
    const qint64 own_group = addGroup(target_, "own group", 0);
    addRoutedHost(target_, "own host", own_group, "100500");
    addRouterHost(target_, 100500, "user", "secret");
    addCredential(target_, "own credential", "user", "secret");
    addProfile(target_, "own profile");

    Backup::Report report;
    ASSERT_EQ(importBook(&report), Backup::Result::SUCCESS);

    EXPECT_EQ(report.local_groups, 1);
    EXPECT_EQ(report.local_hosts, 1);

    EXPECT_EQ(groupNames(target_), QStringList({ "from the file" }));
    EXPECT_EQ(hostNames(target_), QStringList({ "host of the file" }));
    EXPECT_FALSE(findRouter(target_).has_value());
    EXPECT_TRUE(allRouterHosts(target_).isEmpty());
    EXPECT_TRUE(credentialList(target_).isEmpty());
    EXPECT_TRUE(profileList(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The same file read twice leaves the same address book, and no copy of anything appears.
TEST_F(BackupTest, ImportedTwiceLeavesOneBook)
{
    addRouter(source_, "router.example.com");
    const qint64 group = addGroup(source_, "group", 0);

    addRoutedHost(source_, "through-router", group, "100500");
    addRouterHost(source_, 100500, "user", "secret");

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);
    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    EXPECT_TRUE(findRouter(target_).has_value());
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
    ASSERT_NO_FATAL_FAILURE(setFileVersion(3));

    EXPECT_EQ(importBook(), Backup::Result::UNSUPPORTED_VERSION);
}

//--------------------------------------------------------------------------------------------------
// A file of the first version comes from a book that held several routers, so it is not imported.
TEST_F(BackupTest, FileOfTheFirstVersionIsNotImported)
{
    addGroup(source_, "group", 0);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    ASSERT_NO_FATAL_FAILURE(setFileVersion(1));

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
// A database holds one router, so the file names no router for a host: a host reached by its ID goes
// through the router of the database it is imported into, and a direct one stays direct.
TEST_F(BackupTest, HostFollowsItsRouter)
{
    addRouter(source_, "router.example.com");
    const qint64 group = addGroup(source_, "group", 0);

    addHost(source_, "direct", group);
    addRoutedHost(source_, "through-router", group, "100500");

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    // A router of this book, which the import replaces along with everything else.
    addRouter(target_, "decoy.example.com");

    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    const std::optional<RouterConfig> router = findRouter(target_);
    ASSERT_TRUE(router.has_value());
    EXPECT_EQ(router->address(), QString("router.example.com"));

    const std::optional<LocalHostConfig> routed = hostByName(target_, "through-router");
    ASSERT_TRUE(routed.has_value());
    EXPECT_EQ(routed->address(), QString("100500"));

    EXPECT_TRUE(hostByName(target_, "direct").has_value());
}

//--------------------------------------------------------------------------------------------------
// A credential travels whole, under the guid the records referring to it know it by.
TEST_F(BackupTest, CredentialKeepsItsGuidThroughTheFile)
{
    const qint64 credential_id = addCredential(source_, "office", "user", "secret");

    const std::optional<CredentialConfig> original = findCredential(source_, credential_id);
    ASSERT_TRUE(original.has_value());

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    const std::optional<CredentialConfig> stored = findCredentialByGuid(target_, original->guid());
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->type(), CredentialConfig::Type::ASPIA);
    EXPECT_EQ(stored->displayName(), QString("office"));
    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
TEST_F(BackupTest, OsCredentialKeepsItsTypeThroughTheFile)
{
    CredentialConfig credential;
    credential.setType(CredentialConfig::Type::OS);
    credential.setDisplayName("workstation");
    credential.setUsername("DOMAIN\\user");
    credential.setPassword(SecureString(QString("secret")));
    ASSERT_TRUE(CredentialStore(source_).add(credential));

    const std::optional<CredentialConfig> original = findCredential(source_, credential.id());
    ASSERT_TRUE(original.has_value());

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    const std::optional<CredentialConfig> stored = findCredentialByGuid(target_, original->guid());
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->type(), CredentialConfig::Type::OS);
    EXPECT_EQ(stored->displayName(), QString("workstation"));
    EXPECT_EQ(stored->username(), QString("DOMAIN\\user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// A credential of the file is a record the address book would take: a name, a whole pair, a kind it
// knows and a guid of its own.
TEST_F(BackupTest, FileWithABrokenCredentialIsNotImported)
{
    addCredential(source_, "office", "user", "secret");
    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->credentials_size(), 1);
        data->mutable_credentials(0)->clear_password();
    });
    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        data->mutable_credentials(0)->set_type(100500);
    });
    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        *data->add_credentials() = data->credentials(0);
    });
    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);

    EXPECT_TRUE(credentialList(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A host names the record of credentials it refers to by guid, so on the other side it refers to
// the record the import wrote. A host of a router remembered by its link alone travels as well.
TEST_F(BackupTest, HostsKeepTheirLinksToCredentialsThroughTheFile)
{
    const qint64 credential_id = addCredential(source_, "office", "user", "secret");
    addRouter(source_, "router.example.com");

    LocalHostConfig local_host;
    local_host.setName("host");
    local_host.setAddress("192.168.0.1");
    local_host.setCredentialId(credential_id);
    ASSERT_TRUE(LocalHostStore(source_).add(local_host));

    RouterHostConfig router_host;
    router_host.setHostId(100500);
    router_host.setCredentialId(credential_id);
    ASSERT_TRUE(RouterHostStore(source_).add(router_host));

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    const QList<CredentialConfig> credentials = credentialList(target_);
    ASSERT_EQ(credentials.size(), 1);

    const QList<LocalHostConfig> local_hosts = allLocalHosts(target_);
    ASSERT_EQ(local_hosts.size(), 1);
    EXPECT_EQ(local_hosts.front().credentialId(), credentials.front().id());

    const QList<RouterHostConfig> router_hosts = allRouterHosts(target_);
    ASSERT_EQ(router_hosts.size(), 1);
    EXPECT_EQ(router_hosts.front().credentialId(), credentials.front().id());
    EXPECT_TRUE(router_hosts.front().username().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The operating system credentials last chosen for a host travel by guid as well.
TEST_F(BackupTest, HostsKeepTheirOsCredentialsThroughTheFile)
{
    CredentialConfig os;
    os.setType(CredentialConfig::Type::OS);
    os.setDisplayName("workstation");
    os.setUsername("DOMAIN\\user");
    os.setPassword(SecureString(QString("secret")));
    ASSERT_TRUE(CredentialStore(source_).add(os));

    addRouter(source_, "router.example.com");

    LocalHostConfig local_host;
    local_host.setName("host");
    local_host.setAddress("192.168.0.1");

    LocalHostStore local_host_store(source_);
    ASSERT_TRUE(local_host_store.add(local_host));
    ASSERT_TRUE(local_host_store.setOsCredentialId(local_host.id(), os.id()));

    RouterHostStore store(source_);

    RouterHostConfig router_host;
    router_host.setHostId(100500);
    router_host.setUsername("user");
    router_host.setPassword(SecureString(QString("secret")));
    ASSERT_TRUE(store.add(router_host));
    ASSERT_TRUE(store.setOsCredentialId(100500, os.id()));

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    const QList<CredentialConfig> credentials = credentialList(target_);
    ASSERT_EQ(credentials.size(), 1);

    const QList<LocalHostConfig> local_hosts = allLocalHosts(target_);
    ASSERT_EQ(local_hosts.size(), 1);
    EXPECT_EQ(local_hosts.front().osCredentialId(), credentials.front().id());

    const QList<RouterHostConfig> router_hosts = allRouterHosts(target_);
    ASSERT_EQ(router_hosts.size(), 1);
    EXPECT_EQ(router_hosts.front().osCredentialId(), credentials.front().id());
}

//--------------------------------------------------------------------------------------------------
// The operating system credentials of a host are a record the file carries, or none.
TEST_F(BackupTest, FileWhereAHostNamesOsCredentialsItDoesNotCarryIsNotImported)
{
    addRouter(source_, "router.example.com");

    LocalHostConfig local_host;
    local_host.setName("host");
    local_host.setAddress("192.168.0.1");
    ASSERT_TRUE(LocalHostStore(source_).add(local_host));

    RouterHostConfig router_host;
    router_host.setHostId(100500);
    router_host.setUsername("user");
    router_host.setPassword(SecureString(QString("secret")));
    ASSERT_TRUE(RouterHostStore(source_).add(router_host));

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        data->mutable_local_hosts(0)->set_os_credential_guid("6f1d2c3b-4a59-4e8f-9a0b-1c2d3e4f5a6b");
    });
    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        data->mutable_router_hosts(0)->set_os_credential_guid("6f1d2c3b-4a59-4e8f-9a0b-1c2d3e4f5a6b");
    });
    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);

    EXPECT_TRUE(allLocalHosts(target_).isEmpty());
    EXPECT_TRUE(allRouterHosts(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A host refers to a record of credentials the file carries, or to none.
TEST_F(BackupTest, FileWhereAHostNamesCredentialsItDoesNotCarryIsNotImported)
{
    const qint64 credential_id = addCredential(source_, "office", "user", "secret");
    addRouter(source_, "router.example.com");

    LocalHostConfig local_host;
    local_host.setName("host");
    local_host.setAddress("192.168.0.1");
    local_host.setCredentialId(credential_id);

    LocalHostStore source_store(source_);
    ASSERT_TRUE(source_store.add(local_host));

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        data->clear_credentials();
    });
    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);

    ASSERT_TRUE(source_store.remove(local_host.id()));

    RouterHostConfig router_host;
    router_host.setHostId(100500);
    router_host.setCredentialId(credential_id);
    ASSERT_TRUE(RouterHostStore(source_).add(router_host));

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        data->clear_credentials();
    });
    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);

    EXPECT_TRUE(allLocalHosts(target_).isEmpty());
    EXPECT_TRUE(allRouterHosts(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A column that does not open has nothing to put into the file. Exported as an empty pair next to
// the link the row carries, it would pass for a row remembered by its link alone and come back
// without the pair. The book is refused as a whole, as with any record that does not open.
TEST_F(BackupTest, BookWithALinkedRouterHostThatDoesNotOpenIsNotExported)
{
    const qint64 credential_id = addCredential(source_, "office", "user", "secret");
    addRouter(source_, "router.example.com");

    RouterHostConfig host;
    host.setHostId(100500);
    host.setCredentialId(credential_id);
    host.setUsername("own-user");
    host.setPassword(SecureString("own-secret"));
    ASSERT_TRUE(RouterHostStore(source_).add(host));

    {
        SqlDatabase raw;
        ASSERT_TRUE(raw.open(QFileInfo(backupPath()).dir().filePath("source.db3")));
        ASSERT_TRUE(raw.exec("UPDATE router_hosts SET data=X'00' WHERE host_id=100500"));
    }

    EXPECT_EQ(exportBook(), Backup::Result::UNREADABLE_RECORD);
}

//--------------------------------------------------------------------------------------------------
// A removed record of credentials the router holds keeps its row, and the hosts keep their links
// to it. The record is not a part of the book any more, so the file carries neither the record
// nor the links, nor the row of a router host that had nothing but such a link.
TEST_F(BackupTest, RemovedCredentialsTakeTheirLinksOutOfTheFile)
{
    const qint64 credential_id = addCredential(source_, "office", "user", "secret");
    addRouter(source_, "router.example.com");

    LocalHostConfig local_host;
    local_host.setName("host");
    local_host.setAddress("192.168.0.1");
    local_host.setCredentialId(credential_id);
    ASSERT_TRUE(LocalHostStore(source_).add(local_host));

    RouterHostConfig router_host;
    router_host.setHostId(100500);
    router_host.setCredentialId(credential_id);
    ASSERT_TRUE(RouterHostStore(source_).add(router_host));

    {
        SqlDatabase raw;
        ASSERT_TRUE(raw.open(QFileInfo(backupPath()).dir().filePath("source.db3")));
        ASSERT_TRUE(raw.exec("UPDATE credentials SET sync_revision=7, sync_dirty=0"));
    }

    ASSERT_TRUE(CredentialStore(source_).remove(credential_id));

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    EXPECT_TRUE(credentialList(target_).isEmpty());
    EXPECT_TRUE(allRouterHosts(target_).isEmpty());

    const QList<LocalHostConfig> local_hosts = allLocalHosts(target_);
    ASSERT_EQ(local_hosts.size(), 1);
    EXPECT_EQ(local_hosts.front().credentialId(), 0);
}

//--------------------------------------------------------------------------------------------------
// A profile comes back with its guid, its name and every parameter it was written with.
TEST_F(BackupTest, ProfileKeepsItsParametersThroughTheFile)
{
    proto::control::Config desktop_config = defaultDesktopConfig();
    desktop_config.set_audio(false);
    desktop_config.set_disable_key_combinations(true);
    desktop_config.set_record_sessions(true);
    desktop_config.set_recording_path("/records");

    ProfileStore source_store(source_);

    ProfileConfig profile;
    profile.setName("office");
    profile.setUdpMethods(0);
    profile.setDesktopConfig(desktop_config);
    ASSERT_TRUE(source_store.add(profile));

    ProfileConfig default_profile = source_store.defaultProfile();
    default_profile.setUdpMethods(UDP_METHOD_DIRECT);
    ASSERT_TRUE(source_store.modify(default_profile));

    Backup::Report exported;
    ASSERT_EQ(exportBook(&exported), Backup::Result::SUCCESS);
    EXPECT_EQ(exported.profiles, 2);
    EXPECT_EQ(exported.total(), 2);

    Backup::Report imported;
    ASSERT_EQ(importBook(&imported), Backup::Result::SUCCESS);
    EXPECT_EQ(imported.profiles, 2);

    const QList<ProfileConfig> profiles = profileList(target_);
    ASSERT_EQ(profiles.size(), 1);
    EXPECT_EQ(profiles.front().guid(), profile.guid());
    EXPECT_EQ(profiles.front().name(), QString("office"));
    EXPECT_EQ(profiles.front().udpMethods(), 0u);
    EXPECT_EQ(serialize(profiles.front().desktopConfig()), serialize(desktop_config));

    EXPECT_EQ(ProfileStore(target_).defaultProfile().udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));
}

//--------------------------------------------------------------------------------------------------
// A file of an older version has no default profile, so the address book ends up with the built-in
// one, and the profiles the user made are replaced like the rest of the book.
TEST_F(BackupTest, FileWithoutADefaultProfileLeavesTheBuiltInOne)
{
    addProfile(source_, "office");
    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        for (int i = 0; i < data->profiles_size(); ++i)
        {
            if (data->profiles(i).guid() == ProfileConfig::kDefaultGuid)
            {
                data->mutable_profiles()->DeleteSubrange(i, 1);
                break;
            }
        }
        ASSERT_EQ(data->profiles_size(), 1);
    });

    ProfileStore store(target_);

    ProfileConfig default_profile = store.defaultProfile();
    default_profile.setUdpMethods(UDP_METHOD_UPNP);
    ASSERT_TRUE(store.modify(default_profile));
    addProfile(target_, "home");

    Backup::Report imported;
    ASSERT_EQ(importBook(&imported), Backup::Result::SUCCESS);
    EXPECT_EQ(imported.profiles, 1);

    EXPECT_GT(store.defaultProfile().id(), 0);
    EXPECT_EQ(store.defaultProfile().udpMethods(), static_cast<quint32>(UDP_METHOD_ALL));

    const QList<ProfileConfig> profiles = profileList(target_);
    ASSERT_EQ(profiles.size(), 1);
    EXPECT_EQ(profiles.front().name(), QString("office"));
}

//--------------------------------------------------------------------------------------------------
// A profile of the file is a record the address book would take: a name and a guid of its own.
TEST_F(BackupTest, FileWithABrokenProfileIsNotImported)
{
    addProfile(source_, "office");
    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);

    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        ASSERT_EQ(data->profiles_size(), 2);
        ASSERT_EQ(data->profiles(1).name(), "office");
        data->mutable_profiles(1)->clear_name();
    });
    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        data->mutable_profiles(0)->set_guid("not a guid");
    });
    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        *data->add_profiles() = data->profiles(0);
    });
    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);

    EXPECT_TRUE(profileList(target_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The profile chosen for a host travels by guid, also for a host of a router without credentials.
TEST_F(BackupTest, HostsKeepTheirProfilesThroughTheFile)
{
    const qint64 profile_id = addProfile(source_, "office");
    addRouter(source_, "router.example.com");

    LocalHostConfig local_host;
    local_host.setName("host");
    local_host.setAddress("192.168.0.1");
    local_host.setProfileId(profile_id);
    ASSERT_TRUE(LocalHostStore(source_).add(local_host));

    ASSERT_TRUE(RouterHostStore(source_).setProfileId(100500, profile_id));

    Backup::Report exported;
    ASSERT_EQ(exportBook(&exported), Backup::Result::SUCCESS);
    EXPECT_EQ(exported.router_hosts, 1);

    ASSERT_EQ(importBook(), Backup::Result::SUCCESS);

    const QList<ProfileConfig> profiles = profileList(target_);
    ASSERT_EQ(profiles.size(), 1);

    const QList<LocalHostConfig> local_hosts = allLocalHosts(target_);
    ASSERT_EQ(local_hosts.size(), 1);
    EXPECT_EQ(local_hosts.front().profileId(), profiles.front().id());

    const QList<RouterHostConfig> router_hosts = allRouterHosts(target_);
    ASSERT_EQ(router_hosts.size(), 1);
    EXPECT_EQ(router_hosts.front().profileId(), profiles.front().id());
    EXPECT_TRUE(router_hosts.front().username().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The profile of a host is one the file carries, or none.
TEST_F(BackupTest, FileWhereAHostNamesAProfileItDoesNotCarryIsNotImported)
{
    const qint64 profile_id = addProfile(source_, "office");
    addRouter(source_, "router.example.com");

    LocalHostConfig local_host;
    local_host.setName("host");
    local_host.setAddress("192.168.0.1");
    ASSERT_TRUE(LocalHostStore(source_).add(local_host));

    ASSERT_TRUE(RouterHostStore(source_).setProfileId(100500, profile_id));

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        data->mutable_local_hosts(0)->set_profile_guid("6f1d2c3b-4a59-4e8f-9a0b-1c2d3e4f5a6b");
    });
    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);

    ASSERT_EQ(exportBook(), Backup::Result::SUCCESS);
    editFileContent([](proto::storage::BackupFile::Content* data)
    {
        data->mutable_router_hosts(0)->set_profile_guid("6f1d2c3b-4a59-4e8f-9a0b-1c2d3e4f5a6b");
    });
    EXPECT_EQ(importBook(), Backup::Result::INVALID_FORMAT);

    EXPECT_TRUE(allLocalHosts(target_).isEmpty());
    EXPECT_TRUE(allRouterHosts(target_).isEmpty());
}
