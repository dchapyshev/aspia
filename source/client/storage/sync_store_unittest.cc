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

#include "client/storage/sync_store.h"

#include <QDateTime>

#include "client/storage/database_test_base.h"

class SyncStoreTest : public DatabaseTestBase
{
protected:
    bool setCredentialSyncRevision(const QString& guid, qint64 sync_revision)
    {
        SyncTransaction transaction(db_);
        return transaction.begin(syncStorageId()) == Database::SyncResult::DONE &&
               transaction.setRecordSyncRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS, guid, sync_revision) &&
               transaction.commit();
    }

    bool confirmSentCredential(const QString& guid, qint64 sync_revision, qint64 sync_dirty)
    {
        SyncTransaction transaction(db_);
        return transaction.begin(syncStorageId()) == Database::SyncResult::DONE &&
               transaction.confirmSentRecord(proto::router::SYNC_DATA_TYPE_CREDENTIALS, guid, sync_revision,
                                             sync_dirty) &&
               transaction.commit();
    }

    bool confirmSentProfile(const QString& guid, qint64 sync_revision, qint64 sync_dirty)
    {
        SyncTransaction transaction(db_);
        return transaction.begin(syncStorageId()) == Database::SyncResult::DONE &&
               transaction.confirmSentRecord(proto::router::SYNC_DATA_TYPE_PROFILES, guid, sync_revision, sync_dirty) &&
               transaction.commit();
    }
};

//--------------------------------------------------------------------------------------------------
// The synchronization changes the records of the device only in a transaction of the storage
// the device is attached to. A transaction of another storage, or of none, does not begin, and
// what a transaction did not commit is not kept.
TEST_F(SyncStoreTest, SyncChangesAreRefusedForAnotherStorage)
{
    addRouter(true);

    const QByteArray storage_id = Random::byteArray(16);
    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), storage_id, SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DONE);

    SyncTransaction transaction(db_);
    EXPECT_EQ(transaction.begin(Random::byteArray(16)), Database::SyncResult::DETACHED);
    EXPECT_EQ(transaction.begin(QByteArray()), Database::SyncResult::DETACHED);
    EXPECT_FALSE(transaction.setReceivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS, 12));
    EXPECT_FALSE(transaction.commit());

    {
        SyncTransaction dropped(db_);
        ASSERT_EQ(dropped.begin(storage_id), Database::SyncResult::DONE);
        ASSERT_TRUE(dropped.setReceivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS, 12));
    }

    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 0);

    ASSERT_EQ(transaction.begin(storage_id), Database::SyncResult::DONE);
    ASSERT_TRUE(transaction.setReceivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS, 12));
    ASSERT_TRUE(transaction.commit());
    EXPECT_FALSE(transaction.setReceivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS, 13));

    EXPECT_EQ(sync_store_.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 12);
}

//--------------------------------------------------------------------------------------------------
// The router stores a change only over the revision the change was based on. A local change that
// wins over a received record stays as it is and is sent over the revision of that record.
TEST_F(SyncStoreTest, LocalChangeThatWinsIsBasedOnTheReceivedRevision)
{
    attachSyncStorage();

    const qint64 credential_id = addCredential("office", "user", "secret");
    const QString guid = findCredential(db_, credential_id)->guid();

    ASSERT_TRUE(markSynced("credentials", credential_id, 7));

    CredentialConfig edited = credential("office", "local-user", "local-secret");
    edited.setId(credential_id);
    ASSERT_TRUE(credentials_.modify(edited));

    ASSERT_TRUE(setCredentialSyncRevision(guid, 9));

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].credential.username(), QString("local-user"));
    EXPECT_EQ(unsynced[0].sync_revision, 9);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);
}

//--------------------------------------------------------------------------------------------------
// A record that was never sent stays unsynced when it wins over a received record and takes its revision.
TEST_F(SyncStoreTest, CredentialNeverSentThatWinsStaysUnsynced)
{
    attachSyncStorage();

    const qint64 credential_id = addCredential("office", "user", "secret");
    const QString guid = findCredential(db_, credential_id)->guid();

    QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, 0);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);

    ASSERT_TRUE(setCredentialSyncRevision(guid, 9));

    unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].credential.username(), QString("user"));
    EXPECT_EQ(unsynced[0].sync_revision, 9);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);
}

//--------------------------------------------------------------------------------------------------
// A record the router stored has nothing more to send, unless it was edited while it was on its
// way: what is taken off the count of its changes is the count the record was sent with.
TEST_F(SyncStoreTest, ConfirmedCredentialIsSentAgainOnlyWhenEditedMeanwhile)
{
    attachSyncStorage();

    const qint64 credential_id = addCredential("office", "user", "secret");
    const QString guid = findCredential(db_, credential_id)->guid();

    ASSERT_TRUE(confirmSentCredential(guid, 12, 1));
    EXPECT_TRUE(unsyncedCredentials().isEmpty());

    CredentialConfig edited = credential("office", "other-user", "other-secret");
    edited.setId(credential_id);
    ASSERT_TRUE(credentials_.modify(edited));
    ASSERT_TRUE(credentials_.modify(edited));

    const QList<CredentialStore::Unsynced> sent = unsyncedCredentials();
    ASSERT_EQ(sent.size(), 1);
    EXPECT_EQ(sent[0].sync_revision, 12);
    EXPECT_EQ(sent[0].sync_dirty, 2);

    ASSERT_TRUE(credentials_.modify(edited));
    ASSERT_TRUE(confirmSentCredential(guid, 13, sent[0].sync_dirty));

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, 13);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);
}

//--------------------------------------------------------------------------------------------------
// Once the router has stored the removal of a record, the row kept for it is gone. A record that
// came back while the removal was on its way stays and is sent as a change.
TEST_F(SyncStoreTest, ConfirmedRemovalErasesTheRowUnlessTheRecordCameBack)
{
    attachSyncStorage();

    const qint64 gone_id = addCredential("gone", "user", "secret");
    const qint64 back_id = addCredential("back", "user", "secret");
    const QString gone_guid = findCredential(db_, gone_id)->guid();
    const QString back_guid = findCredential(db_, back_id)->guid();

    ASSERT_TRUE(markSynced("credentials", gone_id, 7));
    ASSERT_TRUE(markSynced("credentials", back_id, 8));
    ASSERT_TRUE(credentials_.remove(gone_id));
    ASSERT_TRUE(credentials_.remove(back_id));

    CredentialConfig restored = credential("back", "user", "secret");
    restored.setGuid(back_guid);
    ASSERT_TRUE(credentials_.add(restored));

    ASSERT_TRUE(confirmSentCredential(gone_guid, 9, 1));
    ASSERT_TRUE(confirmSentCredential(back_guid, 10, 1));

    EXPECT_EQ(rowCount("credentials"), 1);

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_FALSE(unsynced[0].deleted);
    EXPECT_EQ(unsynced[0].credential.guid(), back_guid);
    EXPECT_EQ(unsynced[0].sync_revision, 10);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);
}

//--------------------------------------------------------------------------------------------------
// A record removed while it was on its way to the router for the first time left no row, and the
// router holds it now. Its removal is kept to be sent.
TEST_F(SyncStoreTest, CredentialRemovedOnItsWayToTheRouterIsRemovedThereToo)
{
    attachSyncStorage();

    const qint64 credential_id = addCredential("office", "user", "secret");
    const QString guid = findCredential(db_, credential_id)->guid();

    ASSERT_TRUE(credentials_.remove(credential_id));
    ASSERT_EQ(rowCount("credentials"), 0);

    const qint64 start_time = QDateTime::currentSecsSinceEpoch();
    ASSERT_TRUE(confirmSentCredential(guid, 12, 1));

    EXPECT_TRUE(credentialList().isEmpty());

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_TRUE(unsynced[0].deleted);
    EXPECT_EQ(unsynced[0].credential.guid(), guid);
    EXPECT_GE(unsynced[0].credential.modifyTime(), start_time);
    EXPECT_EQ(unsynced[0].sync_revision, 12);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);
}

//--------------------------------------------------------------------------------------------------
// A profile the router stored is sent again only when it was edited while it was on its way.
TEST_F(SyncStoreTest, ConfirmedProfileIsSentAgainOnlyWhenEditedMeanwhile)
{
    attachSyncStorage();

    const qint64 profile_id = addProfile("office");

    ProfileConfig edited;
    ASSERT_EQ(profiles_.find(profile_id, &edited), Database::FindResult::FOUND);

    ASSERT_TRUE(confirmSentProfile(edited.guid(), 12, 1));
    EXPECT_TRUE(unsyncedProfiles().isEmpty());

    edited.setName("renamed");
    ASSERT_TRUE(profiles_.modify(edited));
    ASSERT_TRUE(profiles_.modify(edited));

    const QList<ProfileStore::Unsynced> sent = unsyncedProfiles();
    ASSERT_EQ(sent.size(), 1);
    EXPECT_EQ(sent[0].sync_revision, 12);
    EXPECT_EQ(sent[0].sync_dirty, 2);

    ASSERT_TRUE(profiles_.modify(edited));
    ASSERT_TRUE(confirmSentProfile(edited.guid(), 13, sent[0].sync_dirty));

    const QList<ProfileStore::Unsynced> unsynced = unsyncedProfiles();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, 13);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);
}

//--------------------------------------------------------------------------------------------------
// A profile removed on its first way to the router left no row, so its removal is kept to be sent.
TEST_F(SyncStoreTest, ProfileRemovedOnItsWayToTheRouterIsRemovedThereToo)
{
    attachSyncStorage();

    const qint64 profile_id = addProfile("office");

    ProfileConfig stored;
    ASSERT_EQ(profiles_.find(profile_id, &stored), Database::FindResult::FOUND);

    ASSERT_TRUE(profiles_.remove(profile_id));
    ASSERT_EQ(rowCount("profiles"), 1);

    const qint64 start_time = QDateTime::currentSecsSinceEpoch();
    ASSERT_TRUE(confirmSentProfile(stored.guid(), 12, 1));

    EXPECT_TRUE(profileList().isEmpty());

    const QList<ProfileStore::Unsynced> unsynced = unsyncedProfiles();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_TRUE(unsynced[0].deleted);
    EXPECT_EQ(unsynced[0].profile.guid(), stored.guid());
    EXPECT_GE(unsynced[0].profile.modifyTime(), start_time);
    EXPECT_EQ(unsynced[0].sync_revision, 12);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);
}
