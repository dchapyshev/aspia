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

#include "client/sync/sync_credentials.h"

#include <QDateTime>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "client/storage/router_store.h"
#include "client/sync/sync_crypto.h"
#include "proto/router_constants.h"

namespace {

// The clock of the router. It is set apart from the clock of the devices on purpose: a record is
// as old as the router says, whatever the clocks show.
const qint64 kRouterTime = 1000000;
const char kStorageId[] = "0123456789abcdef";

class SyncCredentialsTest : public testing::Test
{
protected:
    void SetUp() override
    {
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

        ASSERT_TRUE(dir_.isValid());
        ASSERT_TRUE(laptop_.open(dir_.filePath("laptop.db3")));
        ASSERT_TRUE(phone_.open(dir_.filePath("phone.db3")));

        RouterConfig router;
        router.setAddress("router.example.com");
        router.setUsername("user");
        router.setPassword(SecureString(QString("secret")));
        router.setSyncEnabled(true);

        for (Database* db : { &laptop_, &phone_ })
        {
            RouterStore store(*db);
            ASSERT_TRUE(store.save(router));
            ASSERT_EQ(store.setSyncStorage(router, kStorageId, key_, false), Database::SyncResult::DONE);
        }
    }

    static CredentialConfig addCredential(Database& db, const QString& username)
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername(username);
        credential.setPassword(SecureString(QString("secret")));

        EXPECT_TRUE(CredentialStore(db).add(credential));
        return credential;
    }

    static void modifyCredential(Database& db, const QString& guid, const QString& username)
    {
        CredentialStore store(db);

        CredentialConfig credential;
        ASSERT_EQ(store.findByGuid(guid, &credential), Database::FindResult::FOUND);

        credential.setUsername(username);
        ASSERT_TRUE(store.modify(credential));
    }

    static void removeCredential(Database& db, const QString& guid)
    {
        CredentialStore store(db);

        CredentialConfig credential;
        ASSERT_EQ(store.findByGuid(guid, &credential), Database::FindResult::FOUND);
        ASSERT_TRUE(store.remove(credential.id()));
    }

    // The name of the user of the record, or an empty string when the device has no such record.
    static QString storedUsername(Database& db, const QString& guid)
    {
        CredentialConfig credential;
        if (CredentialStore(db).findByGuid(guid, &credential) != Database::FindResult::FOUND)
            return QString();

        return credential.username();
    }

    static QList<CredentialStore::Unsynced> unsyncedCredentials(Database& db)
    {
        QList<CredentialStore::Unsynced> credentials;
        EXPECT_EQ(CredentialStore(db).unsynced(&credentials), Database::ReadResult::OK);
        return credentials;
    }

    // Makes the last change of the record |age| seconds old.
    void setLocalAge(const QString& file_name, const QString& guid, qint64 age)
    {
        SqlDatabase raw;
        ASSERT_TRUE(raw.open(dir_.filePath(file_name)));

        SqlQuery query(raw, "UPDATE credentials SET modify_time=? WHERE guid=?");
        query.addInt64(QDateTime::currentSecsSinceEpoch() - age);
        query.addText(guid);
        ASSERT_TRUE(query.exec());
    }

    // Makes the data of the record unreadable and the record unsynced.
    void damageLocalRecord(const QString& file_name, const QString& guid)
    {
        SqlDatabase raw;
        ASSERT_TRUE(raw.open(dir_.filePath(file_name)));

        SqlQuery query(raw, "UPDATE credentials SET data=?, sync_dirty=1 WHERE guid=?");
        query.addBlob(QByteArray("damaged"));
        query.addText(guid);
        ASSERT_TRUE(query.exec());
    }

    // Stands for the router: stores what the device has to send, the way the router does when the
    // revisions the changes are based on fit, and returns the stored records.
    proto::router::SyncRecordList sendRecords(SyncCredentials& sync)
    {
        proto::router::SyncRecordRequest request;
        EXPECT_EQ(sync.readUnsyncedRecords(&request), SyncCredentials::Result::DONE);

        proto::router::SyncRecordResult result;
        proto::router::SyncRecordList list;

        for (const proto::router::SyncRecord& record : request.record())
        {
            proto::router::SyncRecord* stored = list.add_record();
            stored->CopyFrom(record);
            stored->set_revision(++revision_);

            result.add_record_revision(revision_);
        }

        EXPECT_EQ(sync.confirmSentRecords(result), SyncCredentials::Result::DONE);
        return list;
    }

    // Hands the records to the device as stored by the router |age| seconds ago.
    static void receiveRecords(SyncCredentials& sync, proto::router::SyncRecordList list, qint64 age)
    {
        list.set_current_time(kRouterTime);

        for (proto::router::SyncRecord& record : *list.mutable_record())
            record.set_time(kRouterTime - age);

        ASSERT_EQ(sync.storeReceivedRecords(list), SyncCredentials::Result::DONE);
    }

    // A record a newer version of the client has made: it carries |unknown_field|, a field this
    // version does not know.
    proto::router::SyncRecord newerRecord(
        const char* guid, qint64 revision, const char* username, const std::string& unknown_field) const
    {
        proto::router::SyncCredential content;
        content.set_display_name("office");
        content.set_username(username);
        content.set_password("secret");

        const QByteArray plain = serialize(content) + QByteArray::fromStdString(unknown_field);
        const std::optional<QByteArray> sealed =
            SyncCrypto::encryptRecord(key_, proto::router::SYNC_DATA_TYPE_CREDENTIALS, guid, plain);
        EXPECT_TRUE(sealed.has_value());

        proto::router::SyncRecord record;
        record.set_revision(revision);
        record.set_type(proto::router::SYNC_DATA_TYPE_CREDENTIALS);
        record.set_id(guid);
        record.set_data(sealed.value_or(QByteArray()).toStdString());
        return record;
    }

    // The fields of the record the device has to send, opened.
    proto::router::SyncCredential sentContent(SyncCredentials& sync, const char* guid)
    {
        proto::router::SyncRecordRequest request;
        EXPECT_EQ(sync.readUnsyncedRecords(&request), SyncCredentials::Result::DONE);
        EXPECT_EQ(request.record_size(), 1);

        proto::router::SyncCredential content;
        if (request.record_size() != 1)
            return content;

        const std::optional<QByteArray> opened = SyncCrypto::decryptRecord(
            key_, proto::router::SYNC_DATA_TYPE_CREDENTIALS, guid, QByteArray::fromStdString(request.record(0).data()));
        EXPECT_TRUE(opened.has_value());
        EXPECT_TRUE(parse(opened.value_or(QByteArray()), &content));
        return content;
    }

    // Brings a record to both devices and returns its guid.
    QString addSharedCredential()
    {
        const QString guid = addCredential(laptop_, "user").guid();
        receiveRecords(phone_sync_, sendRecords(laptop_sync_), 0);

        EXPECT_EQ(storedUsername(phone_, guid), QString("user"));
        return guid;
    }

    QTemporaryDir dir_;
    Database laptop_;
    Database phone_;
    const SecureByteArray key_ = SyncCrypto::generateStorageKey();
    SyncCredentials laptop_sync_{ laptop_, kStorageId, key_ };
    SyncCredentials phone_sync_{ phone_, kStorageId, key_ };
    qint64 revision_ = 0;
};

} // namespace

//--------------------------------------------------------------------------------------------------
// A record made on one device goes to the router sealed, based on no revision, and comes to the
// other device as it was made, with the revision of the router and the age the router gave it.
TEST_F(SyncCredentialsTest, NewRecordReachesTheOtherDevice)
{
    const CredentialConfig credential = addCredential(laptop_, "user");

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_sync_.readUnsyncedRecords(&request), SyncCredentials::Result::DONE);
    ASSERT_EQ(request.record_size(), 1);
    EXPECT_EQ(request.record(0).revision(), 0);
    EXPECT_EQ(request.record(0).type(), proto::router::SYNC_DATA_TYPE_CREDENTIALS);
    EXPECT_EQ(request.record(0).id(), credential.guid().toStdString());
    EXPECT_FALSE(request.record(0).deleted());
    EXPECT_FALSE(request.record(0).data().empty());
    EXPECT_LE(request.record(0).data().size(), proto::router::kMaxSyncRecordDataSize);
    EXPECT_EQ(request.record(0).data().find("secret"), std::string::npos);

    const proto::router::SyncRecordList list = sendRecords(laptop_sync_);
    ASSERT_EQ(list.record_size(), 1);
    EXPECT_TRUE(unsyncedCredentials(laptop_).isEmpty());

    const qint64 start_time = QDateTime::currentSecsSinceEpoch();
    receiveRecords(phone_sync_, list, 100);

    CredentialConfig received;
    ASSERT_EQ(CredentialStore(phone_).findByGuid(credential.guid(), &received), Database::FindResult::FOUND);
    EXPECT_EQ(received.type(), credential.type());
    EXPECT_EQ(received.displayName(), credential.displayName());
    EXPECT_EQ(received.username(), credential.username());
    EXPECT_EQ(received.password(), credential.password());
    EXPECT_GE(received.modifyTime(), start_time - 100);
    EXPECT_LE(received.modifyTime(), QDateTime::currentSecsSinceEpoch() - 100);

    EXPECT_TRUE(unsyncedCredentials(phone_).isEmpty());
    EXPECT_EQ(SyncStore(phone_).receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), list.record(0).revision());
}

//--------------------------------------------------------------------------------------------------
// A removal goes to the router as a record with no data, based on the revision the device knew,
// and removes the record from the other device.
TEST_F(SyncCredentialsTest, RemovalReachesTheOtherDevice)
{
    const QString guid = addSharedCredential();

    removeCredential(laptop_, guid);

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_sync_.readUnsyncedRecords(&request), SyncCredentials::Result::DONE);
    ASSERT_EQ(request.record_size(), 1);
    EXPECT_EQ(request.record(0).revision(), 1);
    EXPECT_TRUE(request.record(0).deleted());
    EXPECT_TRUE(request.record(0).data().empty());

    receiveRecords(phone_sync_, sendRecords(laptop_sync_), 0);

    EXPECT_TRUE(storedUsername(phone_, guid).isEmpty());
    EXPECT_TRUE(unsyncedCredentials(laptop_).isEmpty());
    EXPECT_TRUE(unsyncedCredentials(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Both devices changed the record. The change that is younger wins: a received one replaces the
// local one, and a local one stays to be sent over the revision of the received one.
TEST_F(SyncCredentialsTest, YoungerChangeWins)
{
    const QString guid = addSharedCredential();

    modifyCredential(phone_, guid, "phone-user");
    setLocalAge("phone.db3", guid, 30);

    modifyCredential(laptop_, guid, "laptop-user");
    receiveRecords(phone_sync_, sendRecords(laptop_sync_), 25);

    EXPECT_EQ(storedUsername(phone_, guid), QString("laptop-user"));
    EXPECT_TRUE(unsyncedCredentials(phone_).isEmpty());

    modifyCredential(phone_, guid, "phone-user");
    setLocalAge("phone.db3", guid, 20);

    modifyCredential(laptop_, guid, "other-laptop-user");
    receiveRecords(phone_sync_, sendRecords(laptop_sync_), 25);

    EXPECT_EQ(storedUsername(phone_, guid), QString("phone-user"));

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials(phone_);
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, revision_);

    receiveRecords(laptop_sync_, sendRecords(phone_sync_), 0);

    EXPECT_EQ(storedUsername(laptop_, guid), QString("phone-user"));
    EXPECT_TRUE(unsyncedCredentials(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A record the device has never sent meets a record of the router with the same guid: the device
// was detached and attached again, or the record was imported. A local one that is younger wins,
// and it is still to be sent, now over the revision of the received one.
TEST_F(SyncCredentialsTest, RecordNeverSentThatWinsIsSent)
{
    const QString guid = addCredential(laptop_, "laptop-user").guid();
    const proto::router::SyncRecordList list = sendRecords(laptop_sync_);

    CredentialConfig local;
    local.setGuid(guid);
    local.setDisplayName("office");
    local.setUsername("phone-user");
    local.setPassword(SecureString(QString("secret")));
    ASSERT_TRUE(CredentialStore(phone_).add(local));

    receiveRecords(phone_sync_, list, 30);

    EXPECT_EQ(storedUsername(phone_, guid), QString("phone-user"));

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials(phone_);
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, revision_);

    receiveRecords(laptop_sync_, sendRecords(phone_sync_), 0);

    EXPECT_EQ(storedUsername(laptop_, guid), QString("phone-user"));
    EXPECT_TRUE(unsyncedCredentials(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// One device removed the record the other one changed. A removal that is younger removes the
// record, and a change that is younger brings the record back to the device that removed it.
TEST_F(SyncCredentialsTest, ReceivedRemovalMeetsALocalChange)
{
    const QString removed_guid = addSharedCredential();
    const QString kept_guid = addSharedCredential();

    modifyCredential(phone_, removed_guid, "phone-user");
    modifyCredential(phone_, kept_guid, "phone-user");
    setLocalAge("phone.db3", removed_guid, 30);
    setLocalAge("phone.db3", kept_guid, 20);

    removeCredential(laptop_, removed_guid);
    removeCredential(laptop_, kept_guid);
    receiveRecords(phone_sync_, sendRecords(laptop_sync_), 25);

    EXPECT_TRUE(storedUsername(phone_, removed_guid).isEmpty());
    EXPECT_EQ(storedUsername(phone_, kept_guid), QString("phone-user"));
    ASSERT_EQ(unsyncedCredentials(phone_).size(), 1);

    receiveRecords(laptop_sync_, sendRecords(phone_sync_), 0);

    EXPECT_TRUE(storedUsername(laptop_, removed_guid).isEmpty());
    EXPECT_EQ(storedUsername(laptop_, kept_guid), QString("phone-user"));
    EXPECT_TRUE(unsyncedCredentials(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// One device changed the record the other one removed. A change that is younger brings the record
// back, and a removal that is younger stays to be sent over the revision of the change.
TEST_F(SyncCredentialsTest, ReceivedChangeMeetsALocalRemoval)
{
    const QString restored_guid = addSharedCredential();
    const QString removed_guid = addSharedCredential();

    removeCredential(phone_, restored_guid);
    removeCredential(phone_, removed_guid);
    setLocalAge("phone.db3", restored_guid, 30);
    setLocalAge("phone.db3", removed_guid, 20);

    modifyCredential(laptop_, restored_guid, "laptop-user");
    modifyCredential(laptop_, removed_guid, "laptop-user");
    receiveRecords(phone_sync_, sendRecords(laptop_sync_), 25);

    EXPECT_EQ(storedUsername(phone_, restored_guid), QString("laptop-user"));
    EXPECT_TRUE(storedUsername(phone_, removed_guid).isEmpty());

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials(phone_);
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_TRUE(unsynced[0].deleted);
    EXPECT_EQ(unsynced[0].credential.guid(), removed_guid);

    receiveRecords(laptop_sync_, sendRecords(phone_sync_), 0);

    EXPECT_EQ(storedUsername(laptop_, restored_guid), QString("laptop-user"));
    EXPECT_TRUE(storedUsername(laptop_, removed_guid).isEmpty());
    EXPECT_TRUE(unsyncedCredentials(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Devices that made the same change, or both removed the record, have nothing to tell each other,
// whichever of them was the last.
TEST_F(SyncCredentialsTest, SameChangeOnBothDevicesIsNotSentBack)
{
    const QString changed_guid = addSharedCredential();
    const QString removed_guid = addSharedCredential();

    modifyCredential(phone_, changed_guid, "same-user");
    removeCredential(phone_, removed_guid);
    setLocalAge("phone.db3", changed_guid, 5);
    setLocalAge("phone.db3", removed_guid, 5);

    modifyCredential(laptop_, changed_guid, "same-user");
    removeCredential(laptop_, removed_guid);
    receiveRecords(phone_sync_, sendRecords(laptop_sync_), 25);

    EXPECT_EQ(storedUsername(phone_, changed_guid), QString("same-user"));
    EXPECT_TRUE(unsyncedCredentials(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A device receives what it has sent itself. A record changed after it was sent is based on that
// very revision, so what comes back does not replace the change, however young the router calls it.
TEST_F(SyncCredentialsTest, OwnRecordThatComesBackDoesNotReplaceALaterChange)
{
    const QString guid = addCredential(laptop_, "user").guid();
    const proto::router::SyncRecordList list = sendRecords(laptop_sync_);

    modifyCredential(laptop_, guid, "later-user");
    setLocalAge("laptop.db3", guid, 100);

    receiveRecords(laptop_sync_, list, 0);

    EXPECT_EQ(storedUsername(laptop_, guid), QString("later-user"));

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials(laptop_);
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, 1);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);
}

//--------------------------------------------------------------------------------------------------
// A record changed between the moment it was read to be sent and the answer of the router is sent
// once more, over the revision the router has given.
TEST_F(SyncCredentialsTest, RecordChangedWhileItWasSentIsSentAgain)
{
    const QString guid = addCredential(laptop_, "user").guid();

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_sync_.readUnsyncedRecords(&request), SyncCredentials::Result::DONE);
    ASSERT_EQ(request.record_size(), 1);

    modifyCredential(laptop_, guid, "later-user");

    proto::router::SyncRecordResult result;
    result.add_record_revision(7);
    ASSERT_EQ(laptop_sync_.confirmSentRecords(result), SyncCredentials::Result::DONE);

    ASSERT_EQ(laptop_sync_.readUnsyncedRecords(&request), SyncCredentials::Result::DONE);
    ASSERT_EQ(request.record_size(), 1);
    EXPECT_EQ(request.record(0).revision(), 7);
    EXPECT_EQ(unsyncedCredentials(laptop_).value(0).sync_dirty, 1);
}

//--------------------------------------------------------------------------------------------------
// The router answers with a zero for a record it did not store, because it holds a revision other
// than the one the change was based on. Such a record stays as it was. An answer that does not
// fit what was sent is refused as a whole.
TEST_F(SyncCredentialsTest, RecordTheRouterDidNotStoreStaysUnsynced)
{
    const QString guid = addSharedCredential();

    modifyCredential(laptop_, guid, "laptop-user");

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_sync_.readUnsyncedRecords(&request), SyncCredentials::Result::DONE);
    ASSERT_EQ(request.record_size(), 1);

    proto::router::SyncRecordResult result;
    result.add_record_revision(0);
    ASSERT_EQ(laptop_sync_.confirmSentRecords(result), SyncCredentials::Result::DONE);

    QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials(laptop_);
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, 1);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);

    ASSERT_EQ(laptop_sync_.readUnsyncedRecords(&request), SyncCredentials::Result::DONE);
    result.add_record_revision(8);
    EXPECT_EQ(laptop_sync_.confirmSentRecords(result), SyncCredentials::Result::INVALID_ANSWER);

    unsynced = unsyncedCredentials(laptop_);
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, 1);
}

//--------------------------------------------------------------------------------------------------
// A received record that does not open loses to what the device has: the record of the device
// stays and takes the revision of the received one, so that its next change is stored over it. A
// record the device does not have is passed by, and the reading goes on.
TEST_F(SyncCredentialsTest, ReceivedRecordThatDoesNotOpenLoses)
{
    const QString guid = addSharedCredential();

    proto::router::SyncRecordList list;

    proto::router::SyncRecord* damaged = list.add_record();
    damaged->set_revision(5);
    damaged->set_type(proto::router::SYNC_DATA_TYPE_CREDENTIALS);
    damaged->set_id(guid.toStdString());
    damaged->set_data("damaged");

    proto::router::SyncRecord* unknown = list.add_record();
    unknown->set_revision(6);
    unknown->set_type(proto::router::SYNC_DATA_TYPE_CREDENTIALS);
    unknown->set_id("11111111-1111-1111-1111-111111111111");
    unknown->set_data("damaged");

    receiveRecords(phone_sync_, list, 0);

    EXPECT_EQ(storedUsername(phone_, guid), QString("user"));
    EXPECT_TRUE(unsyncedCredentials(phone_).isEmpty());
    EXPECT_EQ(SyncStore(phone_).receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 6);

    QList<CredentialConfig> credentials;
    ASSERT_EQ(CredentialStore(phone_).list(&credentials), Database::ReadResult::OK);
    EXPECT_EQ(credentials.size(), 1);

    modifyCredential(phone_, guid, "phone-user");

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials(phone_);
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, 5);
}

//--------------------------------------------------------------------------------------------------
// A damaged record of the device has nothing to keep, so an older change on the other device replaces it.
TEST_F(SyncCredentialsTest, DamagedLocalRecordLosesToAnOlderChange)
{
    const QString guid = addSharedCredential();

    modifyCredential(laptop_, guid, "laptop-user");
    const proto::router::SyncRecordList list = sendRecords(laptop_sync_);

    damageLocalRecord("phone.db3", guid);
    setLocalAge("phone.db3", guid, 20);

    receiveRecords(phone_sync_, list, 25);

    EXPECT_EQ(storedUsername(phone_, guid), QString("laptop-user"));
    EXPECT_TRUE(unsyncedCredentials(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A damaged record of the device has nothing to keep, so an older removal on the other device removes it.
TEST_F(SyncCredentialsTest, DamagedLocalRecordLosesToAnOlderRemoval)
{
    const QString guid = addSharedCredential();

    removeCredential(laptop_, guid);
    const proto::router::SyncRecordList list = sendRecords(laptop_sync_);

    damageLocalRecord("phone.db3", guid);
    setLocalAge("phone.db3", guid, 20);

    receiveRecords(phone_sync_, list, 25);

    QList<CredentialConfig> credentials;
    EXPECT_EQ(CredentialStore(phone_).list(&credentials), Database::ReadResult::OK);
    EXPECT_TRUE(credentials.isEmpty());
    EXPECT_TRUE(unsyncedCredentials(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A newer version of the client has put into the record a field this one does not know.
TEST_F(SyncCredentialsTest, FieldUnknownToTheDeviceComesBackWithItsChange)
{
    const char kGuid[] = "11111111-2222-4333-8444-555555555555";
    const std::string kUnknownField = std::string("\x2a\x06") + "domain";

    proto::router::SyncRecordList list;
    *list.add_record() = newerRecord(kGuid, 1, "user", kUnknownField);
    receiveRecords(laptop_sync_, list, 0);
    ASSERT_EQ(storedUsername(laptop_, kGuid), QString("user"));

    modifyCredential(laptop_, kGuid, "other-user");

    const proto::router::SyncCredential sent = sentContent(laptop_sync_, kGuid);
    EXPECT_EQ(sent.username(), "other-user");
    EXPECT_EQ(sent.unknown_fields(), kUnknownField);
}

//--------------------------------------------------------------------------------------------------
// The device cannot have changed a field it does not know, so the field of the received record is
// the newer one even when the change of the device wins.
TEST_F(SyncCredentialsTest, FieldUnknownToTheDeviceIsTakenFromAReceivedRecordThatLoses)
{
    const char kGuid[] = "11111111-2222-4333-8444-555555555555";
    const std::string kOldField = std::string("\x2a\x04") + "corp";
    const std::string kNewField = std::string("\x2a\x05") + "corp2";

    proto::router::SyncRecordList first;
    *first.add_record() = newerRecord(kGuid, 1, "user", kOldField);
    receiveRecords(laptop_sync_, first, 0);

    modifyCredential(laptop_, kGuid, "laptop-user");

    proto::router::SyncRecordList second;
    *second.add_record() = newerRecord(kGuid, 2, "phone-user", kNewField);
    receiveRecords(laptop_sync_, second, 100);

    EXPECT_EQ(storedUsername(laptop_, kGuid), QString("laptop-user"));

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials(laptop_);
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, 2);

    const proto::router::SyncCredential sent = sentContent(laptop_sync_, kGuid);
    EXPECT_EQ(sent.username(), "laptop-user");
    EXPECT_EQ(sent.unknown_fields(), kNewField);
}

//--------------------------------------------------------------------------------------------------
// The router takes a bounded number of records in one request, so the rest waits for the next one.
TEST_F(SyncCredentialsTest, NoMoreThanARequestHoldsIsSentAtOnce)
{
    for (int i = 0; i < proto::router::kMaxSyncPageSize + 1; ++i)
        addCredential(laptop_, QString("user%1").arg(i));

    EXPECT_EQ(sendRecords(laptop_sync_).record_size(), proto::router::kMaxSyncPageSize);
    EXPECT_EQ(unsyncedCredentials(laptop_).size(), 1);

    EXPECT_EQ(sendRecords(laptop_sync_).record_size(), 1);
    EXPECT_TRUE(unsyncedCredentials(laptop_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncCredentialsTest, OversizedRecordIsNotSentAndTheRestAre)
{
    CredentialConfig oversized;
    oversized.setDisplayName("office");
    oversized.setUsername("user");
    oversized.setPassword(
        SecureString(QString(static_cast<qsizetype>(proto::router::kMaxSyncRecordDataSize), QChar('x'))));
    ASSERT_TRUE(CredentialStore(laptop_).add(oversized));

    const QString guid = addCredential(laptop_, "user").guid();

    const proto::router::SyncRecordList list = sendRecords(laptop_sync_);
    ASSERT_EQ(list.record_size(), 1);
    EXPECT_EQ(list.record(0).id(), guid.toStdString());

    const QList<CredentialStore::Unsynced> unsynced = unsyncedCredentials(laptop_);
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].credential.guid(), oversized.guid());
}

//--------------------------------------------------------------------------------------------------
// The records of the device go only to the storage the device is attached to. Once the device has
// left the storage, as when the synchronization is turned off, nothing is read for sending, though
// every record of the device is new to the router again.
TEST_F(SyncCredentialsTest, RecordsAreNotReadForAStorageTheDeviceHasLeft)
{
    addCredential(laptop_, "user");
    RouterStore store(laptop_);


    RouterConfig router;
    ASSERT_EQ(store.find(&router), Database::FindResult::FOUND);

    router.setSyncEnabled(false);
    ASSERT_TRUE(store.save(router));

    ASSERT_EQ(unsyncedCredentials(laptop_).size(), 1);

    proto::router::SyncRecordRequest request;
    EXPECT_EQ(laptop_sync_.readUnsyncedRecords(&request), SyncCredentials::Result::DETACHED);
    EXPECT_EQ(request.record_size(), 0);
}

//--------------------------------------------------------------------------------------------------
// A page of records is stored as a whole. When one of its records cannot be stored, the device is
// left as it was before the page: with none of its records and with the reading where it was.
TEST_F(SyncCredentialsTest, PageThatFailsHalfwayLeavesNothing)
{
    addCredential(laptop_, "first");
    addCredential(laptop_, "second");

    proto::router::SyncRecordList list = sendRecords(laptop_sync_);
    ASSERT_EQ(list.record_size(), 2);

    list.set_current_time(kRouterTime);
    list.mutable_record(1)->set_revision(0);

    EXPECT_EQ(phone_sync_.storeReceivedRecords(list), SyncCredentials::Result::FAILED);

    QList<CredentialConfig> credentials;
    ASSERT_EQ(CredentialStore(phone_).list(&credentials), Database::ReadResult::OK);
    EXPECT_TRUE(credentials.isEmpty());
    EXPECT_EQ(SyncStore(phone_).receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 0);
}
