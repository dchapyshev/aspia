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

#include "client/sync/sync_router_hosts.h"

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
#include "client/sync/sync_profiles.h"

namespace {

const qint64 kRouterTime = 1000000;
const char kStorageId[] = "0123456789abcdef";
const HostId kHostId = 100500;
const char kRecordId[] = "100500";

class SyncRouterHostsTest : public testing::Test
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

    static RouterHostConfig routerHost(const QString& username, const QString& password)
    {
        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername(username);
        host.setPassword(SecureString(password));
        return host;
    }

    static void setUsername(Database& db, const QString& username)
    {
        ASSERT_TRUE(RouterHostStore(db).modify(routerHost(username, "secret")));
    }

    static std::optional<RouterHostConfig> storedHost(Database& db)
    {
        RouterHostConfig host;
        if (RouterHostStore(db).find(kHostId, &host) != Database::FindResult::FOUND)
            return std::nullopt;

        return host;
    }

    static QList<RouterHostStore::Unsynced> unsyncedHosts(Database& db)
    {
        QList<RouterHostStore::Unsynced> hosts;
        EXPECT_NE(RouterHostStore(db).unsynced(&hosts), Database::ReadResult::FAILED);
        return hosts;
    }

    void rawExec(const QString& file_name, const char* sql)
    {
        SqlDatabase raw;
        ASSERT_TRUE(raw.open(dir_.filePath(file_name)));

        SqlQuery query(raw, sql);
        query.addUInt64(kHostId);
        ASSERT_TRUE(query.exec());
    }

    // Stands for the router: stores what the device has to send and returns the stored records.
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

    // Brings what the laptop has to send to the phone in the order of an exchange: the records a
    // host links come before the host.
    void sendLaptopToPhone()
    {
        receiveRecords(phone_credentials_, sendRecords(laptop_credentials_));
        receiveRecords(phone_profiles_, sendRecords(laptop_profiles_));
        receiveRecords(phone_hosts_, sendRecords(laptop_hosts_));
    }

    // A record of the host a newer version of the client has made: it carries |unknown_field|, a
    // field this version does not know.
    proto::router::SyncRecord newerRecord(
        qint64 revision, const char* username, const std::string& unknown_field) const
    {
        proto::router::SyncRouterHost content;
        content.set_username(username);
        content.set_password("secret");

        const QByteArray plain = serialize(content) + QByteArray::fromStdString(unknown_field);
        const std::optional<QByteArray> sealed =
            SyncCrypto::encryptRecord(key_, proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS, kRecordId, plain);
        EXPECT_TRUE(sealed.has_value());

        proto::router::SyncRecord record;
        record.set_revision(revision);
        record.set_type(proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS);
        record.set_id(kRecordId);
        record.set_data(sealed.value_or(QByteArray()).toStdString());
        return record;
    }

    // The fields of the host the device has to send, opened.
    proto::router::SyncRouterHost sentContent(SyncRecords& sync)
    {
        proto::router::SyncRecordRequest request;
        EXPECT_EQ(sync.readUnsyncedRecords(&request), SyncRecords::SyncResult::DONE);
        EXPECT_EQ(request.record_size(), 1);

        proto::router::SyncRouterHost content;
        if (request.record_size() != 1)
            return content;

        const std::optional<QByteArray> opened = SyncCrypto::decryptRecord(
            key_, proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS, kRecordId,
            QByteArray::fromStdString(request.record(0).data()));
        EXPECT_TRUE(opened.has_value());
        EXPECT_TRUE(parse(opened.value_or(QByteArray()), &content));
        return content;
    }

    // Brings a host with its own pair to both devices.
    void addSharedHost()
    {
        ASSERT_TRUE(RouterHostStore(laptop_).add(routerHost("user", "secret")));
        receiveRecords(phone_hosts_, sendRecords(laptop_hosts_));
        ASSERT_TRUE(storedHost(phone_).has_value());
    }

    QTemporaryDir dir_;
    Database laptop_;
    Database phone_;
    const SecureByteArray key_ = SyncCrypto::generateStorageKey();
    SyncCredentials laptop_credentials_{ laptop_, kStorageId, key_ };
    SyncCredentials phone_credentials_{ phone_, kStorageId, key_ };
    SyncProfiles laptop_profiles_{ laptop_, kStorageId, key_ };
    SyncProfiles phone_profiles_{ phone_, kStorageId, key_ };
    SyncRouterHosts laptop_hosts_{ laptop_, kStorageId, key_ };
    SyncRouterHosts phone_hosts_{ phone_, kStorageId, key_ };
    qint64 revision_ = 0;
};

} // namespace

//--------------------------------------------------------------------------------------------------
// A host goes to the router sealed and named by the id of the host. It comes to the other device
// with its pair and its links, which point there to the records of that device.
TEST_F(SyncRouterHostsTest, NewHostReachesTheOtherDeviceWithItsLinks)
{
    const qint64 credential_id = addCredential(laptop_, "office");
    const qint64 os_credential_id = addCredential(laptop_, "windows");
    const qint64 profile_id = addProfile(laptop_, "office");

    RouterHostStore store(laptop_);

    RouterHostConfig host = routerHost("user", "secret");
    host.setCredentialId(credential_id);
    ASSERT_TRUE(store.add(host));
    ASSERT_TRUE(store.setOsCredentialId(kHostId, os_credential_id));
    ASSERT_TRUE(store.setProfileId(kHostId, profile_id));

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_hosts_.readUnsyncedRecords(&request), SyncRecords::SyncResult::DONE);
    ASSERT_EQ(request.record_size(), 1);
    EXPECT_EQ(request.record(0).revision(), 0);
    EXPECT_EQ(request.record(0).type(), proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS);
    EXPECT_EQ(request.record(0).id(), kRecordId);
    EXPECT_FALSE(request.record(0).deleted());
    EXPECT_EQ(request.record(0).data().find("secret"), std::string::npos);

    sendLaptopToPhone();

    const std::optional<RouterHostConfig> received = storedHost(phone_);
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->username(), QString("user"));
    EXPECT_EQ(received->password().toString(), QString("secret"));
    EXPECT_EQ(credentialGuid(phone_, received->credentialId()), credentialGuid(laptop_, credential_id));
    EXPECT_EQ(credentialGuid(phone_, received->osCredentialId()), credentialGuid(laptop_, os_credential_id));
    EXPECT_EQ(profileGuid(phone_, received->profileId()), profileGuid(laptop_, profile_id));

    EXPECT_TRUE(unsyncedHosts(laptop_).isEmpty());
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());
    EXPECT_EQ(SyncStore(phone_).receivedRevision(proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS), revision_);
}

//--------------------------------------------------------------------------------------------------
// A host comes before the credentials it links. The rest of the host is taken at once, and the link
// is kept by an empty row, which is neither listed nor sent. The host links the credentials as soon
// as they come.
TEST_F(SyncRouterHostsTest, HostThatCameBeforeItsCredentialsLinksThemOnceTheyCome)
{
    const qint64 credential_id = addCredential(laptop_, "office");

    RouterHostConfig host = routerHost("user", "secret");
    host.setCredentialId(credential_id);
    ASSERT_TRUE(RouterHostStore(laptop_).add(host));

    receiveRecords(phone_hosts_, sendRecords(laptop_hosts_));

    const std::optional<RouterHostConfig> received = storedHost(phone_);
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->username(), QString("user"));
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());

    CredentialStore phone_store(phone_);

    CredentialConfig linked;
    EXPECT_EQ(phone_store.find(received->credentialId(), &linked), Database::FindResult::NOT_FOUND);

    QList<CredentialConfig> listed;
    EXPECT_EQ(phone_store.list(&listed), Database::ReadResult::OK);
    EXPECT_TRUE(listed.isEmpty());

    QList<CredentialStore::Unsynced> unsynced;
    EXPECT_EQ(phone_store.unsynced(&unsynced), Database::ReadResult::OK);
    EXPECT_TRUE(unsynced.isEmpty());

    receiveRecords(phone_credentials_, sendRecords(laptop_credentials_));

    ASSERT_EQ(phone_store.find(received->credentialId(), &linked), Database::FindResult::FOUND);
    EXPECT_EQ(linked.guid(), credentialGuid(laptop_, credential_id));
    EXPECT_EQ(linked.displayName(), QString("office"));
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A host that holds nothing but a link comes before the credentials it links. It is kept with the
// link: left out, it would not be there when the credentials come.
TEST_F(SyncRouterHostsTest, HostWithNothingButALinkIsKeptBeforeItsCredentialsCome)
{
    const qint64 credential_id = addCredential(laptop_, "office");

    RouterHostConfig host = routerHost(QString(), QString());
    host.setCredentialId(credential_id);
    ASSERT_TRUE(RouterHostStore(laptop_).add(host));

    receiveRecords(phone_hosts_, sendRecords(laptop_hosts_));

    const std::optional<RouterHostConfig> received = storedHost(phone_);
    ASSERT_TRUE(received.has_value());
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());
    EXPECT_EQ(SyncStore(phone_).receivedRevision(proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS), revision_);

    receiveRecords(phone_credentials_, sendRecords(laptop_credentials_));

    EXPECT_EQ(credentialGuid(phone_, received->credentialId()), credentialGuid(laptop_, credential_id));
}

//--------------------------------------------------------------------------------------------------
// A host links credentials the device does not have, and the pair of the host is changed there. The
// host goes back to the router with the link it came with.
TEST_F(SyncRouterHostsTest, ChangedHostKeepsTheLinkToCredentialsTheDeviceDoesNotHave)
{
    const qint64 credential_id = addCredential(laptop_, "office");

    RouterHostConfig host = routerHost("user", "secret");
    host.setCredentialId(credential_id);
    ASSERT_TRUE(RouterHostStore(laptop_).add(host));

    receiveRecords(phone_hosts_, sendRecords(laptop_hosts_));

    std::optional<RouterHostConfig> received = storedHost(phone_);
    ASSERT_TRUE(received.has_value());

    received->setUsername("phone");
    ASSERT_TRUE(RouterHostStore(phone_).modify(*received));

    const proto::router::SyncRouterHost sent = sentContent(phone_hosts_);
    EXPECT_EQ(sent.username(), "phone");
    EXPECT_EQ(QString::fromStdString(sent.credential_guid()), credentialGuid(laptop_, credential_id));
}

//--------------------------------------------------------------------------------------------------
// The credentials a host links are removed before they reach the device, so their removal comes
// instead. The empty row that kept the link is gone, and the host stays with its pair and without
// the link.
TEST_F(SyncRouterHostsTest, HostLosesTheLinkWhenTheRemovalOfItsCredentialsComesInstead)
{
    const qint64 credential_id = addCredential(laptop_, "office");
    const QString credential_guid = credentialGuid(laptop_, credential_id);

    RouterHostConfig host = routerHost("user", "secret");
    host.setCredentialId(credential_id);
    ASSERT_TRUE(RouterHostStore(laptop_).add(host));

    sendRecords(laptop_credentials_);
    receiveRecords(phone_hosts_, sendRecords(laptop_hosts_));

    std::optional<RouterHostConfig> received = storedHost(phone_);
    ASSERT_TRUE(received.has_value());
    EXPECT_GT(received->credentialId(), 0);

    ASSERT_TRUE(CredentialStore(laptop_).remove(credential_id));
    receiveRecords(phone_credentials_, sendRecords(laptop_credentials_));

    received = storedHost(phone_);
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->username(), QString("user"));
    EXPECT_EQ(received->credentialId(), 0);
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());

    qint64 row_id = 0;
    EXPECT_EQ(CredentialStore(phone_).findRowIdByGuid(credential_guid, &row_id), Database::FindResult::NOT_FOUND);
}

//--------------------------------------------------------------------------------------------------
// A host removed on one device goes to the router as removed, named by the id of the host, and is
// removed on the other device.
TEST_F(SyncRouterHostsTest, RemovalReachesTheOtherDevice)
{
    addSharedHost();
    ASSERT_TRUE(RouterHostStore(laptop_).remove(kHostId));

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_hosts_.readUnsyncedRecords(&request), SyncRecords::SyncResult::DONE);
    ASSERT_EQ(request.record_size(), 1);
    EXPECT_EQ(request.record(0).id(), kRecordId);
    EXPECT_EQ(request.record(0).revision(), 1);
    EXPECT_TRUE(request.record(0).deleted());
    EXPECT_TRUE(request.record(0).data().empty());

    receiveRecords(phone_hosts_, sendRecords(laptop_hosts_));

    EXPECT_FALSE(storedHost(phone_).has_value());
    EXPECT_TRUE(unsyncedHosts(laptop_).isEmpty());
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The credentials a host links alone are removed on one device. Once the router has stored the
// removal, the host left with nothing is removed there. On the other device the host is gone as
// soon as the removal of the credentials comes, before the removal of the host.
TEST_F(SyncRouterHostsTest, HostLeftWithoutItsCredentialsIsRemovedOnBothDevices)
{
    const qint64 credential_id = addCredential(laptop_, "office");

    RouterHostConfig host = routerHost(QString(), QString());
    host.setCredentialId(credential_id);
    ASSERT_TRUE(RouterHostStore(laptop_).add(host));

    sendLaptopToPhone();
    ASSERT_TRUE(storedHost(phone_).has_value());

    ASSERT_TRUE(CredentialStore(laptop_).remove(credential_id));

    const proto::router::SyncRecordList credentials = sendRecords(laptop_credentials_);
    EXPECT_FALSE(storedHost(laptop_).has_value());

    receiveRecords(phone_credentials_, credentials);
    EXPECT_FALSE(storedHost(phone_).has_value());

    receiveRecords(phone_hosts_, sendRecords(laptop_hosts_));
    EXPECT_FALSE(storedHost(phone_).has_value());
    EXPECT_TRUE(unsyncedHosts(laptop_).isEmpty());
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Both devices changed the pair of the host. The pair the router holds replaces the one of the
// device that came second, and that one is not sent.
TEST_F(SyncRouterHostsTest, LocalChangeGivesWayToTheRouter)
{
    addSharedHost();

    setUsername(phone_, "phone");
    setUsername(laptop_, "laptop");
    receiveRecords(phone_hosts_, sendRecords(laptop_hosts_));

    EXPECT_EQ(storedHost(phone_).value_or(RouterHostConfig()).username(), QString("laptop"));
    EXPECT_TRUE(unsyncedHosts(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A newer version of the client has put into the host a field this one does not know.
TEST_F(SyncRouterHostsTest, FieldUnknownToTheDeviceComesBackWithItsChange)
{
    const std::string kUnknownField = std::string("\x7a\x06") + "future";

    proto::router::SyncRecordList list;
    *list.add_record() = newerRecord(1, "user", kUnknownField);
    receiveRecords(laptop_hosts_, list);
    ASSERT_EQ(storedHost(laptop_).value_or(RouterHostConfig()).username(), QString("user"));

    setUsername(laptop_, "renamed");

    const proto::router::SyncRouterHost sent = sentContent(laptop_hosts_);
    EXPECT_EQ(sent.username(), "renamed");
    EXPECT_EQ(sent.unknown_fields(), kUnknownField);
}

//--------------------------------------------------------------------------------------------------
// A host whose data does not read would reach the other devices with the pair lost, so it is not
// sent.
TEST_F(SyncRouterHostsTest, DamagedHostIsNotSent)
{
    ASSERT_TRUE(RouterHostStore(laptop_).add(routerHost("user", "secret")));
    rawExec("laptop.db3", "UPDATE router_hosts SET data=X'00' WHERE host_id=?");

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_hosts_.readUnsyncedRecords(&request), SyncRecords::SyncResult::DONE);
    EXPECT_EQ(request.record_size(), 0);
}
