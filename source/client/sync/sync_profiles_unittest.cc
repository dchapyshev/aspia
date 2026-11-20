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

#include "client/sync/sync_profiles.h"

#include <QDateTime>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/net/udp_channel.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "client/storage/router_store.h"
#include "client/sync/sync_crypto.h"

namespace {

const qint64 kRouterTime = 1000000;
const char kStorageId[] = "0123456789abcdef";
const char kGuid[] = "11111111-2222-4333-8444-555555555555";

class SyncProfilesTest : public testing::Test
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

    static ProfileConfig addProfile(Database& db, const QString& name)
    {
        ProfileConfig profile;
        profile.setName(name);
        profile.setDesktopConfig(defaultDesktopConfig());
        profile.setUdpMethods(UDP_METHOD_ALL);

        EXPECT_TRUE(ProfileStore(db).add(profile));
        return profile;
    }

    static ProfileConfig storedProfile(Database& db, const QString& guid)
    {
        ProfileConfig profile;
        EXPECT_EQ(ProfileStore(db).findByGuid(guid, &profile), Database::FindResult::FOUND);
        return profile;
    }

    static void renameProfile(Database& db, const QString& guid, const QString& name)
    {
        ProfileStore store(db);

        ProfileConfig profile;
        ASSERT_EQ(store.findByGuid(guid, &profile), Database::FindResult::FOUND);

        profile.setName(name);
        ASSERT_TRUE(store.modify(profile));
    }

    static void setRecordingPath(Database& db, const QString& guid, const char* path)
    {
        ProfileStore store(db);

        ProfileConfig profile;
        ASSERT_EQ(store.findByGuid(guid, &profile), Database::FindResult::FOUND);

        proto::control::Config desktop_config = profile.desktopConfig();
        desktop_config.set_recording_path(path);
        profile.setDesktopConfig(desktop_config);
        ASSERT_TRUE(store.modify(profile));
    }

    static QList<ProfileStore::Unsynced> unsyncedProfiles(Database& db)
    {
        QList<ProfileStore::Unsynced> profiles;
        EXPECT_NE(ProfileStore(db).unsynced(&profiles), Database::ReadResult::FAILED);
        return profiles;
    }

    void rawExec(const QString& file_name, const char* sql, const QString& guid, qint64 value = 0)
    {
        SqlDatabase raw;
        ASSERT_TRUE(raw.open(dir_.filePath(file_name)));

        SqlQuery query(raw, sql);
        if (value != 0)
            query.addInt64(value);
        query.addText(guid);
        ASSERT_TRUE(query.exec());
    }

    // Makes the last change of the profile |age| seconds old.
    void setLocalAge(const QString& file_name, const QString& guid, qint64 age)
    {
        rawExec(file_name, "UPDATE profiles SET modify_time=? WHERE guid=?", guid,
                QDateTime::currentSecsSinceEpoch() - age);
    }

    // Stands for the router: stores what the device has to send and returns the stored records.
    proto::router::SyncRecordList sendRecords(SyncProfiles& sync)
    {
        proto::router::SyncRecordRequest request;
        EXPECT_EQ(sync.readUnsyncedRecords(&request), SyncProfiles::Result::DONE);

        proto::router::SyncRecordResult result;
        proto::router::SyncRecordList list;

        for (const proto::router::SyncRecord& record : request.record())
        {
            proto::router::SyncRecord* stored = list.add_record();
            stored->CopyFrom(record);
            stored->set_revision(++revision_);

            result.add_record_revision(revision_);
        }

        EXPECT_EQ(sync.confirmSentRecords(result), SyncProfiles::Result::DONE);
        return list;
    }

    // Hands the records to the device as stored by the router |age| seconds ago.
    static void receiveRecords(SyncProfiles& sync, proto::router::SyncRecordList list, qint64 age)
    {
        list.set_current_time(kRouterTime);

        for (proto::router::SyncRecord& record : *list.mutable_record())
            record.set_time(kRouterTime - age);

        ASSERT_EQ(sync.storeReceivedRecords(list), SyncProfiles::Result::DONE);
    }

    // A record a newer version of the client has made: it carries |unknown_field|, a field this
    // version does not know.
    proto::router::SyncRecord newerRecord(
        const char* guid, qint64 revision, const char* name, const std::string& unknown_field) const
    {
        proto::router::SyncProfile content;
        content.set_name(name);
        content.set_udp_methods(UDP_METHOD_ALL);
        content.mutable_desktop()->CopyFrom(defaultDesktopConfig());

        const QByteArray plain = serialize(content) + QByteArray::fromStdString(unknown_field);
        const std::optional<QByteArray> sealed =
            SyncCrypto::encryptRecord(key_, proto::router::SYNC_DATA_TYPE_PROFILES, guid, plain);
        EXPECT_TRUE(sealed.has_value());

        proto::router::SyncRecord record;
        record.set_revision(revision);
        record.set_type(proto::router::SYNC_DATA_TYPE_PROFILES);
        record.set_id(guid);
        record.set_data(sealed.value_or(QByteArray()).toStdString());
        return record;
    }

    // The fields of the profile the device has to send, opened.
    proto::router::SyncProfile sentContent(SyncProfiles& sync, const QString& guid)
    {
        proto::router::SyncRecordRequest request;
        EXPECT_EQ(sync.readUnsyncedRecords(&request), SyncProfiles::Result::DONE);
        EXPECT_EQ(request.record_size(), 1);

        proto::router::SyncProfile content;
        if (request.record_size() != 1)
            return content;

        const std::optional<QByteArray> opened = SyncCrypto::decryptRecord(
            key_, proto::router::SYNC_DATA_TYPE_PROFILES, guid.toStdString(),
            QByteArray::fromStdString(request.record(0).data()));
        EXPECT_TRUE(opened.has_value());
        EXPECT_TRUE(parse(opened.value_or(QByteArray()), &content));
        return content;
    }

    // Brings a profile to both devices and returns its guid.
    QString addSharedProfile()
    {
        const QString guid = addProfile(laptop_, "office").guid();
        receiveRecords(phone_sync_, sendRecords(laptop_sync_), 0);

        EXPECT_EQ(storedProfile(phone_, guid).name(), QString("office"));
        return guid;
    }

    QTemporaryDir dir_;
    Database laptop_;
    Database phone_;
    const SecureByteArray key_ = SyncCrypto::generateStorageKey();
    SyncProfiles laptop_sync_{ laptop_, kStorageId, key_ };
    SyncProfiles phone_sync_{ phone_, kStorageId, key_ };
    qint64 revision_ = 0;
};

} // namespace

//--------------------------------------------------------------------------------------------------
// A profile made on one device goes to the router sealed and comes to the other one with its parameters.
TEST_F(SyncProfilesTest, NewProfileReachesTheOtherDevice)
{
    const ProfileConfig profile = addProfile(laptop_, "office");

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_sync_.readUnsyncedRecords(&request), SyncProfiles::Result::DONE);
    ASSERT_EQ(request.record_size(), 1);
    EXPECT_EQ(request.record(0).revision(), 0);
    EXPECT_EQ(request.record(0).type(), proto::router::SYNC_DATA_TYPE_PROFILES);
    EXPECT_EQ(request.record(0).id(), profile.guid().toStdString());
    EXPECT_FALSE(request.record(0).deleted());
    EXPECT_EQ(request.record(0).data().find("office"), std::string::npos);

    const proto::router::SyncRecordList list = sendRecords(laptop_sync_);
    ASSERT_EQ(list.record_size(), 1);
    EXPECT_TRUE(unsyncedProfiles(laptop_).isEmpty());

    receiveRecords(phone_sync_, list, 0);

    const ProfileConfig received = storedProfile(phone_, profile.guid());
    EXPECT_EQ(received.name(), QString("office"));
    EXPECT_EQ(received.udpMethods(), profile.udpMethods());
    EXPECT_EQ(serialize(received.desktopConfig()), serialize(profile.desktopConfig()));
    EXPECT_TRUE(unsyncedProfiles(phone_).isEmpty());
    EXPECT_EQ(SyncStore(phone_).receivedRevision(proto::router::SYNC_DATA_TYPE_PROFILES), list.record(0).revision());
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncProfilesTest, UntouchedDefaultProfileIsNotSent)
{
    EXPECT_EQ(sendRecords(laptop_sync_).record_size(), 0);
}

//--------------------------------------------------------------------------------------------------
// An edited default profile replaces the untouched one of the other device, which has a single default profile still.
TEST_F(SyncProfilesTest, EditedDefaultProfileReplacesTheUntouchedOne)
{
    ProfileStore laptop_store(laptop_);

    ProfileConfig default_profile = laptop_store.defaultProfile();
    default_profile.setUdpMethods(UDP_METHOD_DIRECT);
    ASSERT_TRUE(laptop_store.modify(default_profile));

    const proto::router::SyncRecordList list = sendRecords(laptop_sync_);
    ASSERT_EQ(list.record_size(), 1);
    EXPECT_EQ(list.record(0).id(), std::string(ProfileConfig::kDefaultGuid));

    receiveRecords(phone_sync_, list, 0);

    ProfileStore phone_store(phone_);

    EXPECT_EQ(phone_store.defaultProfile().udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));

    QList<ProfileConfig> profiles;
    ASSERT_EQ(phone_store.list(&profiles), Database::ReadResult::OK);
    EXPECT_EQ(profiles.size(), 1);
}

//--------------------------------------------------------------------------------------------------
// The path of the recordings belongs to the device: it is not sent and is not taken from a received profile.
TEST_F(SyncProfilesTest, RecordingPathStaysOnTheDevice)
{
    const QString guid = addSharedProfile();

    setRecordingPath(phone_, guid, "D:/phone");
    EXPECT_TRUE(sentContent(phone_sync_, guid).desktop().recording_path().empty());

    receiveRecords(laptop_sync_, sendRecords(phone_sync_), 0);
    EXPECT_TRUE(storedProfile(laptop_, guid).desktopConfig().recording_path().empty());

    renameProfile(laptop_, guid, "renamed");
    receiveRecords(phone_sync_, sendRecords(laptop_sync_), 0);

    const ProfileConfig received = storedProfile(phone_, guid);
    EXPECT_EQ(received.name(), QString("renamed"));
    EXPECT_EQ(received.desktopConfig().recording_path(), "D:/phone");
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncProfilesTest, RemovalReachesTheOtherDevice)
{
    const QString guid = addSharedProfile();
    ASSERT_TRUE(ProfileStore(laptop_).remove(storedProfile(laptop_, guid).id()));

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_sync_.readUnsyncedRecords(&request), SyncProfiles::Result::DONE);
    ASSERT_EQ(request.record_size(), 1);
    EXPECT_EQ(request.record(0).revision(), 1);
    EXPECT_TRUE(request.record(0).deleted());
    EXPECT_TRUE(request.record(0).data().empty());

    receiveRecords(phone_sync_, sendRecords(laptop_sync_), 0);

    ProfileConfig removed;
    EXPECT_EQ(ProfileStore(phone_).findByGuid(guid, &removed), Database::FindResult::NOT_FOUND);
    EXPECT_TRUE(unsyncedProfiles(laptop_).isEmpty());
    EXPECT_TRUE(unsyncedProfiles(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The default profile is never removed: the device keeps it and takes the revision, so that its next edit is stored.
TEST_F(SyncProfilesTest, DefaultProfileReceivedAsRemovedStaysOnTheDevice)
{
    proto::router::SyncRecordList list;

    proto::router::SyncRecord* removed = list.add_record();
    removed->set_revision(5);
    removed->set_type(proto::router::SYNC_DATA_TYPE_PROFILES);
    removed->set_id(ProfileConfig::kDefaultGuid);
    removed->set_deleted(true);

    receiveRecords(phone_sync_, list, 0);

    ProfileStore store(phone_);

    ProfileConfig default_profile;
    ASSERT_EQ(store.findByGuid(ProfileConfig::kDefaultGuid, &default_profile), Database::FindResult::FOUND);
    EXPECT_EQ(SyncStore(phone_).receivedRevision(proto::router::SYNC_DATA_TYPE_PROFILES), 5);

    default_profile = store.defaultProfile();
    default_profile.setUdpMethods(UDP_METHOD_DIRECT);
    ASSERT_TRUE(store.modify(default_profile));

    const QList<ProfileStore::Unsynced> unsynced = unsyncedProfiles(phone_);
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, 5);
}

//--------------------------------------------------------------------------------------------------
// A received profile that does not open loses to the one of the device, and the reading goes on.
TEST_F(SyncProfilesTest, ReceivedProfileThatDoesNotOpenLoses)
{
    const QString guid = addSharedProfile();

    proto::router::SyncRecordList list;

    proto::router::SyncRecord* damaged = list.add_record();
    damaged->set_revision(5);
    damaged->set_type(proto::router::SYNC_DATA_TYPE_PROFILES);
    damaged->set_id(guid.toStdString());
    damaged->set_data("damaged");

    *list.add_record() = newerRecord(kGuid, 6, "", std::string());

    receiveRecords(phone_sync_, list, 0);

    EXPECT_EQ(storedProfile(phone_, guid).name(), QString("office"));
    EXPECT_EQ(SyncStore(phone_).receivedRevision(proto::router::SYNC_DATA_TYPE_PROFILES), 6);

    ProfileConfig unnamed;
    EXPECT_EQ(ProfileStore(phone_).findByGuid(kGuid, &unnamed), Database::FindResult::NOT_FOUND);
}

//--------------------------------------------------------------------------------------------------
// Both devices renamed the profile, and the younger change wins.
TEST_F(SyncProfilesTest, YoungerChangeWins)
{
    const QString guid = addSharedProfile();

    renameProfile(phone_, guid, "phone");
    setLocalAge("phone.db3", guid, 30);

    renameProfile(laptop_, guid, "laptop");
    receiveRecords(phone_sync_, sendRecords(laptop_sync_), 25);

    EXPECT_EQ(storedProfile(phone_, guid).name(), QString("laptop"));
    EXPECT_TRUE(unsyncedProfiles(phone_).isEmpty());

    renameProfile(phone_, guid, "phone");
    setLocalAge("phone.db3", guid, 20);

    renameProfile(laptop_, guid, "other-laptop");
    receiveRecords(phone_sync_, sendRecords(laptop_sync_), 25);

    EXPECT_EQ(storedProfile(phone_, guid).name(), QString("phone"));

    const QList<ProfileStore::Unsynced> unsynced = unsyncedProfiles(phone_);
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, revision_);
}

//--------------------------------------------------------------------------------------------------
// A newer version of the client has put into the profile a field this one does not know.
TEST_F(SyncProfilesTest, FieldUnknownToTheDeviceComesBackWithItsChange)
{
    const std::string kUnknownField = std::string("\x2a\x06") + "future";

    proto::router::SyncRecordList list;
    *list.add_record() = newerRecord(kGuid, 1, "office", kUnknownField);
    receiveRecords(laptop_sync_, list, 0);
    ASSERT_EQ(storedProfile(laptop_, kGuid).name(), QString("office"));

    renameProfile(laptop_, kGuid, "renamed");

    const proto::router::SyncProfile sent = sentContent(laptop_sync_, kGuid);
    EXPECT_EQ(sent.name(), "renamed");
    EXPECT_EQ(sent.unknown_fields(), kUnknownField);
}

//--------------------------------------------------------------------------------------------------
// A profile whose data does not read would reach the other devices with the parameters lost, so it is not sent.
TEST_F(SyncProfilesTest, DamagedProfileIsNotSent)
{
    const QString guid = addProfile(laptop_, "office").guid();
    rawExec("laptop.db3", "UPDATE profiles SET data=X'00' WHERE guid=?", guid);

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_sync_.readUnsyncedRecords(&request), SyncProfiles::Result::DONE);
    EXPECT_EQ(request.record_size(), 0);
}
