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

#ifndef CLIENT_STORAGE_DATABASE_TEST_BASE_H
#define CLIENT_STORAGE_DATABASE_TEST_BASE_H

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/net/udp_channel.h"
#include "base/sql/sql_query.h"
#include "client/storage/credential_store.h"
#include "client/storage/local_group_store.h"
#include "client/storage/local_host_store.h"
#include "client/storage/profile_store.h"
#include "client/storage/router_host_store.h"
#include "client/storage/router_store.h"
#include "client/storage/settings_store.h"
#include "client/storage/sync_store.h"
#include "client/storage/unlock_store.h"

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
inline std::optional<RouterConfig> findRouter(Database& db)
{
    RouterConfig router;
    const Database::FindResult result = RouterStore(db).find(&router);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return router;
}

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
inline std::optional<CredentialConfig> findCredential(Database& db, qint64 credential_id)
{
    CredentialConfig credential;
    const Database::FindResult result = CredentialStore(db).find(credential_id, &credential);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return credential;
}

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
inline std::optional<CredentialConfig> findCredentialByGuid(Database& db, const QString& guid)
{
    CredentialConfig credential;
    const Database::FindResult result = CredentialStore(db).findByGuid(guid, &credential);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return credential;
}

//--------------------------------------------------------------------------------------------------
inline Database::ReencryptResult reencrypt(
    Database& db, const SecureByteArray& key, const QByteArray& salt, const QByteArray& verifier)
{
    Database::Reencryption reencryption(db);

    const Database::ReencryptResult begun = reencryption.begin();
    if (begun != Database::ReencryptResult::DONE)
        return begun;

    DataCryptor::instance().setKey(key);

    if (!reencryption.commit(salt, verifier, 1))
        return Database::ReencryptResult::FAILED;

    return Database::ReencryptResult::DONE;
}

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
inline std::optional<RouterHostConfig> findRouterHost(Database& db, HostId host_id)
{
    RouterHostConfig host;
    const Database::FindResult result = RouterHostStore(db).find(host_id, &host);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return host;
}

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
inline std::optional<LocalHostConfig> findLocalHost(Database& db, qint64 entry_id)
{
    LocalHostConfig host;
    const Database::FindResult result = LocalHostStore(db).find(entry_id, &host);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return host;
}

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
inline std::optional<LocalGroupConfig> findLocalGroup(Database& db, qint64 group_id)
{
    LocalGroupConfig group;
    const Database::FindResult result = LocalGroupStore(db).find(group_id, &group);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return group;
}

class DatabaseTestBase : public testing::Test
{
protected:
    void SetUp() override
    {
        // A record of the address book keeps its text encrypted with a key of the process, which
        // the application sets once the master password is entered.
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

        ASSERT_TRUE(dir_.isValid());

        file_path_ = dir_.filePath("client.db3");
        ASSERT_TRUE(db_.open(file_path_));
    }

    // The moment the row |id| of |table| was last edited, which the public API hands out for the
    // local hosts only.
    qint64 modifyTime(const QString& table, const QString& key, qint64 id)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return -1;

        const std::string sql =
            QString("SELECT modify_time FROM %1 WHERE %2=?").arg(table, key).toStdString();

        SqlQuery query(raw, sql);
        query.addInt64(id);

        if (query.next() != SqlQuery::StepResult::ROW)
            return -1;

        return query.columnInt64(0);
    }

    // Writes a moment the public API cannot produce, since it always stamps the current one.
    bool setModifyTime(const QString& table, const QString& key, qint64 id, qint64 modify_time)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        const std::string sql =
            QString("UPDATE %1 SET modify_time=? WHERE %2=?").arg(table, key).toStdString();

        SqlQuery query(raw, sql);
        query.addInt64(modify_time);
        query.addInt64(id);

        return query.exec();
    }

    // Marks the row |id| of |table| as one the router holds at |revision| with no changes since,
    // which only an exchange with the router does.
    bool markSynced(const QString& table, qint64 id, qint64 revision)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        const std::string sql =
            QString("UPDATE %1 SET sync_revision=?, sync_dirty=0 WHERE id=?").arg(table).toStdString();

        SqlQuery query(raw, sql);
        query.addInt64(revision);
        query.addInt64(id);

        return query.exec();
    }

    // The rows of |table|, the ones marked as deleted, which the public API skips, included.
    qint64 rowCount(const QString& table)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return -1;

        const std::string sql = QString("SELECT COUNT(*) FROM %1").arg(table).toStdString();

        SqlQuery query(raw, sql);
        if (query.next() != SqlQuery::StepResult::ROW)
            return -1;

        return query.columnInt64(0);
    }

    // True when the row |id| of |table| is in place, marked as deleted, with its name and its data
    // wiped.
    bool isMarkedDeleted(const QString& table, qint64 id)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        const std::string sql = QString("SELECT COUNT(*) FROM %1 WHERE id=? AND deleted=1 AND name='' AND data=X''")
                                    .arg(table).toStdString();

        SqlQuery query(raw, sql);
        query.addInt64(id);

        return query.next() == SqlQuery::StepResult::ROW && query.columnInt64(0) == 1;
    }

    // Spoils the sealed text of a record the way a damaged database or one of another installation
    // has it: the row is in place, its data does not open.
    bool corruptRecordData(const QString& table, qint64 id)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        const std::string sql =
            QString("UPDATE %1 SET data=X'00' WHERE id=?").arg(table).toStdString();

        SqlQuery query(raw, sql);
        query.addInt64(id);

        return query.exec();
    }

    void addRouter(bool sync_enabled = false)
    {
        RouterConfig router;
        router.setAddress("router.example.com");
        router.setUsername("router-user");
        router.setPassword(SecureString("router-secret"));
        router.setSyncEnabled(sync_enabled);

        EXPECT_TRUE(router_store_.save(router));
    }

    // The synchronization changes the records in a transaction of the storage the device is
    // attached to, so a test that plays the synchronization attaches the device first.
    void attachSyncStorage()
    {
        addRouter(true);

        const SecureByteArray key(Random::byteArray(32));
        ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16), key, false),
                  Database::SyncResult::DONE);
    }

    QByteArray syncStorageId()
    {
        const std::optional<RouterConfig> router = findRouter(db_);
        return router.has_value() ? router->syncStorageId() : QByteArray();
    }

    CredentialConfig credential(const QString& name, const QString& username,
                                const QString& password)
    {
        CredentialConfig config;
        config.setDisplayName(name);
        config.setUsername(username);
        config.setPassword(SecureString(password));
        return config;
    }

    qint64 addCredential(const QString& name, const QString& username, const QString& password)
    {
        CredentialConfig config = credential(name, username, password);
        EXPECT_TRUE(credentials_.add(config));
        return config.id();
    }

    // The list scans of the fixture: the read is expected to succeed, and the list comes back by
    // value the way the tests consume it.
    QList<CredentialConfig> credentialList()
    {
        QList<CredentialConfig> credentials;
        EXPECT_EQ(credentials_.list(&credentials), Database::ReadResult::OK);
        return credentials;
    }

    QList<CredentialStore::Unsynced> unsyncedCredentials()
    {
        QList<CredentialStore::Unsynced> credentials;
        EXPECT_EQ(credentials_.unsynced(&credentials), Database::ReadResult::OK);
        return credentials;
    }

    bool storeReceivedCredential(const CredentialConfig& credential, qint64 sync_revision)
    {
        SyncTransaction transaction(db_);
        return transaction.begin(syncStorageId()) == Database::SyncResult::DONE &&
               credentials_.storeReceived(transaction, credential, sync_revision) && transaction.commit();
    }

    qint64 addProfile(const QString& name)
    {
        ProfileConfig config;
        config.setName(name);
        config.setDesktopConfig(defaultDesktopConfig());
        config.setUdpMethods(UDP_METHOD_ALL);
        EXPECT_TRUE(profiles_.add(config));
        return config.id();
    }

    // The profiles the user made, the default one left out.
    QList<ProfileConfig> profileList()
    {
        QList<ProfileConfig> profiles;
        EXPECT_EQ(profiles_.list(&profiles), Database::ReadResult::OK);
        EXPECT_TRUE(!profiles.isEmpty() && profiles.front().isDefault());
        profiles.removeFirst();
        return profiles;
    }

    QList<ProfileStore::Unsynced> unsyncedProfiles()
    {
        QList<ProfileStore::Unsynced> profiles;
        EXPECT_EQ(profiles_.unsynced(&profiles), Database::ReadResult::OK);
        return profiles;
    }

    bool storeReceivedProfile(const ProfileConfig& profile, qint64 sync_revision)
    {
        SyncTransaction transaction(db_);
        return transaction.begin(syncStorageId()) == Database::SyncResult::DONE &&
               profiles_.storeReceived(transaction, profile, sync_revision) && transaction.commit();
    }

    // Writes a check time the public API cannot produce, since it always stamps the current one.
    bool setRouterHostCheckTime(HostId host_id, qint64 check_time)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        SqlQuery query(raw, "UPDATE router_hosts SET check_time=? WHERE host_id=?");
        query.addInt64(check_time);
        query.addUInt64(host_id);

        return query.exec();
    }

    // Spoils the sealed text of a router host the way corruptRecordData() does for a record of a table.
    // The row is named by the id of its host.
    bool corruptRouterHostData(HostId host_id)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        SqlQuery query(raw, "UPDATE router_hosts SET data=X'00' WHERE host_id=?");
        query.addUInt64(host_id);

        return query.exec();
    }

    RouterHostConfig routerHost(HostId host_id, const QString& username, const QString& password)
    {
        RouterHostConfig host;
        host.setHostId(host_id);
        host.setUsername(username);
        host.setPassword(SecureString(password));
        return host;
    }

    void addRouterHost(HostId host_id, const QString& username, const QString& password)
    {
        EXPECT_TRUE(router_hosts_.add(routerHost(host_id, username, password)));
    }

    QList<RouterHostConfig> allRouterHosts()
    {
        QList<RouterHostConfig> hosts;
        EXPECT_EQ(router_hosts_.list(&hosts), Database::ReadResult::OK);
        return hosts;
    }

    QList<HostId> outdatedRouterHosts()
    {
        QList<HostId> hosts;
        EXPECT_TRUE(router_hosts_.outdated(&hosts));
        return hosts;
    }

    QList<RouterHostStore::Unsynced> unsyncedRouterHosts()
    {
        QList<RouterHostStore::Unsynced> hosts;
        EXPECT_EQ(router_hosts_.unsynced(&hosts), Database::ReadResult::OK);
        return hosts;
    }

    bool storeReceivedRouterHost(const RouterHostConfig& host, qint64 sync_revision)
    {
        SyncTransaction transaction(db_);
        return transaction.begin(syncStorageId()) == Database::SyncResult::DONE &&
               router_hosts_.storeReceived(transaction, host, sync_revision) && transaction.commit();
    }

    qint64 addGroup(const QString& name, qint64 parent_id)
    {
        LocalGroupConfig group;
        group.setName(name);
        group.setParentId(parent_id);

        EXPECT_TRUE(local_groups_.add(group));
        return group.id();
    }

    QStringList groupNames()
    {
        QStringList names;
        for (const LocalGroupConfig& group : allLocalGroups())
            names.append(group.name());
        names.sort();
        return names;
    }

    QList<LocalGroupConfig> localGroupList(qint64 parent_id)
    {
        QList<LocalGroupConfig> groups;
        EXPECT_TRUE(local_groups_.list(parent_id, &groups));
        return groups;
    }

    QList<LocalGroupConfig> allLocalGroups()
    {
        QList<LocalGroupConfig> groups;
        EXPECT_TRUE(local_groups_.all(&groups));
        return groups;
    }

    qint64 addHost(const QString& name, qint64 group_id)
    {
        LocalHostConfig host;
        host.setName(name);
        host.setAddress("192.168.0.1");
        host.setGroupId(group_id);

        EXPECT_TRUE(local_hosts_.add(host));
        return host.id();
    }

    QStringList hostNamesOfGroup(qint64 group_id)
    {
        QStringList names;
        for (const LocalHostConfig& host : localHostList(group_id))
            names.append(host.name());
        names.sort();
        return names;
    }

    QList<LocalHostConfig> localHostList(qint64 group_id)
    {
        QList<LocalHostConfig> hosts;
        EXPECT_EQ(local_hosts_.list(group_id, &hosts), Database::ReadResult::OK);
        return hosts;
    }

    QList<LocalHostConfig> allLocalHosts()
    {
        QList<LocalHostConfig> hosts;
        EXPECT_EQ(local_hosts_.all(&hosts), Database::ReadResult::OK);
        return hosts;
    }

    Database db_;
    CredentialStore credentials_{ db_ };
    ProfileStore profiles_{ db_ };
    LocalGroupStore local_groups_{ db_ };
    LocalHostStore local_hosts_{ db_ };
    RouterStore router_store_{ db_ };
    RouterHostStore router_hosts_{ db_ };
    SettingsStore settings_store_{ db_ };
    SyncStore sync_store_{ db_ };
    UnlockStore unlock_store_{ db_ };
    QString file_path_;

private:
    QTemporaryDir dir_;
};

#endif // CLIENT_STORAGE_DATABASE_TEST_BASE_H
