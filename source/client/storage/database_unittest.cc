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

#include "client/storage/database.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QThread>

#include "base/serialization.h"
#include "base/crypto/os_crypt.h"
#include "client/router/router_test_fixture.h"
#include "client/storage/database_test_base.h"
#include "client/storage/master_password.h"
#include "proto/desktop_control.h"
#include "proto/storage.h"

namespace {

const char kUserGuid[] = "4f0c1c2e-6a5b-4b0e-9f3a-1d2c3b4a5e6f";

} // namespace

class DatabaseTest : public DatabaseTestBase
{
protected:
    // Spoils the sealed text of the router the way corruptRecordData() does for a record of a table. The
    // router is kept among the settings.
    bool corruptRouterData()
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        return raw.exec("UPDATE settings SET value='AA==' WHERE name='router'");
    }

    bool eraseCredential(const QString& guid)
    {
        SyncTransaction transaction(db_);
        return transaction.begin(syncStorageId()) == Database::SyncResult::DONE &&
               transaction.eraseRecord(proto::router::SYNC_DATA_TYPE_CREDENTIALS, guid) && transaction.commit();
    }

    bool setReceivedCredentialsRevision(qint64 revision)
    {
        SyncTransaction transaction(db_);
        return transaction.begin(syncStorageId()) == Database::SyncResult::DONE &&
               transaction.setReceivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS, revision) &&
               transaction.commit();
    }

    bool eraseProfile(const QString& guid)
    {
        SyncTransaction transaction(db_);
        return transaction.begin(syncStorageId()) == Database::SyncResult::DONE &&
               transaction.eraseRecord(proto::router::SYNC_DATA_TYPE_PROFILES, guid) && transaction.commit();
    }

    bool setReceivedProfilesRevision(qint64 revision)
    {
        SyncTransaction transaction(db_);
        return transaction.begin(syncStorageId()) == Database::SyncResult::DONE &&
               transaction.setReceivedRevision(proto::router::SYNC_DATA_TYPE_PROFILES, revision) &&
               transaction.commit();
    }
};

//--------------------------------------------------------------------------------------------------
// A move that finds no record writes nothing, and the answer must say so: the caller that is told
// the move went through leaves the record on screen in a group the database never put it in.
TEST_F(DatabaseTest, MoveOfARecordThatIsNotThereIsRefused)
{
    const qint64 group = addGroup("group", 0);
    const qint64 other = addGroup("other", 0);
    const qint64 entry_id = addHost("host", group);

    ASSERT_TRUE(local_hosts_.remove(entry_id));
    EXPECT_FALSE(local_hosts_.move(entry_id, other));

    ASSERT_TRUE(local_groups_.remove(group));
    EXPECT_FALSE(local_groups_.move(group, other));
}

//--------------------------------------------------------------------------------------------------
// Changing the master password rewrites every record under the new key. The record itself did not
// change, so the moment it was last edited must survive: it is a column of the list the user reads.
TEST_F(DatabaseTest, ReencryptionKeepsTheMomentARecordWasEdited)
{
    const qint64 group = addGroup("group", 0);
    const qint64 entry_id = addHost("host", group);

    const qint64 modify_time = findLocalHost(db_, entry_id)->modifyTime();

    // The stamp counts whole seconds, so a rewrite within the same second is indistinguishable from
    // one that left it alone.
    QThread::msleep(1100);

    ASSERT_EQ(reencrypt(db_, DataCryptor::instance().key(), "salt", "verifier"), Database::ReencryptResult::DONE);

    EXPECT_EQ(findLocalHost(db_, entry_id)->modifyTime(), modify_time);
}

//--------------------------------------------------------------------------------------------------
// A check against the router and a change of the master password write a record without editing it.
TEST_F(DatabaseTest, RewriteThatIsNotAnEditKeepsTheMomentOfTheLastOne)
{
    const HostId host_id = 100500;
    const qint64 credential_id = addCredential("name", "user", "secret");
    addRouterHost(host_id, "host-user", "host-secret");

    ASSERT_TRUE(setModifyTime("credentials", "id", credential_id, 1));
    ASSERT_TRUE(setModifyTime("router_hosts", "host_id", host_id, 1));

    ASSERT_TRUE(router_hosts_.updateCheckTime(host_id));
    ASSERT_EQ(reencrypt(db_, DataCryptor::instance().key(), "salt", "verifier"), Database::ReencryptResult::DONE);

    EXPECT_EQ(modifyTime("credentials", "id", credential_id), 1);
    EXPECT_EQ(modifyTime("router_hosts", "host_id", host_id), 1);

    const std::optional<RouterHostConfig> host = findRouterHost(db_, host_id);
    ASSERT_TRUE(host.has_value());
    EXPECT_EQ(host->password().toString(), QString("host-secret"));
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseTest, DatabaseIsIdentifiedByTheNameOfItsFile)
{
    EXPECT_EQ(db_.guid(), QString("client"));

    const QString guid = "7b0f1d18-0e3a-4a0e-9a4e-2a1c6a0f0c11";

    Database other;
    ASSERT_TRUE(other.open(QFileInfo(file_path_).absolutePath() + '/' + guid + ".db3"));
    EXPECT_EQ(other.guid(), guid);
}

//--------------------------------------------------------------------------------------------------
// Only the files named by a guid are databases, and the list tells them apart before any is unlocked.
TEST_F(DatabaseTest, DatabasesOfTheDirectoryAreListed)
{
    const QDir dir = QFileInfo(file_path_).dir();

    const QString first_id = "11111111-1111-4111-8111-111111111111";
    const QString damaged_id = "22222222-2222-4222-8222-222222222222";
    const QString second_id = "33333333-3333-4333-8333-333333333333";

    Database first;
    ASSERT_TRUE(first.open(dir.filePath(first_id + ".db3")));
    ASSERT_TRUE(SettingsStore(first).setName("First"));

    Database second;
    ASSERT_TRUE(second.open(dir.filePath(second_id + ".db3")));
    ASSERT_TRUE(SettingsStore(second).setName("Second"));
    ASSERT_TRUE(UnlockStore(second).setAutoUnlockBlob("blob"));
    ASSERT_EQ(reencrypt(second, DataCryptor::instance().key(), "salt", "verifier"),
              Database::ReencryptResult::DONE);

    for (const QString& file_name : { QString(damaged_id + ".db3"), QString("notes.db3"), QString("notes.txt") })
    {
        QFile file(dir.filePath(file_name));
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        ASSERT_EQ(file.write("data"), 4);
    }

    DatabaseTestPeer::setFilePath(file_path_);

    const QList<Database::Info> list = Database::list();
    ASSERT_EQ(list.size(), 3);

    EXPECT_EQ(list[0].id, first_id);
    EXPECT_EQ(list[0].name, QString("First"));
    EXPECT_FALSE(list[0].auto_unlock);
    EXPECT_FALSE(list[0].has_password);

    EXPECT_EQ(list[1].id, damaged_id);
    EXPECT_TRUE(list[1].name.isEmpty());
    EXPECT_TRUE(list[1].has_password);

    EXPECT_EQ(list[2].id, second_id);
    EXPECT_EQ(list[2].name, QString("Second"));
    EXPECT_TRUE(list[2].auto_unlock);
    EXPECT_TRUE(list[2].has_password);
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseTest, RemovedDatabaseIsNoLongerListed)
{
    const QString id = "11111111-1111-4111-8111-111111111111";
    const QString file_path = QFileInfo(file_path_).dir().filePath(id + ".db3");

    DatabaseTestPeer::setFilePath(file_path);
    ASSERT_TRUE(SettingsStore().setName("First"));
    ASSERT_EQ(Database::list().size(), 1);

    ASSERT_TRUE(Database::remove(id));
    EXPECT_FALSE(QFileInfo::exists(file_path));
    EXPECT_TRUE(Database::list().isEmpty());
    EXPECT_TRUE(Database::filePath().isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseTest, ConnectionFollowsTheSelectedDatabase)
{
    DatabaseTestPeer::setFilePath(file_path_);
    ASSERT_TRUE(SettingsStore().setName("First"));

    Database::select("other");
    EXPECT_EQ(Database::filePath(), QFileInfo(file_path_).dir().filePath("other.db3"));
    EXPECT_TRUE(SettingsStore().name().isEmpty());

    Database::select("client");
    EXPECT_EQ(SettingsStore().name(), QString("First"));
}

//--------------------------------------------------------------------------------------------------
// A host and a router number their fields the same way, so a router column would parse as a host one
// and hand out an address that was never a host. The seal names the table it was made for, and a
// column carried across tables does not open.
TEST_F(DatabaseTest, RouterDataDoesNotOpenAsHostData)
{
    RouterConfig router;
    router.setAddress("router.example.com");
    router.setUsername("router-user");
    router.setPassword(SecureString("router-secret"));

    std::optional<QByteArray> sealed = router.encryptedData();
    ASSERT_TRUE(sealed.has_value());

    LocalHostConfig host;
    EXPECT_FALSE(host.setEncryptedData(*sealed));

    EXPECT_TRUE(host.address().isEmpty());
    EXPECT_TRUE(host.username().isEmpty());
    EXPECT_TRUE(host.password().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A column rewritten by anyone without the key does not turn into different fields, it stops being
// readable at all.
TEST_F(DatabaseTest, TamperedDataDoesNotOpen)
{
    LocalHostConfig host;
    host.setAddress("192.168.0.1");
    host.setUsername("user");
    host.setPassword(SecureString("secret"));

    std::optional<QByteArray> sealed = host.encryptedData();
    ASSERT_TRUE(sealed.has_value());

    QByteArray tampered = *sealed;
    tampered[tampered.size() - 1] = static_cast<char>(tampered[tampered.size() - 1] ^ 0x01);

    LocalHostConfig target;
    EXPECT_FALSE(target.setEncryptedData(tampered));
    EXPECT_TRUE(target.address().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A list that was not read at all must not pass for a book without records.
TEST_F(DatabaseTest, ListOfAClosedDatabaseFails)
{
    Database closed;

    QList<LocalHostConfig> hosts;
    EXPECT_EQ(LocalHostStore(closed).all(&hosts), Database::ReadResult::FAILED);
    EXPECT_TRUE(hosts.isEmpty());

    QList<CredentialConfig> credentials;
    EXPECT_EQ(CredentialStore(closed).list(&credentials), Database::ReadResult::FAILED);
    EXPECT_TRUE(credentials.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The record is handed over with FOUND and only with it. A caller that asks again with the same
// variable must not get the record of the previous search back.
TEST_F(DatabaseTest, SearchThatFoundNothingLeavesTheOutputEmpty)
{
    const qint64 group_id = addGroup("group", 0);
    const qint64 entry_id = addHost("host", group_id);
    addRouter();
    addRouterHost(100500, "user", "secret");
    const qint64 credential_id = addCredential("office", "user", "secret");

    LocalHostConfig host;
    ASSERT_EQ(local_hosts_.find(entry_id, &host), Database::FindResult::FOUND);
    EXPECT_EQ(local_hosts_.find(entry_id + 1000, &host), Database::FindResult::NOT_FOUND);
    EXPECT_EQ(host.id(), -1);
    EXPECT_TRUE(host.name().isEmpty());
    EXPECT_TRUE(host.guid().isEmpty());

    LocalGroupConfig group;
    ASSERT_EQ(local_groups_.find(group_id, &group), Database::FindResult::FOUND);
    EXPECT_EQ(local_groups_.find(group_id + 1000, &group), Database::FindResult::NOT_FOUND);
    EXPECT_EQ(group.id(), -1);
    EXPECT_TRUE(group.name().isEmpty());

    Database without_router;
    ASSERT_TRUE(without_router.open(QFileInfo(file_path_).absolutePath() + "/without_router.db3"));

    RouterConfig router;
    ASSERT_EQ(router_store_.find(&router), Database::FindResult::FOUND);
    EXPECT_EQ(RouterStore(without_router).find(&router), Database::FindResult::NOT_FOUND);
    EXPECT_TRUE(router.address().isEmpty());
    EXPECT_TRUE(router.guid().isEmpty());

    RouterHostConfig router_host;
    ASSERT_EQ(router_hosts_.find(100500, &router_host), Database::FindResult::FOUND);
    EXPECT_EQ(router_hosts_.find(100501, &router_host), Database::FindResult::NOT_FOUND);
    EXPECT_EQ(router_host.hostId(), kInvalidHostId);
    EXPECT_TRUE(router_host.username().isEmpty());

    const std::optional<CredentialConfig> credential_record = findCredential(db_, credential_id);
    ASSERT_TRUE(credential_record.has_value());

    CredentialConfig credential;
    ASSERT_EQ(credentials_.find(credential_id, &credential), Database::FindResult::FOUND);
    EXPECT_EQ(credentials_.find(credential_id + 1000, &credential), Database::FindResult::NOT_FOUND);
    EXPECT_EQ(credential.id(), -1);
    EXPECT_TRUE(credential.displayName().isEmpty());

    ASSERT_EQ(credentials_.findByGuid(credential_record->guid(), &credential),
              Database::FindResult::FOUND);
    EXPECT_EQ(credentials_.findByGuid("no-such-guid", &credential),
              Database::FindResult::NOT_FOUND);
    EXPECT_EQ(credential.id(), -1);
    EXPECT_TRUE(credential.displayName().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A record that did not open still hands over what lies outside its sealed column: the caller can
// name it, keep its links and write it anew instead of losing the row. What was in the sealed
// column comes back empty, even when the same variable held a whole record a moment ago.
TEST_F(DatabaseTest, SearchOfARecordThatDoesNotOpenKeepsWhatWasRead)
{
    const qint64 group_id = addGroup("group", 0);
    const qint64 readable_entry_id = addHost("readable", group_id);
    const qint64 broken_entry_id = addHost("broken", group_id);
    const qint64 readable_credential_id = addCredential("home", "user", "secret");
    const qint64 broken_credential_id = addCredential("office", "user", "secret");
    addRouter();

    LocalHostConfig linked;
    ASSERT_EQ(local_hosts_.find(broken_entry_id, &linked), Database::FindResult::FOUND);
    linked.setCredentialId(broken_credential_id);
    ASSERT_TRUE(local_hosts_.modify(linked));

    addRouterHost(100501, "user-1", "secret");

    RouterHostConfig broken_router_host = routerHost(100500, "user-2", "secret");
    broken_router_host.setCredentialId(broken_credential_id);
    ASSERT_TRUE(router_hosts_.add(broken_router_host));

    ASSERT_TRUE(corruptRecordData("local_hosts", broken_entry_id));
    ASSERT_TRUE(corruptRecordData("credentials", broken_credential_id));
    ASSERT_TRUE(corruptRouterData());
    ASSERT_TRUE(corruptRouterHostData(100500));

    LocalHostConfig host;
    ASSERT_EQ(local_hosts_.find(readable_entry_id, &host), Database::FindResult::FOUND);
    ASSERT_FALSE(host.address().isEmpty());

    EXPECT_EQ(local_hosts_.find(broken_entry_id, &host), Database::FindResult::UNREADABLE);
    EXPECT_EQ(host.id(), broken_entry_id);
    EXPECT_EQ(host.name(), QString("broken"));
    EXPECT_EQ(host.groupId(), group_id);
    EXPECT_EQ(host.credentialId(), broken_credential_id);
    EXPECT_FALSE(host.guid().isEmpty());
    EXPECT_TRUE(host.address().isEmpty());
    EXPECT_TRUE(host.username().isEmpty());

    RouterHostConfig router_host;
    ASSERT_EQ(router_hosts_.find(100501, &router_host), Database::FindResult::FOUND);
    ASSERT_FALSE(router_host.username().isEmpty());

    EXPECT_EQ(router_hosts_.find(100500, &router_host), Database::FindResult::UNREADABLE);
    EXPECT_EQ(router_host.hostId(), 100500U);
    EXPECT_EQ(router_host.credentialId(), broken_credential_id);
    EXPECT_TRUE(router_host.username().isEmpty());

    CredentialConfig credential;
    ASSERT_EQ(credentials_.find(readable_credential_id, &credential), Database::FindResult::FOUND);
    ASSERT_FALSE(credential.username().isEmpty());

    EXPECT_EQ(credentials_.find(broken_credential_id, &credential), Database::FindResult::UNREADABLE);
    EXPECT_EQ(credential.id(), broken_credential_id);
    EXPECT_EQ(credential.displayName(), QString("office"));
    EXPECT_TRUE(credential.username().isEmpty());

    RouterConfig router;
    EXPECT_EQ(router_store_.find(&router), Database::FindResult::UNREADABLE);
    EXPECT_FALSE(router.guid().isEmpty());
    EXPECT_TRUE(router.address().isEmpty());
    EXPECT_TRUE(router.username().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Changing the master password reseals every record. The fields must read the same afterwards,
// under the key the new password derives.
TEST_F(DatabaseTest, ReencryptionKeepsDataReadable)
{
    const qint64 group = addGroup("group", 0);

    LocalHostConfig host;
    host.setName("host");
    host.setGroupId(group);
    host.setAddress("192.168.0.1");
    host.setUsername("user");
    host.setPassword(SecureString("secret"));
    ASSERT_TRUE(local_hosts_.add(host));

    ASSERT_EQ(reencrypt(db_, SecureByteArray(Random::byteArray(32)), "salt", "verifier"),
              Database::ReencryptResult::DONE);

    std::optional<LocalHostConfig> stored = findLocalHost(db_, host.id());
    ASSERT_TRUE(stored.has_value());

    EXPECT_EQ(stored->address(), QString("192.168.0.1"));
    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// A change of the master password rewrites every record of a router. The device token rides the
// rewrite untouched, whether or not the keystore that wrapped it would open it right now.
TEST_F(DatabaseTest, ReencryptionKeepsTheDeviceToken)
{
    addRouter();

    std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    stored->setDeviceToken("wrapped-elsewhere");
    ASSERT_TRUE(router_store_.save(*stored));

    ASSERT_EQ(reencrypt(db_, SecureByteArray(Random::byteArray(32)), "salt", "verifier"),
              Database::ReencryptResult::DONE);

    const std::optional<RouterConfig> reread = findRouter(db_);
    ASSERT_TRUE(reread.has_value());
    EXPECT_EQ(reread->deviceToken(), QByteArray("wrapped-elsewhere"));
}

//--------------------------------------------------------------------------------------------------
// The key of the synchronized data and the storage it belongs to are kept in the record of the
// router, so a change of the master password reseals them with it.
TEST_F(DatabaseTest, SyncStorageKeyIsKeptInTheRecordOfTheRouter)
{
    addRouter(true);

    std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->syncStorageId().isEmpty());
    EXPECT_TRUE(stored->syncStorageKey().isEmpty());

    const QByteArray storage_id = Random::byteArray(16);
    const SecureByteArray key(Random::byteArray(32));

    ASSERT_EQ(router_store_.setSyncStorage(*stored, storage_id, key, false), Database::SyncResult::DONE);

    const std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());
    EXPECT_EQ(router->syncStorageId(), storage_id);
    EXPECT_EQ(router->syncStorageKey(), key);

    ASSERT_EQ(reencrypt(db_, SecureByteArray(Random::byteArray(32)), "salt", "verifier"),
              Database::ReencryptResult::DONE);

    const std::optional<RouterConfig> reread = findRouter(db_);
    ASSERT_TRUE(reread.has_value());
    EXPECT_EQ(reread->syncStorageId(), storage_id);
    EXPECT_EQ(reread->syncStorageKey(), key);
}

//--------------------------------------------------------------------------------------------------
// The storage belongs to the user of the router, and the router tells who the user is when the
// device signs in. Whoever saves the record knows only the address and the name, and both may
// change while the user stays the same. So a record saved for another address or name keeps the
// storage, its key and everything the device knows of the records.
TEST_F(DatabaseTest, SyncStorageIsKeptWhenTheAccountIsEdited)
{
    addRouter(true);

    const qint64 kept_id = addCredential("kept", "user", "secret");
    const qint64 removed_id = addCredential("removed", "user", "secret");

    const QByteArray storage_id = Random::byteArray(16);
    const SecureByteArray key(Random::byteArray(32));
    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), storage_id, key, false), Database::SyncResult::DONE);

    ASSERT_TRUE(markSynced("credentials", kept_id, 7));
    ASSERT_TRUE(markSynced("credentials", removed_id, 8));
    ASSERT_TRUE(credentials_.remove(removed_id));
    ASSERT_TRUE(setReceivedCredentialsRevision(12));

    std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());

    router->setAddress("moved.example.com");
    router->setUsername("another-user");
    ASSERT_TRUE(router_store_.save(*router));

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->address(), QString("moved.example.com"));
    EXPECT_EQ(stored->username(), QString("another-user"));
    EXPECT_EQ(stored->syncStorageId(), storage_id);
    EXPECT_EQ(stored->syncStorageKey(), key);

    EXPECT_EQ(rowCount("credentials"), 2);
    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 12);

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_TRUE(unsynced[0].deleted);
    EXPECT_EQ(unsynced[0].sync_revision, 8);
}

//--------------------------------------------------------------------------------------------------
// Synchronization turned off drops the storage and keeps the guid of the user for turning it on again.
TEST_F(DatabaseTest, SyncTurnedOffAndOnAgainKeepsTheGuidOfTheUser)
{
    addRouter(true);

    std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());

    router->setUserGuid(kUserGuid);
    ASSERT_TRUE(router_store_.save(*router));

    const qint64 kept_id = addCredential("kept", "user", "secret");
    const qint64 removed_id = addCredential("removed", "user", "secret");

    const QByteArray storage_id = Random::byteArray(16);
    const SecureByteArray key(Random::byteArray(32));
    ASSERT_EQ(router_store_.setSyncStorage(*router, storage_id, key, false), Database::SyncResult::DONE);
    ASSERT_TRUE(router_store_.setSyncStorageResetPending(*router));

    ASSERT_TRUE(markSynced("credentials", kept_id, 7));
    ASSERT_TRUE(markSynced("credentials", removed_id, 8));
    ASSERT_TRUE(credentials_.remove(removed_id));
    ASSERT_TRUE(setReceivedCredentialsRevision(12));

    ASSERT_TRUE(router_store_.save(*router));

    std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->syncStorageId(), storage_id);
    EXPECT_EQ(stored->syncStorageKey(), key);
    EXPECT_TRUE(stored->isSyncStorageResetPending());
    EXPECT_EQ(rowCount("credentials"), 2);

    RouterConfig turned_off = *router;
    turned_off.setUserGuid(QString());
    turned_off.setSyncEnabled(false);
    ASSERT_TRUE(router_store_.save(turned_off));

    stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->isSyncEnabled());
    EXPECT_EQ(stored->userGuid(), QString(kUserGuid));
    EXPECT_TRUE(stored->syncStorageId().isEmpty());
    EXPECT_TRUE(stored->syncStorageKey().isEmpty());
    EXPECT_FALSE(stored->isSyncStorageResetPending());

    EXPECT_EQ(rowCount("credentials"), 1);
    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 0);

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, 0);

    RouterConfig edited = *stored;
    edited.setUserGuid(QString());
    edited.setSyncEnabled(true);
    ASSERT_TRUE(router_store_.save(edited));

    stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->isSyncEnabled());
    EXPECT_EQ(stored->userGuid(), QString(kUserGuid));
}

//--------------------------------------------------------------------------------------------------
// Synchronization turned off detaches the device from the storage. The record keeps neither the
// storage nor its key, and the records of the device become new to whatever storage comes next.
TEST_F(DatabaseTest, SyncStorageIsDroppedWhenSyncIsTurnedOff)
{
    addRouter(true);

    const qint64 kept_id = addCredential("kept", "user", "secret");
    const qint64 removed_id = addCredential("removed", "user", "secret");

    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DONE);

    ASSERT_TRUE(markSynced("credentials", kept_id, 7));
    ASSERT_TRUE(markSynced("credentials", removed_id, 8));
    ASSERT_TRUE(credentials_.remove(removed_id));
    ASSERT_TRUE(setReceivedCredentialsRevision(12));

    std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());

    router->setSyncEnabled(false);
    ASSERT_TRUE(router_store_.save(*router));

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->isSyncEnabled());
    EXPECT_TRUE(stored->syncStorageId().isEmpty());
    EXPECT_TRUE(stored->syncStorageKey().isEmpty());

    EXPECT_EQ(rowCount("credentials"), 1);
    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 0);

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, 0);
}

//--------------------------------------------------------------------------------------------------
// A default profile whose data does not read stands for the built-in parameters, and it is still
// the row it was: a save writes the row over, and the next read gives what was saved.
TEST_F(DatabaseTest, DamagedDefaultProfileIsRepairedByASave)
{
    const qint64 default_id = profiles_.defaultProfile().id();
    ASSERT_TRUE(corruptRecordData("profiles", default_id));

    ProfileConfig profile = profiles_.defaultProfile();
    EXPECT_EQ(profile.id(), default_id);
    EXPECT_TRUE(profile.isDefault());
    EXPECT_TRUE(profile.name().isEmpty());
    EXPECT_EQ(serialize(profile.desktopConfig()), serialize(defaultDesktopConfig()));
    EXPECT_EQ(profile.udpMethods(), static_cast<quint32>(UDP_METHOD_ALL));

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    ASSERT_TRUE(local_hosts_.add(host));
    EXPECT_EQ(profiles_.hostProfile(HostConfig::forLocalHost(host)).id(), default_id);

    profile.setUdpMethods(UDP_METHOD_DIRECT);
    ASSERT_TRUE(profiles_.modify(profile));

    ProfileConfig repaired;
    ASSERT_EQ(profiles_.find(default_id, &repaired), Database::FindResult::FOUND);
    EXPECT_EQ(repaired.udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));
    EXPECT_EQ(profiles_.defaultProfile().udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));
}

//--------------------------------------------------------------------------------------------------
// A batch replaces the default profile like every other record: a batch without one leaves the
// built-in parameters, a batch with one brings its own, and the hosts that name it land on it.
TEST_F(DatabaseTest, BatchReplacesTheDefaultProfile)
{
    ProfileConfig profile = profiles_.defaultProfile();
    profile.setUdpMethods(UDP_METHOD_UPNP);
    ASSERT_TRUE(profiles_.modify(profile));

    ASSERT_TRUE(db_.import({}, {}, {}, {}, {}, {}));
    EXPECT_GT(profiles_.defaultProfile().id(), 0);
    EXPECT_EQ(profiles_.defaultProfile().udpMethods(), static_cast<quint32>(UDP_METHOD_ALL));

    ProfileConfig imported;
    imported.setId(-1);
    imported.setGuid(ProfileConfig::kDefaultGuid);
    imported.setDesktopConfig(defaultDesktopConfig());
    imported.setUdpMethods(UDP_METHOD_PCP);

    ProfileConfig office;
    office.setId(-2);
    office.setGuid("11111111-2222-3333-4444-555555555555");
    office.setName("office");
    office.setDesktopConfig(defaultDesktopConfig());
    office.setUdpMethods(0);

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setProfileId(-1);

    ASSERT_TRUE(db_.import({}, {}, { host }, {}, {}, { imported, office }));

    const ProfileConfig current = profiles_.defaultProfile();
    EXPECT_GT(current.id(), 0);
    EXPECT_EQ(current.udpMethods(), static_cast<quint32>(UDP_METHOD_PCP));

    const QList<ProfileConfig> profiles = profileList();
    ASSERT_EQ(profiles.size(), 1);
    EXPECT_EQ(profiles.front().name(), QString("office"));

    EXPECT_EQ(allLocalHosts().front().profileId(), current.id());
}

//--------------------------------------------------------------------------------------------------
// The password of the proxy is a secret like the ones of the address book. A change of the master
// password reseals it, and a key that does not open it gets nothing of it.
TEST_F(DatabaseTest, ProxyIsResealedWithTheMasterPassword)
{
    ProxySettings settings;
    settings.enabled = true;
    settings.type = ProxySettings::Type::HTTPS;
    settings.address = "10.0.0.1";
    settings.port = 3128;
    settings.user = "user";
    settings.password = SecureString("secret");

    ProxyConfig proxy;
    proxy.setSettings(settings);
    ASSERT_TRUE(settings_store_.setProxyConfig(proxy));

    const SecureByteArray old_key = DataCryptor::instance().key();

    ASSERT_EQ(reencrypt(db_, SecureByteArray(Random::byteArray(32)), "salt", "verifier"),
              Database::ReencryptResult::DONE);

    ProxyConfig reread;
    ASSERT_EQ(settings_store_.proxyConfig(&reread), Database::ReadResult::OK);
    EXPECT_TRUE(reread.settings().enabled);
    EXPECT_EQ(reread.settings().type, ProxySettings::Type::HTTPS);
    EXPECT_EQ(reread.settings().address, QString("10.0.0.1"));
    EXPECT_EQ(reread.settings().port, 3128);
    EXPECT_EQ(reread.settings().password.toString(), QString("secret"));

    DataCryptor::instance().setKey(old_key);

    ProxyConfig unreadable;
    EXPECT_EQ(settings_store_.proxyConfig(&unreadable), Database::ReadResult::INCOMPLETE);
    EXPECT_FALSE(unreadable.settings().enabled);
    EXPECT_TRUE(unreadable.settings().address.isEmpty());
    EXPECT_TRUE(unreadable.settings().password.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Editing the credentials, and resealing them under a new master password, says nothing about the
// host still being there. Neither may pass for an answer of the router.
TEST_F(DatabaseTest, EditedRouterHostCredentialsKeepTheirCheckTime)
{
    addRouter();

    // One row the router has answered about and one it has not. An edit that moves the check time
    // in either direction shows up as a change of what is offered for checking.
    addRouterHost(100500, "user", "secret");
    addRouterHost(100501, "other-user", "other-secret");

    ASSERT_TRUE(router_hosts_.updateCheckTime(100500));
    ASSERT_EQ(outdatedRouterHosts(), QList<HostId>({ HostId(100501) }));

    ASSERT_TRUE(router_hosts_.modify(routerHost(100500, "edited-user", "edited-secret")));
    ASSERT_TRUE(router_hosts_.modify(routerHost(100501, "third-user", "third-secret")));

    EXPECT_EQ(outdatedRouterHosts(), QList<HostId>({ HostId(100501) }));

    ASSERT_EQ(reencrypt(db_, SecureByteArray(Random::byteArray(32)), "salt", "verifier"),
              Database::ReencryptResult::DONE);

    EXPECT_EQ(outdatedRouterHosts(), QList<HostId>({ HostId(100501) }));
}

//--------------------------------------------------------------------------------------------------
// Changing the master password reseals the credentials of router hosts as well. Left out, they would
// stay under the old key and never open again.
TEST_F(DatabaseTest, ReencryptionKeepsRouterHostCredentialsReadable)
{
    addRouter();
    addRouterHost(100500, "user", "secret");

    ASSERT_EQ(reencrypt(db_, SecureByteArray(Random::byteArray(32)), "salt", "verifier"),
              Database::ReencryptResult::DONE);

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());

    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));
}
//--------------------------------------------------------------------------------------------------
// The device token is a wrap made by the OS keystore, and the record carries it as opaque bytes.
// Re-wrapped on the way in or out, a wrap the keystore refuses to open right now (a record from
// another machine, a locked keychain) would be destroyed by the first rewrite of the record.
TEST_F(DatabaseTest, DeviceTokenRidesTheRecordAsOpaqueBytes)
{
    RouterConfig router;
    router.setAddress("router.example.com");
    router.setUsername("router-user");
    router.setPassword(SecureString("router-secret"));
    router.setDeviceToken("wrapped-elsewhere");

    const std::optional<QByteArray> blob = router.encryptedData();
    ASSERT_TRUE(blob.has_value());

    // The column holds the wrap exactly as the object does.
    const std::optional<QByteArray> plain = DataCryptor::instance().decrypt(*blob, "routers");
    ASSERT_TRUE(plain.has_value());
    proto::storage::RouterBlob data;
    ASSERT_TRUE(parse(*plain, &data));
    EXPECT_EQ(data.device_token(), "wrapped-elsewhere");

    RouterConfig reopened;
    ASSERT_TRUE(reopened.setEncryptedData(*blob));
    EXPECT_EQ(reopened.deviceToken(), QByteArray("wrapped-elsewhere"));
}

//--------------------------------------------------------------------------------------------------
// The bounds of a record are the bounds of what is stored. A name past them would come back cut,
// and half a name is not what the user typed.
TEST_F(DatabaseTest, RecordsPastTheirBoundsAreNotStored)
{
    const QString long_name(LocalHostConfig::kMaxNameLength + 1, QLatin1Char('a'));
    const QString long_comment(LocalHostConfig::kMaxCommentLength + 1, QLatin1Char('a'));

    LocalGroupConfig group;
    group.setName(long_name);
    EXPECT_FALSE(local_groups_.add(group));

    LocalGroupConfig unnamed;
    EXPECT_FALSE(local_groups_.add(unnamed));

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setComment(long_comment);
    EXPECT_FALSE(local_hosts_.add(host));

    // An address is what the host is reached at, and there is no host without one.
    LocalHostConfig without_address;
    without_address.setName("host");
    without_address.setUsername("user");
    without_address.setPassword(SecureString(QString("secret")));
    EXPECT_FALSE(local_hosts_.add(without_address));

    EXPECT_TRUE(allLocalGroups().isEmpty());
    EXPECT_TRUE(allLocalHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A batch is written whole. The records name each other by their place in it, because the ids they
// will have do not exist until they are written.
TEST_F(DatabaseTest, BatchIsWrittenWithItsOwnLinks)
{
    RouterConfig router;
    router.setAddress("router.example.com");
    router.setUsername("router-user");
    router.setPassword(SecureString(QString("router-secret")));

    LocalGroupConfig parent;
    parent.setId(-1);
    parent.setName("parent");

    LocalGroupConfig child;
    child.setId(-2);
    child.setParentId(-1);
    child.setName("child");

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("100500");
    host.setGroupId(-2);

    RouterHostConfig credentials;
    credentials.setHostId(100500);
    credentials.setUsername("user");
    credentials.setPassword(SecureString(QString("secret")));

    ASSERT_TRUE(db_.import(router, { parent, child }, { host }, { credentials }, {}, {}));

    ASSERT_TRUE(findRouter(db_).has_value());

    const QList<LocalGroupConfig> groups = allLocalGroups();
    ASSERT_EQ(groups.size(), 2);

    // Listed by name: "child" comes before "parent".
    const std::optional<LocalGroupConfig> stored_parent = findLocalGroup(db_, groups.back().id());
    const std::optional<LocalGroupConfig> stored_child = findLocalGroup(db_, groups.front().id());
    ASSERT_TRUE(stored_parent.has_value() && stored_child.has_value());

    EXPECT_EQ(stored_parent->parentId(), 0);
    EXPECT_EQ(stored_child->parentId(), stored_parent->id());

    const QList<LocalHostConfig> hosts = allLocalHosts();
    ASSERT_EQ(hosts.size(), 1);
    EXPECT_EQ(hosts.front().groupId(), stored_child->id());

    const QList<RouterHostConfig> stored_credentials = allRouterHosts();
    ASSERT_EQ(stored_credentials.size(), 1);
    EXPECT_EQ(stored_credentials.front().hostId(), 100500U);
}

//--------------------------------------------------------------------------------------------------
// A record of the batch that cannot be written takes the whole batch with it. Half a batch is not
// an address book anyone asked for, and nothing says which half arrived.
TEST_F(DatabaseTest, BatchWithARecordThatCannotBeWrittenLeavesNothingBehind)
{
    addGroup("already here", 0);

    LocalGroupConfig group;
    group.setId(-1);
    group.setName("group");

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setGroupId(-1);

    // Half a pair of credentials, which the host writer refuses.
    LocalHostConfig broken;
    broken.setName("broken");
    broken.setAddress("192.168.0.2");
    broken.setUsername("user");
    broken.setGroupId(-1);

    EXPECT_FALSE(db_.import({}, { group }, { host, broken }, {}, {}, {}));

    EXPECT_EQ(allLocalGroups().size(), 1);
    EXPECT_TRUE(allLocalHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The row of a host of a router stays as long as it holds something for the host. Removed
// credentials take only themselves, and what else is kept for the host stays with the row.
TEST_F(DatabaseTest, RouterHostRowOutlivesItsCredentials)
{
    addRouter();
    const qint64 credential_id = addCredential("office", "user", "secret");

    CredentialConfig os = credential("workstation", "DOMAIN\\user", "secret");
    os.setType(CredentialConfig::Type::OS);
    ASSERT_TRUE(credentials_.add(os));

    RouterHostConfig host = routerHost(100500, "own-user", "own-secret");
    host.setCredentialId(credential_id);
    ASSERT_TRUE(router_hosts_.add(host));
    ASSERT_TRUE(router_hosts_.setOsCredentialId(100500, os.id()));

    ASSERT_TRUE(router_hosts_.modify(routerHost(100500, QString(), QString())));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->username().isEmpty());
    EXPECT_TRUE(stored->password().isEmpty());
    EXPECT_EQ(stored->credentialId(), 0);
    EXPECT_EQ(stored->osCredentialId(), os.id());

    EXPECT_EQ(reencrypt(db_, DataCryptor::instance().key(), QByteArray(32, 's'), QByteArray(32, 'v')),
              Database::ReencryptResult::DONE);

    EXPECT_FALSE(router_hosts_.modify(routerHost(100501, QString(), QString())));
    EXPECT_EQ(allRouterHosts().size(), 1);

    ASSERT_TRUE(router_hosts_.remove(100500));
    EXPECT_TRUE(allRouterHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A removed profile leaves every host that had it on the default profile. The row of a router host
// that held nothing but the profile is gone with it.
TEST_F(DatabaseTest, RemovedProfileLeavesItsHostsOnTheDefaultOne)
{
    addRouter();
    const qint64 profile_id = addProfile("office");

    ASSERT_TRUE(router_hosts_.setProfileId(100500, profile_id));

    LocalHostConfig local_host;
    local_host.setName("host");
    local_host.setAddress("192.168.0.1");
    local_host.setProfileId(profile_id);
    ASSERT_TRUE(local_hosts_.add(local_host));

    ASSERT_TRUE(profiles_.remove(profile_id));

    EXPECT_FALSE(findRouterHost(db_, 100500).has_value());
    EXPECT_EQ(findLocalHost(db_, local_host.id())->profileId(), 0);
}

//--------------------------------------------------------------------------------------------------
// A session runs on the parameters of the profile chosen for its host. A host without a profile,
// a host that is not in the book and a host whose profile is gone all run on the default ones.
TEST_F(DatabaseTest, SessionRunsOnTheProfileOfItsHost)
{
    proto::control::Config office_desktop = defaultDesktopConfig();
    office_desktop.set_audio(false);

    ProfileConfig office;
    office.setName("office");
    office.setDesktopConfig(office_desktop);
    office.setUdpMethods(UDP_METHOD_DIRECT);
    ASSERT_TRUE(profiles_.add(office));

    proto::control::Config default_desktop = defaultDesktopConfig();
    default_desktop.set_clipboard(false);
    ProfileConfig default_profile = profiles_.defaultProfile();
    default_profile.setDesktopConfig(default_desktop);
    default_profile.setUdpMethods(UDP_METHOD_UPNP);
    ASSERT_TRUE(profiles_.modify(default_profile));

    LocalHostConfig local_host;
    local_host.setName("host");
    local_host.setAddress("192.168.0.1");
    local_host.setProfileId(office.id());
    ASSERT_TRUE(local_hosts_.add(local_host));

    ProfileConfig profile = profiles_.hostProfile(HostConfig::forLocalHost(local_host));
    EXPECT_EQ(profile.id(), office.id());
    EXPECT_EQ(serialize(profile.desktopConfig()), serialize(office_desktop));
    EXPECT_EQ(profile.udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));

    addRouter();
    ASSERT_TRUE(router_hosts_.setProfileId(100500, office.id()));

    profile = profiles_.hostProfile(HostConfig::forRouterHost(100500, "host"));
    EXPECT_EQ(profile.id(), office.id());
    EXPECT_EQ(profile.udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));

    profile = profiles_.hostProfile(HostConfig::forRouterHost(100501, "other"));
    EXPECT_EQ(profile.id(), default_profile.id());
    EXPECT_EQ(serialize(profile.desktopConfig()), serialize(default_desktop));
    EXPECT_EQ(profile.udpMethods(), static_cast<quint32>(UDP_METHOD_UPNP));

    HostConfig quick_connect;
    quick_connect.setAddress("192.168.0.2");
    profile = profiles_.hostProfile(quick_connect);
    EXPECT_EQ(profile.id(), default_profile.id());
    EXPECT_EQ(profile.udpMethods(), static_cast<quint32>(UDP_METHOD_UPNP));

    ASSERT_TRUE(profiles_.remove(office.id()));

    profile = profiles_.hostProfile(HostConfig::forLocalHost(local_host));
    EXPECT_EQ(profile.id(), default_profile.id());
    EXPECT_EQ(serialize(profile.desktopConfig()), serialize(default_desktop));
    EXPECT_EQ(profile.udpMethods(), static_cast<quint32>(UDP_METHOD_UPNP));
}

//--------------------------------------------------------------------------------------------------
// The hosts of a batch name their profiles by the keys of the batch as well.
TEST_F(DatabaseTest, BatchLinksHostsToTheirProfiles)
{
    ProfileConfig profile;
    profile.setId(-1);
    profile.setName("office");
    profile.setDesktopConfig(defaultDesktopConfig());

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setProfileId(-1);

    RouterConfig router;
    router.setAddress("router.example.com");
    router.setUsername("router-user");
    router.setPassword(SecureString(QString("router-secret")));

    RouterHostConfig router_host = routerHost(100500, QString(), QString());
    router_host.setProfileId(-1);

    ASSERT_TRUE(db_.import(router, {}, { host }, { router_host }, {}, { profile }));

    const QList<ProfileConfig> profiles = profileList();
    ASSERT_EQ(profiles.size(), 1);

    const QList<LocalHostConfig> hosts = allLocalHosts();
    ASSERT_EQ(hosts.size(), 1);
    EXPECT_EQ(hosts.front().profileId(), profiles.front().id());

    const QList<RouterHostConfig> router_hosts = allRouterHosts();
    ASSERT_EQ(router_hosts.size(), 1);
    EXPECT_EQ(router_hosts.front().profileId(), profiles.front().id());
}

//--------------------------------------------------------------------------------------------------
// A change of the master password rewrites the credentials along with everything else.
TEST_F(DatabaseTest, ReencryptionKeepsCredentialsReadable)
{
    const qint64 credential_id = addCredential("office", "user", "secret");

    ASSERT_EQ(reencrypt(db_, SecureByteArray(Random::byteArray(32)), "salt", "verifier"),
              Database::ReencryptResult::DONE);

    const std::optional<CredentialConfig> stored = findCredential(db_, credential_id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// The credentials of a batch are written with it and take the place of the ones the book held.
TEST_F(DatabaseTest, BatchCarriesItsCredentials)
{
    addCredential("old", "user", "secret");

    CredentialConfig config = credential("office", "user", "secret");
    config.setGuid("guid-of-the-file");

    ASSERT_TRUE(db_.import({}, {}, {}, {}, { config }, {}));

    const QList<CredentialConfig> stored = credentialList();
    ASSERT_EQ(stored.size(), 1);
    EXPECT_EQ(stored.front().guid(), QString("guid-of-the-file"));
    EXPECT_EQ(stored.front().displayName(), QString("office"));
    EXPECT_EQ(stored.front().password().toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// The hosts of a batch name the credentials of the batch by their keys, so the credentials are
// written first and the hosts come out linked to the ids they were given.
TEST_F(DatabaseTest, BatchLinksHostsToItsCredentials)
{
    CredentialConfig config = credential("office", "user", "secret");
    config.setId(-1);

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setCredentialId(-1);

    RouterConfig router;
    router.setAddress("router.example.com");
    router.setUsername("router-user");
    router.setPassword(SecureString(QString("router-secret")));

    RouterHostConfig router_host = routerHost(100500, "user", "secret");
    router_host.setCredentialId(-1);

    ASSERT_TRUE(db_.import(router, {}, { host }, { router_host }, { config }, {}));

    const QList<CredentialConfig> credentials = credentialList();
    ASSERT_EQ(credentials.size(), 1);

    const QList<LocalHostConfig> hosts = allLocalHosts();
    ASSERT_EQ(hosts.size(), 1);
    EXPECT_EQ(hosts.front().credentialId(), credentials.front().id());

    const QList<RouterHostConfig> router_hosts = allRouterHosts();
    ASSERT_EQ(router_hosts.size(), 1);
    EXPECT_EQ(router_hosts.front().credentialId(), credentials.front().id());
}

//--------------------------------------------------------------------------------------------------
// The operating system credentials last chosen for a host are kept through an edit of the host,
// which does not know about them. A removed record leaves the host without one. A host of a router
// keeps them only in the row of its stored credentials.
TEST_F(DatabaseTest, HostsKeepTheOsCredentialsLastChosenForThem)
{
    CredentialConfig os = credential("workstation", "DOMAIN\\user", "secret");
    os.setType(CredentialConfig::Type::OS);
    ASSERT_TRUE(credentials_.add(os));

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    ASSERT_TRUE(local_hosts_.add(host));
    EXPECT_EQ(findLocalHost(db_, host.id())->osCredentialId(), 0);

    ASSERT_TRUE(local_hosts_.setOsCredentialId(host.id(), os.id()));
    EXPECT_EQ(findLocalHost(db_, host.id())->osCredentialId(), os.id());

    LocalHostConfig edited;
    edited.setId(host.id());
    edited.setName("renamed");
    edited.setAddress("192.168.0.1");
    ASSERT_TRUE(local_hosts_.modify(edited));
    EXPECT_EQ(findLocalHost(db_, host.id())->osCredentialId(), os.id());

    addRouter();
    ASSERT_TRUE(router_hosts_.add(routerHost(100500, "user", "secret")));

    ASSERT_TRUE(router_hosts_.setOsCredentialId(100500, os.id()));
    ASSERT_TRUE(router_hosts_.modify(routerHost(100500, "other-user", "other-secret")));
    EXPECT_EQ(findRouterHost(db_, 100500)->osCredentialId(), os.id());

    ASSERT_TRUE(router_hosts_.setOsCredentialId(100501, os.id()));
    EXPECT_EQ(allRouterHosts().size(), 1);

    ASSERT_TRUE(credentials_.remove(os.id()));
    EXPECT_EQ(findLocalHost(db_, host.id())->osCredentialId(), 0);
    EXPECT_EQ(findRouterHost(db_, 100500)->osCredentialId(), 0);
}

//--------------------------------------------------------------------------------------------------
// The hosts of a batch name their operating system credentials by the keys of the batch as well.
TEST_F(DatabaseTest, BatchLinksHostsToTheirOsCredentials)
{
    CredentialConfig aspia = credential("office", "user", "secret");
    aspia.setId(-1);

    CredentialConfig os = credential("workstation", "DOMAIN\\user", "secret");
    os.setId(-2);
    os.setType(CredentialConfig::Type::OS);

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setOsCredentialId(-2);

    RouterConfig router;
    router.setAddress("router.example.com");
    router.setUsername("router-user");
    router.setPassword(SecureString(QString("router-secret")));

    RouterHostConfig router_host = routerHost(100500, QString(), QString());
    router_host.setCredentialId(-1);
    router_host.setOsCredentialId(-2);

    ASSERT_TRUE(db_.import(router, {}, { host }, { router_host }, { aspia, os }, {}));

    QList<CredentialConfig> credentials;
    ASSERT_EQ(credentials_.list(&credentials, CredentialConfig::Type::OS), Database::ReadResult::OK);
    ASSERT_EQ(credentials.size(), 1);

    const QList<LocalHostConfig> hosts = allLocalHosts();
    ASSERT_EQ(hosts.size(), 1);
    EXPECT_EQ(hosts.front().osCredentialId(), credentials.front().id());

    const QList<RouterHostConfig> router_hosts = allRouterHosts();
    ASSERT_EQ(router_hosts.size(), 1);
    EXPECT_EQ(router_hosts.front().osCredentialId(), credentials.front().id());
}

//--------------------------------------------------------------------------------------------------
// Once the record of credentials is removed, the hosts that referred to it refer to nothing and
// keep the pair of their own, if any. A row of a router host that was kept by the link alone is
// gone. The book stays one a change of the master password walks through.
TEST_F(DatabaseTest, RemovedRecordLeavesTheHostsThatReferredToIt)
{
    addRouter();
    const qint64 group = addGroup("group", 0);
    const qint64 credential_id = addCredential("office", "user", "secret");
    const qint64 other_id = addCredential("other", "other-user", "other-secret");

    RouterHostConfig link_only = routerHost(100500, QString(), QString());
    link_only.setCredentialId(credential_id);
    ASSERT_TRUE(router_hosts_.add(link_only));

    RouterHostConfig with_pair = routerHost(100501, "own-user", "own-secret");
    with_pair.setCredentialId(credential_id);
    ASSERT_TRUE(router_hosts_.add(with_pair));

    RouterHostConfig other_link = routerHost(100502, QString(), QString());
    other_link.setCredentialId(other_id);
    ASSERT_TRUE(router_hosts_.add(other_link));

    LocalHostConfig local_host;
    local_host.setName("host");
    local_host.setAddress("192.168.0.1");
    local_host.setGroupId(group);
    local_host.setCredentialId(credential_id);
    ASSERT_TRUE(local_hosts_.add(local_host));

    ASSERT_TRUE(credentials_.remove(credential_id));

    EXPECT_FALSE(findRouterHost(db_, 100500).has_value());

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100501);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->credentialId(), 0);
    EXPECT_EQ(stored->username(), QString("own-user"));
    EXPECT_EQ(stored->password().toString(), QString("own-secret"));

    stored = findRouterHost(db_, 100502);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->credentialId(), other_id);

    std::optional<LocalHostConfig> stored_local = findLocalHost(db_, local_host.id());
    ASSERT_TRUE(stored_local.has_value());
    EXPECT_EQ(stored_local->credentialId(), 0);

    EXPECT_EQ(reencrypt(db_, DataCryptor::instance().key(), QByteArray(32, 's'), QByteArray(32, 'v')),
              Database::ReencryptResult::DONE);
}

//--------------------------------------------------------------------------------------------------
// The router learns of a removal from the device, so a removed record it holds keeps its row until
// then. The row is marked as deleted and wiped, and nothing but the exchange sees it. The hosts
// that referred to the record keep their links, which lead to no record, and are entered with
// their own pairs.
TEST_F(DatabaseTest, RemovedCredentialTheRouterHoldsKeepsARowMarkedAsDeleted)
{
    addRouter();
    const qint64 group = addGroup("group", 0);
    const qint64 credential_id = addCredential("office", "user", "secret");
    const qint64 other_id = addCredential("other", "other-user", "other-secret");

    const std::optional<CredentialConfig> stored = findCredential(db_, credential_id);
    ASSERT_TRUE(stored.has_value());

    ASSERT_TRUE(markSynced("credentials", credential_id, 7));
    ASSERT_TRUE(markSynced("credentials", other_id, 8));
    ASSERT_TRUE(setModifyTime("credentials", "id", credential_id, 1));

    RouterHostConfig link_only = routerHost(100500, QString(), QString());
    link_only.setCredentialId(credential_id);
    ASSERT_TRUE(router_hosts_.add(link_only));

    addRouterHost(100501, "own-user", "own-secret");
    ASSERT_TRUE(router_hosts_.setOsCredentialId(100501, credential_id));

    LocalHostConfig local_host;
    local_host.setName("host");
    local_host.setAddress("192.168.0.1");
    local_host.setGroupId(group);
    local_host.setUsername("own-user");
    local_host.setPassword(SecureString("own-secret"));
    local_host.setCredentialId(credential_id);
    ASSERT_TRUE(local_hosts_.add(local_host));

    const qint64 other_host_id = addHost("other-host", group);
    ASSERT_TRUE(local_hosts_.setOsCredentialId(other_host_id, credential_id));

    const qint64 start_time = QDateTime::currentSecsSinceEpoch();
    ASSERT_TRUE(credentials_.remove(credential_id));

    EXPECT_FALSE(findCredential(db_, credential_id).has_value());
    EXPECT_FALSE(findCredentialByGuid(db_, stored->guid()).has_value());

    const QList<CredentialConfig> credentials = credentialList();
    ASSERT_EQ(credentials.size(), 1);
    EXPECT_EQ(credentials[0].id(), other_id);

    EXPECT_TRUE(isMarkedDeleted("credentials", credential_id));

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_TRUE(unsynced[0].deleted);
    EXPECT_EQ(unsynced[0].credential.id(), credential_id);
    EXPECT_EQ(unsynced[0].credential.guid(), stored->guid());
    EXPECT_GE(unsynced[0].credential.modifyTime(), start_time);
    EXPECT_EQ(unsynced[0].sync_revision, 7);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);

    std::optional<RouterHostConfig> router_host = findRouterHost(db_, 100500);
    ASSERT_TRUE(router_host.has_value());
    EXPECT_EQ(router_host->credentialId(), credential_id);

    std::optional<std::pair<QString, SecureString>> entered = router_hosts_.credentials(100500);
    ASSERT_TRUE(entered.has_value());
    EXPECT_TRUE(entered->first.isEmpty());

    router_host = findRouterHost(db_, 100501);
    ASSERT_TRUE(router_host.has_value());
    EXPECT_EQ(router_host->osCredentialId(), credential_id);
    EXPECT_EQ(router_host->username(), QString("own-user"));

    const std::optional<LocalHostConfig> stored_host = findLocalHost(db_, local_host.id());
    ASSERT_TRUE(stored_host.has_value());
    EXPECT_EQ(stored_host->credentialId(), credential_id);

    entered = local_hosts_.credentials(local_host.id());
    ASSERT_TRUE(entered.has_value());
    EXPECT_EQ(entered->first, QString("own-user"));
    EXPECT_EQ(entered->second.toString(), QString("own-secret"));

    const std::optional<LocalHostConfig> other_host = findLocalHost(db_, other_host_id);
    ASSERT_TRUE(other_host.has_value());
    EXPECT_EQ(other_host->osCredentialId(), credential_id);

    // The record is no longer there to be edited, and removing it once more changes nothing.
    CredentialConfig edited = credential("office", "user", "secret");
    edited.setId(credential_id);
    EXPECT_FALSE(credentials_.modify(edited));

    EXPECT_TRUE(credentials_.remove(credential_id));
    EXPECT_EQ(unsyncedCredentials().value(0).sync_dirty, 1);

    EXPECT_EQ(reencrypt(db_, DataCryptor::instance().key(), QByteArray(32, 's'), QByteArray(32, 'v')),
              Database::ReencryptResult::DONE);
}

//--------------------------------------------------------------------------------------------------
// A record that comes back under the guid of a removed one, as an import of a file brings it, is
// the same record to the router. It takes the row of that one over and goes out as one more
// change, and the hosts that referred to the record are entered with it again.
TEST_F(DatabaseTest, CredentialThatComesBackTakesTheRowOfTheRemovedOne)
{
    addRouter();
    const qint64 group = addGroup("group", 0);
    const qint64 credential_id = addCredential("office", "user", "secret");

    const std::optional<CredentialConfig> stored = findCredential(db_, credential_id);
    ASSERT_TRUE(stored.has_value());

    RouterHostConfig link_only = routerHost(100500, QString(), QString());
    link_only.setCredentialId(credential_id);
    ASSERT_TRUE(router_hosts_.add(link_only));

    const qint64 host_id = addHost("host", group);
    ASSERT_TRUE(local_hosts_.setOsCredentialId(host_id, credential_id));

    ASSERT_TRUE(markSynced("credentials", credential_id, 7));
    ASSERT_TRUE(credentials_.remove(credential_id));

    CredentialConfig restored = credential("office", "other-user", "other-secret");
    restored.setGuid(stored->guid());
    ASSERT_TRUE(credentials_.add(restored));
    EXPECT_EQ(restored.id(), credential_id);
    EXPECT_EQ(rowCount("credentials"), 1);

    const std::optional<std::pair<QString, SecureString>> entered = router_hosts_.credentials(100500);
    ASSERT_TRUE(entered.has_value());
    EXPECT_EQ(entered->first, QString("other-user"));
    EXPECT_EQ(entered->second.toString(), QString("other-secret"));

    const std::optional<LocalHostConfig> local_host = findLocalHost(db_, host_id);
    ASSERT_TRUE(local_host.has_value());
    EXPECT_EQ(local_host->osCredentialId(), credential_id);

    const std::optional<CredentialConfig> by_guid = findCredentialByGuid(db_, stored->guid());
    ASSERT_TRUE(by_guid.has_value());
    EXPECT_EQ(by_guid->id(), credential_id);
    EXPECT_EQ(by_guid->username(), QString("other-user"));
    EXPECT_EQ(by_guid->password().toString(), QString("other-secret"));

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_FALSE(unsynced[0].deleted);
    EXPECT_EQ(unsynced[0].sync_revision, 7);
    EXPECT_EQ(unsynced[0].sync_dirty, 2);
}

//--------------------------------------------------------------------------------------------------
// A database made by an earlier version has the tables of credentials and profiles without the column
// that marks a removed record. The column is added when the database is opened, and the records it had are
// not removed ones.
TEST_F(DatabaseTest, DatabaseOfAnEarlierVersionGetsTheDeletedColumn)
{
    const QString file_path = QFileInfo(file_path_).path() + "/earlier.db3";

    CredentialConfig config = credential("office", "user", "secret");
    config.setGuid("11111111-1111-1111-1111-111111111111");

    const std::optional<QByteArray> encrypted_data = config.encryptedData();
    ASSERT_TRUE(encrypted_data.has_value());

    {
        SqlDatabase raw;
        ASSERT_TRUE(raw.open(file_path));
        ASSERT_TRUE(raw.exec("CREATE TABLE \"credentials\" ("
                             "\"id\" INTEGER UNIQUE,"
                             "\"type\" INTEGER NOT NULL DEFAULT 0,"
                             "\"name\" TEXT NOT NULL DEFAULT '',"
                             "\"data\" BLOB DEFAULT X'',"
                             "\"guid\" TEXT NOT NULL UNIQUE,"
                             "\"modify_time\" INTEGER NOT NULL DEFAULT 0,"
                             "\"sync_revision\" INTEGER NOT NULL DEFAULT 0,"
                             "\"sync_dirty\" INTEGER NOT NULL DEFAULT 0,"
                             "PRIMARY KEY(\"id\" AUTOINCREMENT))"));

        SqlQuery query(raw, "INSERT INTO credentials (type, name, data, guid) VALUES (0, ?, ?, ?)");
        query.addText(config.displayName());
        query.addBlob(*encrypted_data);
        query.addText(config.guid());
        ASSERT_TRUE(query.exec());

        ASSERT_TRUE(raw.exec("CREATE TABLE \"profiles\" ("
                             "\"id\" INTEGER UNIQUE,"
                             "\"name\" TEXT NOT NULL DEFAULT '',"
                             "\"data\" BLOB DEFAULT X'',"
                             "\"guid\" TEXT NOT NULL UNIQUE,"
                             "\"modify_time\" INTEGER NOT NULL DEFAULT 0,"
                             "\"sync_revision\" INTEGER NOT NULL DEFAULT 0,"
                             "\"sync_dirty\" INTEGER NOT NULL DEFAULT 0,"
                             "PRIMARY KEY(\"id\" AUTOINCREMENT))"));

        ProfileConfig profile;
        profile.setDesktopConfig(defaultDesktopConfig());
        profile.setUdpMethods(UDP_METHOD_ALL);

        SqlQuery profile_query(raw, "INSERT INTO profiles (name, data, guid) VALUES ('office', ?, ?)");
        profile_query.addBlob(profile.serializedData());
        profile_query.addText(QString("22222222-2222-2222-2222-222222222222"));
        ASSERT_TRUE(profile_query.exec());
    }

    // The second pass opens a database that has the column already.
    for (int pass = 0; pass < 2; ++pass)
    {
        Database earlier;
        ASSERT_TRUE(earlier.open(file_path));

        QList<CredentialConfig> credentials;
        ASSERT_EQ(CredentialStore(earlier).list(&credentials), Database::ReadResult::OK);
        ASSERT_EQ(credentials.size(), 1);
        EXPECT_EQ(credentials[0].username(), QString("user"));

        QList<ProfileConfig> profiles;
        ASSERT_EQ(ProfileStore(earlier).list(&profiles), Database::ReadResult::OK);
        ASSERT_EQ(profiles.size(), 2);
        EXPECT_EQ(profiles[1].name(), QString("office"));
    }

    SqlDatabase raw;
    ASSERT_TRUE(raw.open(file_path));

    SqlQuery query(raw, "SELECT deleted, sync_dirty FROM credentials");
    ASSERT_EQ(query.next(), SqlQuery::StepResult::ROW);
    EXPECT_EQ(query.columnInt64(0), 0);
    EXPECT_EQ(query.columnInt64(1), 1);

    SqlQuery profile_query(raw, "SELECT deleted FROM profiles WHERE name='office'");
    ASSERT_EQ(profile_query.next(), SqlQuery::StepResult::ROW);
    EXPECT_EQ(profile_query.columnInt64(0), 0);

    SqlQuery version(raw, "PRAGMA user_version");
    ASSERT_EQ(version.next(), SqlQuery::StepResult::ROW);
    EXPECT_EQ(version.columnInt64(0), 1);
}

//--------------------------------------------------------------------------------------------------
// A database made by an earlier version has the router hosts without the column that marks a removed host, and
// the trigger that drops an emptied row whatever the router holds. Both are brought up to date when it is opened.
TEST_F(DatabaseTest, DatabaseOfAnEarlierVersionGetsRemovedRouterHosts)
{
    const QString file_path = QFileInfo(file_path_).path() + "/earlier.db3";

    {
        SqlDatabase raw;
        ASSERT_TRUE(raw.open(file_path));
        ASSERT_TRUE(raw.exec("CREATE TABLE \"router_hosts\" ("
                             "\"host_id\" INTEGER PRIMARY KEY,"
                             "\"check_time\" INTEGER NOT NULL DEFAULT 0,"
                             "\"data\" BLOB DEFAULT X'',"
                             "\"credential_id\" INTEGER,"
                             "\"os_credential_id\" INTEGER,"
                             "\"profile_id\" INTEGER,"
                             "\"modify_time\" INTEGER NOT NULL DEFAULT 0,"
                             "\"sync_revision\" INTEGER NOT NULL DEFAULT 0,"
                             "\"sync_dirty\" INTEGER NOT NULL DEFAULT 0)"));
        ASSERT_TRUE(raw.exec("CREATE TRIGGER \"router_hosts_drop_empty\" AFTER UPDATE ON \"router_hosts\" "
                             "WHEN NEW.\"data\" = X'' AND NEW.\"credential_id\" IS NULL AND "
                             "NEW.\"os_credential_id\" IS NULL AND NEW.\"profile_id\" IS NULL "
                             "BEGIN DELETE FROM \"router_hosts\" WHERE \"host_id\" = NEW.\"host_id\"; END"));
    }

    Database earlier;
    ASSERT_TRUE(earlier.open(file_path));

    RouterHostStore router_hosts(earlier);
    ASSERT_TRUE(router_hosts.add(routerHost(100500, "user", "secret")));

    SqlDatabase raw;
    ASSERT_TRUE(raw.open(file_path));
    ASSERT_TRUE(raw.exec("UPDATE router_hosts SET sync_revision=7, sync_dirty=0"));

    ASSERT_TRUE(router_hosts.remove(100500));

    SqlQuery query(raw, "SELECT deleted, sync_dirty FROM router_hosts WHERE host_id=100500");
    ASSERT_EQ(query.next(), SqlQuery::StepResult::ROW);
    EXPECT_EQ(query.columnInt64(0), 1);
    EXPECT_EQ(query.columnInt64(1), 1);

    SqlQuery version(raw, "PRAGMA user_version");
    ASSERT_EQ(version.next(), SqlQuery::StepResult::ROW);
    EXPECT_EQ(version.columnInt64(0), 1);
}

//--------------------------------------------------------------------------------------------------
// A batch replaces the records together with the router they were exchanged with, so no removal
// is left waiting for that router.
TEST_F(DatabaseTest, BatchLeavesNoRemovedCredentialsBehind)
{
    const qint64 credential_id = addCredential("office", "user", "secret");

    ASSERT_TRUE(markSynced("credentials", credential_id, 7));
    ASSERT_TRUE(credentials_.remove(credential_id));
    ASSERT_EQ(rowCount("credentials"), 1);

    ASSERT_TRUE(db_.import({}, {}, {}, {}, {}, {}));

    EXPECT_EQ(rowCount("credentials"), 0);
    EXPECT_TRUE(unsyncedCredentials().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A received record that wins takes the place of what the device has: a local change is dropped,
// and a removed record is back in its row, where the hosts that referred to it find it.
TEST_F(DatabaseTest, ReceivedCredentialReplacesTheLocalChangeAndTheRemovedRecord)
{
    attachSyncStorage();

    const qint64 group = addGroup("group", 0);
    const qint64 credential_id = addCredential("office", "user", "secret");
    const QString guid = findCredential(db_, credential_id)->guid();

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setGroupId(group);
    host.setCredentialId(credential_id);
    ASSERT_TRUE(local_hosts_.add(host));

    ASSERT_TRUE(markSynced("credentials", credential_id, 7));

    CredentialConfig edited = credential("office", "local-user", "local-secret");
    edited.setId(credential_id);
    ASSERT_TRUE(credentials_.modify(edited));
    ASSERT_EQ(unsyncedCredentials().size(), 1);

    CredentialConfig received = credential("office", "router-user", "router-secret");
    received.setGuid(guid);
    ASSERT_TRUE(storeReceivedCredential(received, 9));

    EXPECT_TRUE(unsyncedCredentials().isEmpty());
    EXPECT_EQ(findCredential(db_, credential_id)->username(), QString("router-user"));

    ASSERT_TRUE(credentials_.remove(credential_id));
    ASSERT_TRUE(isMarkedDeleted("credentials", credential_id));

    ASSERT_TRUE(storeReceivedCredential(received, 10));

    EXPECT_TRUE(unsyncedCredentials().isEmpty());
    EXPECT_EQ(rowCount("credentials"), 1);

    const std::optional<std::pair<QString, SecureString>> entered = local_hosts_.credentials(host.id());
    ASSERT_TRUE(entered.has_value());
    EXPECT_EQ(entered->first, QString("router-user"));
    EXPECT_EQ(entered->second.toString(), QString("router-secret"));
}

//--------------------------------------------------------------------------------------------------
// A removal received from the router leaves no row to tell the router of, whatever the device
// had: an edited record or a removed one. The hosts that referred to the record refer to none.
TEST_F(DatabaseTest, ErasedCredentialLeavesNoRow)
{
    attachSyncStorage();

    const qint64 group = addGroup("group", 0);
    const qint64 edited_id = addCredential("edited", "user", "secret");
    const qint64 removed_id = addCredential("removed", "user", "secret");
    const QString edited_guid = findCredential(db_, edited_id)->guid();
    const QString removed_guid = findCredential(db_, removed_id)->guid();

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setGroupId(group);
    host.setCredentialId(edited_id);
    ASSERT_TRUE(local_hosts_.add(host));

    ASSERT_TRUE(markSynced("credentials", edited_id, 7));
    ASSERT_TRUE(markSynced("credentials", removed_id, 8));

    CredentialConfig edited = credential("edited", "other-user", "other-secret");
    edited.setId(edited_id);
    ASSERT_TRUE(credentials_.modify(edited));
    ASSERT_TRUE(credentials_.remove(removed_id));
    ASSERT_EQ(unsyncedCredentials().size(), 2);

    ASSERT_TRUE(eraseCredential(edited_guid));
    ASSERT_TRUE(eraseCredential(removed_guid));
    EXPECT_TRUE(eraseCredential(removed_guid));

    EXPECT_EQ(rowCount("credentials"), 0);
    EXPECT_TRUE(unsyncedCredentials().isEmpty());
    EXPECT_EQ(findLocalHost(db_, host.id())->credentialId(), 0);
}

//--------------------------------------------------------------------------------------------------
// The device remembers how far it has read the records of the router. A batch replaces the
// records together with the router they were exchanged with, so the reading starts over.
TEST_F(DatabaseTest, ReceivedRevisionIsKeptUntilABatchReplacesTheRecords)
{
    attachSyncStorage();

    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 0);
    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_PROFILES), 0);

    ASSERT_TRUE(setReceivedCredentialsRevision(12));
    ASSERT_TRUE(setReceivedProfilesRevision(14));
    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 12);
    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_PROFILES), 14);

    ASSERT_TRUE(db_.import({}, {}, {}, {}, {}, {}));
    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 0);
    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_PROFILES), 0);
}

//--------------------------------------------------------------------------------------------------
// A device that finds another storage in place of the one it was attached to gives up its records
// and takes what the new storage holds. The rows stay in place, marked as deleted and wiped, so
// that the hosts keep their links, and the reading starts over. The same storage set once more
// changes nothing of that, even with another key.
TEST_F(DatabaseTest, AnotherStorageDiscardsTheDataOfTheDevice)
{
    addRouter(true);
    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DONE);

    const qint64 kept_id = addCredential("kept", "user", "secret");
    const qint64 edited_id = addCredential("edited", "user", "secret");
    const qint64 removed_id = addCredential("removed", "user", "secret");
    const qint64 new_id = addCredential("new", "user", "secret");

    ASSERT_TRUE(markSynced("credentials", kept_id, 7));
    ASSERT_TRUE(markSynced("credentials", edited_id, 8));
    ASSERT_TRUE(markSynced("credentials", removed_id, 9));

    CredentialConfig edited = credential("edited", "other-user", "other-secret");
    edited.setId(edited_id);
    ASSERT_TRUE(credentials_.modify(edited));
    ASSERT_TRUE(credentials_.remove(removed_id));
    ASSERT_TRUE(setReceivedCredentialsRevision(12));

    const QByteArray storage_id = Random::byteArray(16);
    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), storage_id, SecureByteArray(Random::byteArray(32)), true),
              Database::SyncResult::DONE);

    EXPECT_TRUE(credentialList().isEmpty());
    EXPECT_EQ(rowCount("credentials"), 4);

    for (qint64 credential_id : { kept_id, edited_id, removed_id, new_id })
        EXPECT_TRUE(isMarkedDeleted("credentials", credential_id));

    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 0);

    ASSERT_TRUE(setReceivedCredentialsRevision(4));

    const SecureByteArray key(Random::byteArray(32));
    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), storage_id, key, true), Database::SyncResult::DONE);

    EXPECT_EQ(rowCount("credentials"), 4);
    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 4);

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->syncStorageKey(), key);
}

//--------------------------------------------------------------------------------------------------
// The router lost the storage, and another device made a new one in its place. The device keeps its
// records, every one of them is new to the router again, a removal that was not sent has no one to be
// sent to, and the reading starts over.
TEST_F(DatabaseTest, AnotherStorageNotMadeByAResetKeepsTheDataOfTheDevice)
{
    addRouter(true);
    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DONE);

    const qint64 kept_id = addCredential("kept", "user", "secret");
    const qint64 removed_id = addCredential("removed", "user", "secret");
    addCredential("new", "user", "secret");

    ASSERT_TRUE(markSynced("credentials", kept_id, 7));
    ASSERT_TRUE(markSynced("credentials", removed_id, 8));
    ASSERT_TRUE(credentials_.remove(removed_id));
    ASSERT_TRUE(setReceivedCredentialsRevision(12));

    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DONE);

    EXPECT_EQ(credentialList().size(), 2);
    EXPECT_EQ(rowCount("credentials"), 2);
    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 0);

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 2);

    for (const CredentialStore::Unsynced& item : unsynced)
    {
        EXPECT_FALSE(item.deleted);
        EXPECT_EQ(item.sync_revision, 0);
    }
}

//--------------------------------------------------------------------------------------------------
// A record the device gave up is not a removal to tell the router of. Sent as one, it would remove
// the record from a storage that holds it. A record added after that is sent as any new one.
TEST_F(DatabaseTest, DiscardedCredentialsAreNotSent)
{
    addRouter(true);
    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DONE);

    const qint64 kept_id = addCredential("kept", "user", "secret");
    const qint64 removed_id = addCredential("removed", "user", "secret");
    addCredential("new", "user", "secret");

    ASSERT_TRUE(markSynced("credentials", kept_id, 7));
    ASSERT_TRUE(markSynced("credentials", removed_id, 8));
    ASSERT_TRUE(credentials_.remove(removed_id));
    ASSERT_EQ(unsyncedCredentials().size(), 2);

    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), true),
              Database::SyncResult::DONE);

    EXPECT_TRUE(unsyncedCredentials().isEmpty());

    const qint64 added_id = addCredential("added", "user", "secret");

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].credential.id(), added_id);
    EXPECT_FALSE(unsynced[0].deleted);
    EXPECT_EQ(unsynced[0].sync_revision, 0);
}

//--------------------------------------------------------------------------------------------------
// A record the new storage holds under the guid of one the device gave up takes the row of that
// one over, and the local hosts that referred to the record are entered with it again. The router
// hosts are given up with the rest of the data, the storage brings its own.
TEST_F(DatabaseTest, ReceivedCredentialTakesTheRowOfTheDiscardedOne)
{
    addRouter(true);
    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DONE);

    const qint64 group = addGroup("group", 0);
    const qint64 credential_id = addCredential("office", "user", "secret");

    const std::optional<CredentialConfig> stored = findCredential(db_, credential_id);
    ASSERT_TRUE(stored.has_value());

    ASSERT_TRUE(markSynced("credentials", credential_id, 7));

    RouterHostConfig link_only = routerHost(100500, QString(), QString());
    link_only.setCredentialId(credential_id);
    ASSERT_TRUE(router_hosts_.add(link_only));

    const qint64 host_id = addHost("host", group);
    ASSERT_TRUE(local_hosts_.setOsCredentialId(host_id, credential_id));

    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), true),
              Database::SyncResult::DONE);

    EXPECT_FALSE(findCredentialByGuid(db_, stored->guid()).has_value());
    EXPECT_FALSE(findRouterHost(db_, 100500).has_value());
    EXPECT_EQ(rowCount("router_hosts"), 0);

    CredentialConfig received = credential("office", "other-user", "other-secret");
    received.setGuid(stored->guid());
    ASSERT_TRUE(storeReceivedCredential(received, 3));

    EXPECT_EQ(rowCount("credentials"), 1);

    const std::optional<CredentialConfig> by_guid = findCredentialByGuid(db_, stored->guid());
    ASSERT_TRUE(by_guid.has_value());
    EXPECT_EQ(by_guid->id(), credential_id);
    EXPECT_EQ(by_guid->username(), QString("other-user"));

    const std::optional<LocalHostConfig> local_host = findLocalHost(db_, host_id);
    ASSERT_TRUE(local_host.has_value());
    EXPECT_EQ(local_host->osCredentialId(), credential_id);

    EXPECT_TRUE(unsyncedCredentials().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A device that attaches to a storage for the first time keeps its records. They are new to the
// router and are sent to it.
TEST_F(DatabaseTest, FirstStorageKeepsTheDataOfTheDevice)
{
    addRouter(true);

    addCredential("first", "user", "secret");
    addCredential("second", "user", "secret");

    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DONE);

    EXPECT_EQ(credentialList().size(), 2);

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 2);

    for (const CredentialStore::Unsynced& item : unsynced)
    {
        EXPECT_FALSE(item.deleted);
        EXPECT_EQ(item.sync_revision, 0);
    }
}

//--------------------------------------------------------------------------------------------------
// A device with a pending reset is the one the new storage is filled from. It keeps its records,
// every one of them is new to the router again, a removal that was not sent has no one to be sent
// to, and the reading starts over. Attached to the new storage, the device has no reset pending.
TEST_F(DatabaseTest, DeviceWithAPendingResetKeepsItsData)
{
    addRouter(true);
    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DONE);

    const qint64 kept_id = addCredential("kept", "user", "secret");
    const qint64 edited_id = addCredential("edited", "user", "secret");
    const qint64 removed_id = addCredential("removed", "user", "secret");

    ASSERT_TRUE(markSynced("credentials", kept_id, 7));
    ASSERT_TRUE(markSynced("credentials", edited_id, 8));
    ASSERT_TRUE(markSynced("credentials", removed_id, 9));

    CredentialConfig edited = credential("edited", "other-user", "other-secret");
    edited.setId(edited_id);
    ASSERT_TRUE(credentials_.modify(edited));
    ASSERT_TRUE(credentials_.remove(removed_id));
    ASSERT_TRUE(setReceivedCredentialsRevision(12));

    ASSERT_TRUE(router_store_.setSyncStorageResetPending(*findRouter(db_)));

    std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->isSyncStorageResetPending());

    ASSERT_EQ(router_store_.setSyncStorage(*stored, Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DONE);

    EXPECT_EQ(credentialList().size(), 2);
    EXPECT_EQ(rowCount("credentials"), 2);
    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 0);

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 2);

    for (const CredentialStore::Unsynced& item : unsynced)
    {
        EXPECT_FALSE(item.deleted);
        EXPECT_EQ(item.sync_revision, 0);
        EXPECT_EQ(item.sync_dirty, 1);
    }

    stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->isSyncStorageResetPending());
}

//--------------------------------------------------------------------------------------------------
// A backup restored on a device that synchronizes replaces what the storage holds. The
// synchronization stays turned on and the reset of the storage is left pending, whatever account
// the router of the backup is saved for: the storage to reset is the one of the account the device
// signs in with. The device is attached to no storage until the reset is done, so nothing of the
// storage it was exchanging with is written over the restored records.
TEST_F(DatabaseTest, ImportKeepsSyncOnWithAPendingReset)
{
    attachSyncStorage();

    std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());
    ASSERT_FALSE(router->syncStorageId().isEmpty());

    const QByteArray storage_id = router->syncStorageId();

    router->setUsername("another-user");
    ASSERT_TRUE(db_.import(router, {}, {}, {}, { credential("office", "user", "secret") }, {}));

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->username(), QString("another-user"));
    EXPECT_TRUE(stored->isSyncEnabled());
    EXPECT_TRUE(stored->isSyncStorageResetPending());
    EXPECT_TRUE(stored->syncStorageId().isEmpty());
    EXPECT_TRUE(stored->syncStorageKey().isEmpty());

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, 0);
    EXPECT_FALSE(unsynced[0].deleted);

    SyncTransaction transaction(db_);
    EXPECT_EQ(transaction.begin(storage_id), Database::SyncResult::DETACHED);
}

//--------------------------------------------------------------------------------------------------
// A backup without a router leaves the device with no router, and so with nothing to synchronize
// with.
TEST_F(DatabaseTest, ImportWithoutARouterTurnsSyncOff)
{
    attachSyncStorage();

    const QByteArray storage_id = syncStorageId();
    ASSERT_FALSE(storage_id.isEmpty());

    ASSERT_TRUE(db_.import({}, {}, {}, {}, { credential("office", "user", "secret") }, {}));

    EXPECT_FALSE(findRouter(db_).has_value());

    SyncTransaction transaction(db_);
    EXPECT_EQ(transaction.begin(storage_id), Database::SyncResult::DETACHED);
}

//--------------------------------------------------------------------------------------------------
// A device with the synchronization turned off has no storage to replace, so a backup leaves the
// synchronization turned off and no reset pending.
TEST_F(DatabaseTest, ImportWithSyncOffLeavesNoPendingReset)
{
    addRouter();

    const std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());

    ASSERT_TRUE(db_.import(router, {}, {}, {}, { credential("office", "user", "secret") }, {}));

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->isSyncEnabled());
    EXPECT_FALSE(stored->isSyncStorageResetPending());
}

//--------------------------------------------------------------------------------------------------
// A backup of the same account is restored while its session goes on, so the record keeps the user it signed in as.
TEST_F(DatabaseTest, ImportOfTheSameAccountKeepsTheGuidOfTheUser)
{
    attachSyncStorage();

    std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());

    router->setUserGuid(kUserGuid);
    ASSERT_TRUE(router_store_.save(*router));

    RouterConfig backup = *router;
    backup.setUserGuid(QString());
    ASSERT_TRUE(db_.import(backup, {}, {}, {}, {}, {}));

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->isSyncEnabled());
    EXPECT_EQ(stored->userGuid(), QString(kUserGuid));
}

//--------------------------------------------------------------------------------------------------
// A backup of another account leaves the record without a user until the device signs in with it.
TEST_F(DatabaseTest, ImportOfAnotherAccountWaitsForTheUserThatSignsIn)
{
    attachSyncStorage();

    std::optional<RouterConfig> router = findRouter(db_);
    ASSERT_TRUE(router.has_value());

    router->setUserGuid(kUserGuid);
    ASSERT_TRUE(router_store_.save(*router));

    RouterConfig backup = *router;
    backup.setUserGuid(QString());
    backup.setUsername("another-user");
    ASSERT_TRUE(db_.import(backup, {}, {}, {}, {}, {}));

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->isSyncEnabled());
    EXPECT_TRUE(stored->userGuid().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A removed profile the router holds keeps a row marked as deleted, and its hosts run on the default one.
TEST_F(DatabaseTest, RemovedProfileTheRouterHoldsKeepsARowMarkedAsDeleted)
{
    const qint64 profile_id = addProfile("office");

    ProfileConfig stored;
    ASSERT_EQ(profiles_.find(profile_id, &stored), Database::FindResult::FOUND);
    ASSERT_TRUE(markSynced("profiles", profile_id, 7));

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setProfileId(profile_id);
    ASSERT_TRUE(local_hosts_.add(host));

    const qint64 start_time = QDateTime::currentSecsSinceEpoch();
    ASSERT_TRUE(profiles_.remove(profile_id));

    ProfileConfig removed;
    EXPECT_EQ(profiles_.find(profile_id, &removed), Database::FindResult::NOT_FOUND);
    EXPECT_TRUE(profileList().isEmpty());
    EXPECT_TRUE(isMarkedDeleted("profiles", profile_id));

    const QList<ProfileStore::Unsynced> unsynced = unsyncedProfiles();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_TRUE(unsynced[0].deleted);
    EXPECT_EQ(unsynced[0].profile.id(), profile_id);
    EXPECT_EQ(unsynced[0].profile.guid(), stored.guid());
    EXPECT_GE(unsynced[0].profile.modifyTime(), start_time);
    EXPECT_EQ(unsynced[0].sync_revision, 7);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);

    EXPECT_EQ(findLocalHost(db_, host.id())->profileId(), profile_id);
    EXPECT_TRUE(profiles_.hostProfile(HostConfig::forLocalHost(host)).isDefault());

    EXPECT_FALSE(profiles_.modify(stored));
    EXPECT_FALSE(profiles_.remove(profile_id));
    EXPECT_EQ(unsyncedProfiles().value(0).sync_dirty, 1);
}

//--------------------------------------------------------------------------------------------------
// A received profile takes the row of its guid, so the hosts keep it, and a revision already stored changes nothing.
TEST_F(DatabaseTest, ReceivedProfileIsStoredWithTheRevisionOfTheRouter)
{
    attachSyncStorage();

    const qint64 profile_id = addProfile("office");

    ProfileConfig local;
    ASSERT_EQ(profiles_.find(profile_id, &local), Database::FindResult::FOUND);
    ASSERT_TRUE(markSynced("profiles", profile_id, 7));

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setProfileId(profile_id);
    ASSERT_TRUE(local_hosts_.add(host));

    ASSERT_TRUE(profiles_.remove(profile_id));

    ProfileConfig received;
    received.setGuid(local.guid());
    received.setName("received");
    received.setDesktopConfig(defaultDesktopConfig());
    received.setUdpMethods(UDP_METHOD_DIRECT);
    received.setModifyTime(1000);
    ASSERT_TRUE(storeReceivedProfile(received, 12));

    ProfileConfig repeated = received;
    repeated.setName("repeated");
    ASSERT_TRUE(storeReceivedProfile(repeated, 12));

    ProfileConfig other = received;
    other.setGuid("11111111-1111-1111-1111-111111111111");
    other.setName("other");
    ASSERT_TRUE(storeReceivedProfile(other, 13));

    ProfileConfig stored;
    ASSERT_EQ(profiles_.find(profile_id, &stored), Database::FindResult::FOUND);
    EXPECT_EQ(stored.name(), QString("received"));
    EXPECT_EQ(stored.udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));
    EXPECT_EQ(stored.modifyTime(), 1000);

    EXPECT_EQ(profiles_.hostProfile(HostConfig::forLocalHost(host)).id(), profile_id);
    EXPECT_EQ(profileList().size(), 2);
    EXPECT_EQ(rowCount("profiles"), 3);
    EXPECT_TRUE(unsyncedProfiles().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A removal received from the router leaves no row, and the hosts of the profile run on the default one.
TEST_F(DatabaseTest, ErasedProfileLeavesNoRow)
{
    attachSyncStorage();

    const qint64 profile_id = addProfile("office");

    ProfileConfig stored;
    ASSERT_EQ(profiles_.find(profile_id, &stored), Database::FindResult::FOUND);
    ASSERT_TRUE(markSynced("profiles", profile_id, 7));

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setProfileId(profile_id);
    ASSERT_TRUE(local_hosts_.add(host));

    stored.setName("renamed");
    ASSERT_TRUE(profiles_.modify(stored));
    ASSERT_EQ(unsyncedProfiles().size(), 1);

    ASSERT_TRUE(eraseProfile(stored.guid()));
    EXPECT_TRUE(eraseProfile(stored.guid()));

    EXPECT_EQ(rowCount("profiles"), 1);
    EXPECT_TRUE(unsyncedProfiles().isEmpty());
    EXPECT_EQ(findLocalHost(db_, host.id())->profileId(), 0);
}

//--------------------------------------------------------------------------------------------------
// Another storage takes the profiles of the device away and leaves the default one until the storage brings its own.
TEST_F(DatabaseTest, DiscardedProfilesGiveWayToTheStorage)
{
    addRouter(true);
    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DONE);

    const qint64 kept_id = addProfile("kept");
    const qint64 new_id = addProfile("new");
    ASSERT_TRUE(markSynced("profiles", kept_id, 7));

    ProfileConfig default_profile = profiles_.defaultProfile();
    default_profile.setUdpMethods(UDP_METHOD_DIRECT);
    ASSERT_TRUE(profiles_.modify(default_profile));
    ASSERT_TRUE(setReceivedProfilesRevision(12));

    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), true),
              Database::SyncResult::DONE);

    EXPECT_TRUE(profileList().isEmpty());
    EXPECT_TRUE(isMarkedDeleted("profiles", kept_id));
    EXPECT_TRUE(isMarkedDeleted("profiles", new_id));
    EXPECT_EQ(profiles_.defaultProfile().udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));
    EXPECT_TRUE(unsyncedProfiles().isEmpty());
    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_PROFILES), 0);
}

//--------------------------------------------------------------------------------------------------
// A change of the master password reads every record with the old key and writes it back with the
// new one. A column that does not open comes back empty, and written back it would lose what it
// held for good, so the change is refused. A host of a router keeps its own pair next to the
// record of credentials it refers to, and that pair is no exception.
TEST_F(DatabaseTest, MasterPasswordChangeIsRefusedWhenALinkedRouterHostDoesNotOpen)
{
    const QString file_path = QFileInfo(file_path_).dir().filePath("singleton.db3");
    DatabaseTestPeer::setFilePath(file_path);

    Database& db = Database::instance();
    ASSERT_TRUE(db.isValid());

    RouterConfig router;
    router.setAddress("router.example.com");
    router.setUsername("router-user");
    router.setPassword(SecureString("router-secret"));
    ASSERT_TRUE(RouterStore(db).save(router));

    CredentialConfig credential;
    credential.setDisplayName("office");
    credential.setUsername("user");
    credential.setPassword(SecureString("secret"));
    ASSERT_TRUE(CredentialStore(db).add(credential));

    RouterHostConfig host = routerHost(100500, "own-user", "own-secret");
    host.setCredentialId(credential.id());
    ASSERT_TRUE(RouterHostStore().add(host));

    // A column rewritten behind the back of the book does not open.
    {
        SqlDatabase raw;
        ASSERT_TRUE(raw.open(file_path));
        ASSERT_TRUE(raw.exec("UPDATE router_hosts SET data=X'00' WHERE host_id=100500"));
    }

    EXPECT_EQ(MasterPassword::setNew(SecureString(QString("Password123"))),
              MasterPassword::Result::UNREADABLE_RECORD);

    // The column is left as it was, not emptied.
    {
        SqlDatabase raw;
        ASSERT_TRUE(raw.open(file_path));

        SqlQuery query(raw, "SELECT length(data) FROM router_hosts WHERE host_id=100500");
        ASSERT_EQ(query.next(), SqlQuery::StepResult::ROW);
        EXPECT_EQ(query.columnInt64(0), 1);
    }

    // Closes the file before the directory goes.
    DatabaseTestPeer::setFilePath(file_path);
}

//--------------------------------------------------------------------------------------------------
// A password that does not open the book is told apart from a book that can not be re-encrypted,
// so a user who mistyped the current one is not sent looking for a record to repair.
TEST_F(DatabaseTest, MasterPasswordChangeTellsAWrongPasswordFromARecordThatDoesNotOpen)
{
    const QString file_path = QFileInfo(file_path_).dir().filePath("singleton.db3");
    DatabaseTestPeer::setFilePath(file_path);

    Database& db = Database::instance();
    ASSERT_TRUE(db.isValid());

    // There is no password to unlock yet, which is not the user mistyping one.
    EXPECT_EQ(MasterPassword::change(SecureString(QString("Password123")),
                                     SecureString(QString("NewPassword123"))),
              MasterPassword::Result::FAILED);

    ASSERT_EQ(MasterPassword::setNew(SecureString(QString("Password123"))),
              MasterPassword::Result::SUCCESS);

    EXPECT_EQ(MasterPassword::change(SecureString(QString("WrongPassword1")),
                                     SecureString(QString("NewPassword123"))),
              MasterPassword::Result::INVALID_PASSWORD);

    EXPECT_EQ(MasterPassword::change(SecureString(QString("Password123")),
                                     SecureString(QString("NewPassword123"))),
              MasterPassword::Result::SUCCESS);

    // Closes the file before the directory goes.
    DatabaseTestPeer::setFilePath(file_path);
}

//--------------------------------------------------------------------------------------------------
// The automatic unlock keeps working after the master password is changed.
TEST_F(DatabaseTest, AutoUnlockSurvivesAMasterPasswordChange)
{
    const QString file_path = QFileInfo(file_path_).dir().filePath("singleton.db3");
    DatabaseTestPeer::setFilePath(file_path);
    ASSERT_TRUE(Database::instance().isValid());

    EXPECT_FALSE(MasterPassword::autoUnlock());

    ASSERT_EQ(MasterPassword::setNew(SecureString(QString("Password123"))),
              MasterPassword::Result::SUCCESS);
    ASSERT_TRUE(MasterPassword::setAutoUnlockEnabled(true));
    EXPECT_TRUE(MasterPassword::isAutoUnlockEnabled());

    const SecureByteArray key = MasterPassword::currentKey();
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));
    ASSERT_TRUE(MasterPassword::autoUnlock());
    EXPECT_EQ(MasterPassword::currentKey(), key);

    ASSERT_EQ(MasterPassword::change(SecureString(QString("Password123")),
                                     SecureString(QString("NewPassword123"))),
              MasterPassword::Result::SUCCESS);

    const SecureByteArray new_key = MasterPassword::currentKey();
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));
    ASSERT_TRUE(MasterPassword::autoUnlock());
    EXPECT_EQ(MasterPassword::currentKey(), new_key);

    ASSERT_TRUE(MasterPassword::setAutoUnlockEnabled(false));
    EXPECT_FALSE(MasterPassword::isAutoUnlockEnabled());
    EXPECT_FALSE(MasterPassword::autoUnlock());

    // Closes the file before the directory goes.
    DatabaseTestPeer::setFilePath(file_path);
}

//--------------------------------------------------------------------------------------------------
// A remembered key that does not open here stays for the next start, and one that no longer fits the
// password turns the automatic unlock off.
TEST_F(DatabaseTest, AutoUnlockIsTurnedOffOnlyByAKeyThatDoesNotFit)
{
    const QString file_path = QFileInfo(file_path_).dir().filePath("singleton.db3");
    DatabaseTestPeer::setFilePath(file_path);

    Database& db = Database::instance();
    ASSERT_TRUE(db.isValid());

    ASSERT_EQ(MasterPassword::setNew(SecureString(QString("Password123"))),
              MasterPassword::Result::SUCCESS);

    UnlockStore store(db);
    ASSERT_TRUE(store.setAutoUnlockBlob(Random::byteArray(64)));
    EXPECT_FALSE(MasterPassword::autoUnlock());
    EXPECT_TRUE(MasterPassword::isAutoUnlockEnabled());

    QByteArray blob;
    ASSERT_TRUE(OSCrypt::encryptBytes(Random::byteArray(32), &blob));
    ASSERT_TRUE(store.setAutoUnlockBlob(blob));
    EXPECT_FALSE(MasterPassword::autoUnlock());
    EXPECT_FALSE(MasterPassword::isAutoUnlockEnabled());

    // Closes the file before the directory goes.
    DatabaseTestPeer::setFilePath(file_path);
}
