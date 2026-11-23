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

#include "client/sync/sync_local_hosts.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/net/udp_channel.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "client/storage/router_store.h"
#include "client/sync/sync_credentials.h"
#include "client/sync/sync_crypto.h"
#include "client/sync/sync_local_groups.h"
#include "client/sync/sync_profiles.h"

namespace {

const qint64 kRouterTime = 1000000;
const char kStorageId[] = "0123456789abcdef";
const char kGuid[] = "11111111-2222-4333-8444-555555555555";
const char kGroupGuid[] = "22222222-3333-4444-8555-666666666666";

class SyncLocalHostsTest : public testing::Test
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

    static qint64 addCredential(Database& db, const QString& name)
    {
        CredentialConfig credential;
        credential.setDisplayName(name);
        credential.setUsername("user");
        credential.setPassword(SecureString(QString("secret")));

        EXPECT_TRUE(CredentialStore(db).add(credential));
        return credential.id();
    }

    static qint64 addProfile(Database& db, const QString& name)
    {
        ProfileConfig profile;
        profile.setName(name);
        profile.setDesktopConfig(defaultDesktopConfig());
        profile.setUdpMethods(UDP_METHOD_ALL);

        EXPECT_TRUE(ProfileStore(db).add(profile));
        return profile.id();
    }

    static qint64 addGroup(Database& db, const QString& name)
    {
        LocalGroupConfig group;
        group.setName(name);

        EXPECT_TRUE(LocalGroupStore(db).add(group));
        return group.id();
    }

    static LocalHostConfig addHost(Database& db, const QString& name, qint64 group_id = 0)
    {
        LocalHostConfig host;
        host.setName(name);
        host.setAddress("192.168.0.1");
        host.setGroupId(group_id);

        EXPECT_TRUE(LocalHostStore(db).add(host));
        return host;
    }

    static QString credentialGuid(Database& db, qint64 credential_id)
    {
        CredentialConfig credential;
        EXPECT_EQ(CredentialStore(db).find(credential_id, &credential), Database::FindResult::FOUND);
        return credential.guid();
    }

    static QString profileGuid(Database& db, qint64 profile_id)
    {
        ProfileConfig profile;
        EXPECT_EQ(ProfileStore(db).find(profile_id, &profile), Database::FindResult::FOUND);
        return profile.guid();
    }

    static QString groupGuid(Database& db, qint64 group_id)
    {
        LocalGroupConfig group;
        EXPECT_EQ(LocalGroupStore(db).find(group_id, &group), Database::FindResult::FOUND);
        return group.guid();
    }

    // The row of the group of |guid|, or zero when the device has no row for it.
    static qint64 groupRowId(Database& db, const QString& guid)
    {
        qint64 group_id = 0;
        EXPECT_NE(LocalGroupStore(db).findRowIdByGuid(guid, &group_id), Database::FindResult::FAILED);
        return group_id;
    }

    // The host of |guid| as the lists show it, or a host that is not valid when they do not.
    static LocalHostConfig storedHost(Database& db, const QString& guid)
    {
        QList<LocalHostConfig> hosts;
        EXPECT_NE(LocalHostStore(db).all(LocalHostStore::Scope::VISIBLE, &hosts), Database::ReadResult::FAILED);

        for (const LocalHostConfig& host : std::as_const(hosts))
        {
            if (host.guid() == guid)
                return host;
        }

        return LocalHostConfig();
    }

    static void renameHost(Database& db, const QString& guid, const QString& name)
    {
        LocalHostConfig host = storedHost(db, guid);
        host.setName(name);
        ASSERT_TRUE(LocalHostStore(db).modify(host));
    }

    static void removeHost(Database& db, const QString& guid)
    {
        ASSERT_TRUE(LocalHostStore(db).remove(storedHost(db, guid).id()));
    }

    static QList<LocalHostStore::Unsynced> unsyncedHosts(Database& db)
    {
        QList<LocalHostStore::Unsynced> hosts;
        EXPECT_NE(LocalHostStore(db).unsynced(&hosts), Database::ReadResult::FAILED);
        return hosts;
    }

    void rawExec(const QString& file_name, const char* sql, const QString& guid)
    {
        SqlDatabase raw;
        ASSERT_TRUE(raw.open(dir_.filePath(file_name)));

        SqlQuery query(raw, sql);
        query.addText(guid);
        ASSERT_TRUE(query.exec());
    }

    // Stands for the router. It stores what the device has to send and returns the stored records.
    proto::router::SyncRecordList sendRecords(SyncRecords& sync)
    {
        proto::router::SyncRecordRequest request;
        EXPECT_EQ(sync.readUnsyncedRecords(&request), SyncRecords::SyncResult::DONE);

        proto::router::SyncRecordResult result;
        proto::router::SyncRecordList list;

        for (const proto::router::SyncRecord& record : request.record())
        {
            proto::router::SyncRecord* stored = list.add_record();
            stored->CopyFrom(record);
            stored->set_revision(++revision_);

            result.add_record_revision(revision_);
        }

        EXPECT_EQ(sync.confirmSentRecords(result), SyncRecords::SyncResult::DONE);
        return list;
    }

    // Hands the records to the device as stored by the router.
    static void receiveRecords(SyncRecords& sync, proto::router::SyncRecordList list)
    {
        for (proto::router::SyncRecord& record : *list.mutable_record())
            record.set_time(kRouterTime);

        ASSERT_EQ(sync.storeReceivedRecords(list), SyncRecords::SyncResult::DONE);
    }

    // Brings what the laptop has to send to the phone in the order of an exchange. The records a
    // host links come before the host.
    void sendLaptopToPhone()
    {
        receiveRecords(phone_credentials_, sendRecords(laptop_credentials_));
        receiveRecords(phone_profiles_, sendRecords(laptop_profiles_));
        receiveRecords(phone_groups_, sendRecords(laptop_groups_));
        receiveRecords(phone_hosts_, sendRecords(laptop_hosts_));
    }

    void sendPhoneToLaptop()
    {
        receiveRecords(laptop_credentials_, sendRecords(phone_credentials_));
        receiveRecords(laptop_profiles_, sendRecords(phone_profiles_));
        receiveRecords(laptop_groups_, sendRecords(phone_groups_));
        receiveRecords(laptop_hosts_, sendRecords(phone_hosts_));
    }

    // A record a newer version of the client has made. It carries |unknown_field|, a field this
    // version does not know.
    proto::router::SyncRecord newerRecord(const char* guid, qint64 revision, const char* name,
                                          const char* group_guid, const std::string& unknown_field) const
    {
        proto::router::SyncLocalHost content;
        content.set_group_guid(group_guid);
        content.set_name(name);
        content.set_address("192.168.0.1");

        const QByteArray plain = serialize(content) + QByteArray::fromStdString(unknown_field);
        const std::optional<QByteArray> sealed =
            SyncCrypto::encryptRecord(key_, proto::router::SYNC_DATA_TYPE_LOCAL_HOSTS, guid, plain);
        EXPECT_TRUE(sealed.has_value());

        proto::router::SyncRecord record;
        record.set_revision(revision);
        record.set_type(proto::router::SYNC_DATA_TYPE_LOCAL_HOSTS);
        record.set_id(guid);
        record.set_data(sealed.value_or(QByteArray()).toStdString());
        return record;
    }

    // The fields of the host the device has to send, opened.
    proto::router::SyncLocalHost sentContent(SyncRecords& sync, const QString& guid)
    {
        proto::router::SyncRecordRequest request;
        EXPECT_EQ(sync.readUnsyncedRecords(&request), SyncRecords::SyncResult::DONE);
        EXPECT_EQ(request.record_size(), 1);

        proto::router::SyncLocalHost content;
        if (request.record_size() != 1)
            return content;

        const std::optional<QByteArray> opened = SyncCrypto::decryptRecord(
            key_, proto::router::SYNC_DATA_TYPE_LOCAL_HOSTS, guid.toStdString(),
            QByteArray::fromStdString(request.record(0).data()));
        EXPECT_TRUE(opened.has_value());
        EXPECT_TRUE(parse(opened.value_or(QByteArray()), &content));
        return content;
    }

    // Brings a host to both devices and returns its guid.
    QString addSharedHost(const QString& name, qint64 group_id = 0)
    {
        const QString guid = addHost(laptop_, name, group_id).guid();
        sendLaptopToPhone();

        EXPECT_EQ(storedHost(phone_, guid).name(), name);
        return guid;
    }

    QTemporaryDir dir_;
    Database laptop_;
    Database phone_;
    const SecureByteArray key_ = SyncCrypto::generateStorageKey();
    SyncCredentials laptop_credentials_{ laptop_, kStorageId, key_ };
    SyncCredentials phone_credentials_{ phone_, kStorageId, key_ };
    SyncProfiles laptop_profiles_{ laptop_, kStorageId, key_ };
    SyncProfiles phone_profiles_{ phone_, kStorageId, key_ };
    SyncLocalGroups laptop_groups_{ laptop_, kStorageId, key_ };
    SyncLocalGroups phone_groups_{ phone_, kStorageId, key_ };
    SyncLocalHosts laptop_hosts_{ laptop_, kStorageId, key_ };
    SyncLocalHosts phone_hosts_{ phone_, kStorageId, key_ };
    qint64 revision_ = 0;
};

} // namespace

//--------------------------------------------------------------------------------------------------
// A host goes to the router sealed. It comes to the other device with its fields and its links,
// which point there to the records of that device.
TEST_F(SyncLocalHostsTest, NewHostReachesTheOtherDeviceWithItsLinks)
{
    const qint64 group_id = addGroup(laptop_, "office");
    const qint64 credential_id = addCredential(laptop_, "office");
    const qint64 os_credential_id = addCredential(laptop_, "windows");
    const qint64 profile_id = addProfile(laptop_, "office");

    LocalHostConfig host;
    host.setName("host");
    host.setComment("comment");
    host.setAddress("192.168.0.1");
    host.setUsername("user");
    host.setPassword(SecureString(QString("secret")));
    host.setGroupId(group_id);
    host.setCredentialId(credential_id);
    host.setOsCredentialId(os_credential_id);
    host.setProfileId(profile_id);
    ASSERT_TRUE(LocalHostStore(laptop_).add(host));

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_hosts_.readUnsyncedRecords(&request), SyncRecords::SyncResult::DONE);
    ASSERT_EQ(request.record_size(), 1);
    EXPECT_EQ(request.record(0).revision(), 0);
    EXPECT_EQ(request.record(0).type(), proto::router::SYNC_DATA_TYPE_LOCAL_HOSTS);
    EXPECT_EQ(request.record(0).id(), host.guid().toStdString());
    EXPECT_FALSE(request.record(0).deleted());
    EXPECT_EQ(request.record(0).data().find("secret"), std::string::npos);
    EXPECT_EQ(request.record(0).data().find("192.168.0.1"), std::string::npos);

    sendLaptopToPhone();

    const LocalHostConfig received = storedHost(phone_, host.guid());
    ASSERT_TRUE(received.isValid());
    EXPECT_EQ(received.name(), QString("host"));
    EXPECT_EQ(received.comment(), QString("comment"));
    EXPECT_EQ(received.address(), QString("192.168.0.1"));
    EXPECT_EQ(received.username(), QString("user"));
    EXPECT_EQ(received.password().toString(), QString("secret"));
    EXPECT_EQ(groupGuid(phone_, received.groupId()), groupGuid(laptop_, group_id));
    EXPECT_EQ(credentialGuid(phone_, received.credentialId()), credentialGuid(laptop_, credential_id));
    EXPECT_EQ(credentialGuid(phone_, received.osCredentialId()), credentialGuid(laptop_, os_credential_id));
    EXPECT_EQ(profileGuid(phone_, received.profileId()), profileGuid(laptop_, profile_id));
    EXPECT_EQ(received.createTime(), host.createTime());
    EXPECT_EQ(received.modifyTime(), kRouterTime);

    EXPECT_TRUE(unsyncedHosts(laptop_).isEmpty());
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());
    EXPECT_EQ(SyncStore(phone_).receivedRevision(proto::router::SYNC_DATA_TYPE_LOCAL_HOSTS), revision_);
}

//--------------------------------------------------------------------------------------------------
// A host may come before the group it lies in. It waits under the row kept for that group and is
// in the group once the group comes.
TEST_F(SyncLocalHostsTest, HostThatCameBeforeItsGroupIsInPlaceOnceTheGroupComes)
{
    const qint64 group_id = addGroup(laptop_, "office");
    const LocalHostConfig host = addHost(laptop_, "host", group_id);

    receiveRecords(phone_hosts_, sendRecords(laptop_hosts_));

    LocalHostStore host_store(phone_);
    LocalGroupStore group_store(phone_);

    QList<LocalHostConfig> root_hosts;
    EXPECT_EQ(host_store.list(0, &root_hosts), Database::ReadResult::OK);
    EXPECT_TRUE(root_hosts.isEmpty());

    LocalGroupConfig group;
    EXPECT_EQ(group_store.find(storedHost(phone_, host.guid()).groupId(), &group),
              Database::FindResult::NOT_FOUND);
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());

    receiveRecords(phone_groups_, sendRecords(laptop_groups_));

    ASSERT_EQ(group_store.find(storedHost(phone_, host.guid()).groupId(), &group),
              Database::FindResult::FOUND);
    EXPECT_EQ(group.name(), QString("office"));
    EXPECT_EQ(group.guid(), groupGuid(laptop_, group_id));
}

//--------------------------------------------------------------------------------------------------
// A move changes the group a record names, and the other device moves its host the same way.
TEST_F(SyncLocalHostsTest, MovedHostIsMovedOnTheOtherDevice)
{
    const qint64 group_id = addGroup(laptop_, "archive");
    const QString group_guid = groupGuid(laptop_, group_id);
    const QString guid = addSharedHost("host");

    ASSERT_TRUE(LocalHostStore(laptop_).move(storedHost(laptop_, guid).id(), group_id));

    EXPECT_EQ(sentContent(laptop_hosts_, guid).group_guid(), group_guid.toStdString());

    sendLaptopToPhone();

    EXPECT_EQ(storedHost(phone_, guid).groupId(), groupRowId(phone_, group_guid));
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A host removed on one device goes to the router as removed and is removed on the other device.
TEST_F(SyncLocalHostsTest, RemovalReachesTheOtherDevice)
{
    const QString guid = addSharedHost("host");
    removeHost(laptop_, guid);

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_hosts_.readUnsyncedRecords(&request), SyncRecords::SyncResult::DONE);
    ASSERT_EQ(request.record_size(), 1);
    EXPECT_EQ(request.record(0).id(), guid.toStdString());
    EXPECT_TRUE(request.record(0).deleted());
    EXPECT_TRUE(request.record(0).data().empty());

    sendLaptopToPhone();

    EXPECT_FALSE(storedHost(phone_, guid).isValid());
    EXPECT_TRUE(unsyncedHosts(laptop_).isEmpty());
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Removing a group removes the hosts that lie in it, and so it does on the other device.
TEST_F(SyncLocalHostsTest, HostOfARemovedGroupIsRemovedOnTheOtherDevice)
{
    const qint64 group_id = addGroup(laptop_, "office");
    const QString group_guid = groupGuid(laptop_, group_id);
    const QString guid = addSharedHost("host", group_id);

    ASSERT_TRUE(LocalGroupStore(laptop_).remove(group_id));
    sendLaptopToPhone();

    EXPECT_FALSE(storedHost(phone_, guid).isValid());
    EXPECT_EQ(groupRowId(phone_, group_guid), 0);
    EXPECT_TRUE(unsyncedHosts(laptop_).isEmpty());
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A group removed on one device is removed with what lies in it, a host another device has put
// there meanwhile included.
TEST_F(SyncLocalHostsTest, HostAddedIntoAGroupRemovedOnAnotherDeviceIsRemovedOnBoth)
{
    const qint64 group_id = addGroup(laptop_, "office");
    const QString group_guid = groupGuid(laptop_, group_id);
    sendLaptopToPhone();

    const QString guid = addHost(phone_, "host", groupRowId(phone_, group_guid)).guid();
    ASSERT_TRUE(LocalGroupStore(laptop_).remove(group_id));

    sendPhoneToLaptop();
    EXPECT_FALSE(storedHost(laptop_, guid).isValid());

    sendLaptopToPhone();

    EXPECT_FALSE(storedHost(phone_, guid).isValid());
    EXPECT_EQ(groupRowId(phone_, group_guid), 0);
    EXPECT_TRUE(unsyncedHosts(laptop_).isEmpty());
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Both devices renamed the host. The name the router holds replaces the one of the device that
// came second, and that one is not sent.
TEST_F(SyncLocalHostsTest, LocalChangeGivesWayToTheRouter)
{
    const QString guid = addSharedHost("host");

    renameHost(phone_, guid, "phone");
    renameHost(laptop_, guid, "laptop");
    sendLaptopToPhone();

    EXPECT_EQ(storedHost(phone_, guid).name(), QString("laptop"));
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A host removed on one device and changed on another meanwhile comes back as the router holds it.
TEST_F(SyncLocalHostsTest, ReceivedChangeBringsBackAHostRemovedOnTheDevice)
{
    const QString guid = addSharedHost("host");

    removeHost(phone_, guid);
    renameHost(laptop_, guid, "renamed");
    sendLaptopToPhone();

    EXPECT_EQ(storedHost(phone_, guid).name(), QString("renamed"));
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A received host that does not open loses to the one of the device, and the reading goes on. A
// record that is refused leaves no row for the group it names.
TEST_F(SyncLocalHostsTest, ReceivedHostThatDoesNotOpenLoses)
{
    const QString guid = addSharedHost("host");

    proto::router::SyncRecordList list;

    proto::router::SyncRecord* damaged = list.add_record();
    damaged->set_revision(50);
    damaged->set_type(proto::router::SYNC_DATA_TYPE_LOCAL_HOSTS);
    damaged->set_id(guid.toStdString());
    damaged->set_data("damaged");

    *list.add_record() = newerRecord(kGuid, 51, "", kGroupGuid, std::string());

    receiveRecords(phone_hosts_, list);

    EXPECT_EQ(storedHost(phone_, guid).name(), QString("host"));
    EXPECT_EQ(SyncStore(phone_).receivedRevision(proto::router::SYNC_DATA_TYPE_LOCAL_HOSTS), 51);
    EXPECT_FALSE(storedHost(phone_, kGuid).isValid());
    EXPECT_EQ(groupRowId(phone_, kGroupGuid), 0);
}

//--------------------------------------------------------------------------------------------------
// A newer version of the client has put into the host a field this one does not know.
TEST_F(SyncLocalHostsTest, FieldUnknownToTheDeviceComesBackWithItsChange)
{
    const std::string kUnknownField = std::string("\x7a\x06") + "future";

    proto::router::SyncRecordList list;
    *list.add_record() = newerRecord(kGuid, 1, "host", "", kUnknownField);
    receiveRecords(laptop_hosts_, list);
    ASSERT_EQ(storedHost(laptop_, kGuid).name(), QString("host"));

    renameHost(laptop_, kGuid, "renamed");

    const proto::router::SyncLocalHost sent = sentContent(laptop_hosts_, kGuid);
    EXPECT_EQ(sent.name(), "renamed");
    EXPECT_EQ(sent.unknown_fields(), kUnknownField);
}

//--------------------------------------------------------------------------------------------------
// A host whose data does not open would reach the other devices with its address lost, so it is
// not sent.
TEST_F(SyncLocalHostsTest, DamagedHostIsNotSent)
{
    const QString guid = addHost(laptop_, "host").guid();
    rawExec("laptop.db3", "UPDATE local_hosts SET data=X'00' WHERE guid=?", guid);

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_hosts_.readUnsyncedRecords(&request), SyncRecords::SyncResult::DONE);
    EXPECT_EQ(request.record_size(), 0);
}
