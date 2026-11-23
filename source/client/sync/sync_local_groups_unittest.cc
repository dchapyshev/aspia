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

#include "client/sync/sync_local_groups.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "client/storage/router_store.h"
#include "client/sync/sync_crypto.h"

namespace {

const qint64 kRouterTime = 1000000;
const char kStorageId[] = "0123456789abcdef";
const char kGuid[] = "11111111-2222-4333-8444-555555555555";
const char kParentGuid[] = "22222222-3333-4444-8555-666666666666";

class SyncLocalGroupsTest : public testing::Test
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

    static LocalGroupConfig addGroup(Database& db, const QString& name, qint64 parent_id = 0)
    {
        LocalGroupConfig group;
        group.setName(name);
        group.setParentId(parent_id);

        EXPECT_TRUE(LocalGroupStore(db).add(group));
        return group;
    }

    // The group of |guid| as the lists show it, or a group that is not valid when they do not.
    static LocalGroupConfig storedGroup(Database& db, const QString& guid)
    {
        LocalGroupStore store(db);

        qint64 group_id = 0;
        LocalGroupConfig group;
        if (store.findRowIdByGuid(guid, &group_id) == Database::FindResult::FOUND)
            store.find(group_id, &group);

        return group;
    }

    static void renameGroup(Database& db, const QString& guid, const QString& name)
    {
        LocalGroupConfig group = storedGroup(db, guid);
        group.setName(name);
        ASSERT_TRUE(LocalGroupStore(db).modify(group));
    }

    static void moveGroup(Database& db, const QString& guid, const QString& parent_guid)
    {
        ASSERT_TRUE(LocalGroupStore(db).move(storedGroup(db, guid).id(), storedGroup(db, parent_guid).id()));
    }

    static void removeGroup(Database& db, const QString& guid)
    {
        ASSERT_TRUE(LocalGroupStore(db).remove(storedGroup(db, guid).id()));
    }

    static QList<LocalGroupStore::Unsynced> unsyncedGroups(Database& db)
    {
        QList<LocalGroupStore::Unsynced> groups;
        EXPECT_NE(LocalGroupStore(db).unsynced(&groups), Database::ReadResult::FAILED);
        return groups;
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
    proto::router::SyncRecordList sendRecords(SyncLocalGroups& sync)
    {
        proto::router::SyncRecordRequest request;
        EXPECT_EQ(sync.readUnsyncedRecords(&request), SyncLocalGroups::SyncResult::DONE);

        proto::router::SyncRecordResult result;
        proto::router::SyncRecordList list;

        for (const proto::router::SyncRecord& record : request.record())
        {
            proto::router::SyncRecord* stored = list.add_record();
            stored->CopyFrom(record);
            stored->set_revision(++revision_);

            result.add_record_revision(revision_);
        }

        EXPECT_EQ(sync.confirmSentRecords(result), SyncLocalGroups::SyncResult::DONE);
        return list;
    }

    // Hands the records to the device as stored by the router.
    static void receiveRecords(SyncLocalGroups& sync, proto::router::SyncRecordList list)
    {
        for (proto::router::SyncRecord& record : *list.mutable_record())
            record.set_time(kRouterTime);

        ASSERT_EQ(sync.storeReceivedRecords(list), SyncLocalGroups::SyncResult::DONE);
    }

    // A record a newer version of the client has made. It carries |unknown_field|, a field this
    // version does not know.
    proto::router::SyncRecord newerRecord(const char* guid, qint64 revision, const char* name,
                                          const char* parent_guid, const std::string& unknown_field) const
    {
        proto::router::SyncLocalGroup content;
        content.set_parent_guid(parent_guid);
        content.set_name(name);

        const QByteArray plain = serialize(content) + QByteArray::fromStdString(unknown_field);
        const std::optional<QByteArray> sealed =
            SyncCrypto::encryptRecord(key_, proto::router::SYNC_DATA_TYPE_LOCAL_GROUPS, guid, plain);
        EXPECT_TRUE(sealed.has_value());

        proto::router::SyncRecord record;
        record.set_revision(revision);
        record.set_type(proto::router::SYNC_DATA_TYPE_LOCAL_GROUPS);
        record.set_id(guid);
        record.set_data(sealed.value_or(QByteArray()).toStdString());
        return record;
    }

    // The fields of the group the device has to send, opened.
    proto::router::SyncLocalGroup sentContent(SyncLocalGroups& sync, const QString& guid)
    {
        proto::router::SyncRecordRequest request;
        EXPECT_EQ(sync.readUnsyncedRecords(&request), SyncLocalGroups::SyncResult::DONE);
        EXPECT_EQ(request.record_size(), 1);

        proto::router::SyncLocalGroup content;
        if (request.record_size() != 1)
            return content;

        const std::optional<QByteArray> opened = SyncCrypto::decryptRecord(
            key_, proto::router::SYNC_DATA_TYPE_LOCAL_GROUPS, guid.toStdString(),
            QByteArray::fromStdString(request.record(0).data()));
        EXPECT_TRUE(opened.has_value());
        EXPECT_TRUE(parse(opened.value_or(QByteArray()), &content));
        return content;
    }

    // Brings a group to both devices and returns its guid.
    QString addSharedGroup(const QString& name, const QString& parent_guid = QString())
    {
        const qint64 parent_id = parent_guid.isEmpty() ? 0 : storedGroup(laptop_, parent_guid).id();
        const QString guid = addGroup(laptop_, name, parent_id).guid();
        receiveRecords(phone_sync_, sendRecords(laptop_sync_));

        EXPECT_EQ(storedGroup(phone_, guid).name(), name);
        return guid;
    }

    QTemporaryDir dir_;
    Database laptop_;
    Database phone_;
    const SecureByteArray key_ = SyncCrypto::generateStorageKey();
    SyncLocalGroups laptop_sync_{ laptop_, kStorageId, key_ };
    SyncLocalGroups phone_sync_{ phone_, kStorageId, key_ };
    qint64 revision_ = 0;
};

} // namespace

//--------------------------------------------------------------------------------------------------
// A group made on one device goes to the router sealed and comes to the other one with its fields.
TEST_F(SyncLocalGroupsTest, NewGroupReachesTheOtherDevice)
{
    LocalGroupConfig group;
    group.setName("office");
    group.setComment("comment");
    ASSERT_TRUE(LocalGroupStore(laptop_).add(group));

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_sync_.readUnsyncedRecords(&request), SyncLocalGroups::SyncResult::DONE);
    ASSERT_EQ(request.record_size(), 1);
    EXPECT_EQ(request.record(0).revision(), 0);
    EXPECT_EQ(request.record(0).type(), proto::router::SYNC_DATA_TYPE_LOCAL_GROUPS);
    EXPECT_EQ(request.record(0).id(), group.guid().toStdString());
    EXPECT_FALSE(request.record(0).deleted());
    EXPECT_EQ(request.record(0).data().find("office"), std::string::npos);

    const proto::router::SyncRecordList list = sendRecords(laptop_sync_);
    ASSERT_EQ(list.record_size(), 1);
    EXPECT_TRUE(unsyncedGroups(laptop_).isEmpty());

    receiveRecords(phone_sync_, list);

    const LocalGroupConfig received = storedGroup(phone_, group.guid());
    EXPECT_EQ(received.name(), QString("office"));
    EXPECT_EQ(received.comment(), QString("comment"));
    EXPECT_EQ(received.parentId(), 0);
    EXPECT_TRUE(unsyncedGroups(phone_).isEmpty());
    EXPECT_EQ(SyncStore(phone_).receivedRevision(proto::router::SYNC_DATA_TYPE_LOCAL_GROUPS),
              list.record(0).revision());
}

//--------------------------------------------------------------------------------------------------
// A record names the group it lies in by guid, and each device finds that group by its own id.
TEST_F(SyncLocalGroupsTest, SubgroupReachesTheOtherDeviceInsideItsGroup)
{
    addGroup(phone_, "local");

    const LocalGroupConfig office = addGroup(laptop_, "office");
    const LocalGroupConfig branch = addGroup(laptop_, "branch", office.id());

    receiveRecords(phone_sync_, sendRecords(laptop_sync_));

    const LocalGroupConfig received_office = storedGroup(phone_, office.guid());
    ASSERT_TRUE(received_office.isValid());
    EXPECT_NE(received_office.id(), office.id());
    EXPECT_EQ(storedGroup(phone_, branch.guid()).parentId(), received_office.id());
}

//--------------------------------------------------------------------------------------------------
// A group may come before the group it lies in. It waits under the row kept for that group and is
// in its place once the group comes.
TEST_F(SyncLocalGroupsTest, GroupThatCameBeforeItsParentIsInPlaceOnceTheParentComes)
{
    const LocalGroupConfig office = addGroup(laptop_, "office");
    const LocalGroupConfig branch = addGroup(laptop_, "branch", office.id());

    const proto::router::SyncRecordList list = sendRecords(laptop_sync_);
    ASSERT_EQ(list.record_size(), 2);
    ASSERT_EQ(list.record(1).id(), branch.guid().toStdString());

    proto::router::SyncRecordList child_list;
    *child_list.add_record() = list.record(1);
    receiveRecords(phone_sync_, child_list);

    QList<LocalGroupConfig> root_groups;
    ASSERT_TRUE(LocalGroupStore(phone_).list(0, &root_groups));
    EXPECT_TRUE(root_groups.isEmpty());
    EXPECT_FALSE(storedGroup(phone_, office.guid()).isValid());

    proto::router::SyncRecordList parent_list;
    *parent_list.add_record() = list.record(0);
    receiveRecords(phone_sync_, parent_list);

    const LocalGroupConfig received_office = storedGroup(phone_, office.guid());
    EXPECT_EQ(received_office.name(), QString("office"));
    EXPECT_EQ(received_office.parentId(), 0);
    EXPECT_EQ(storedGroup(phone_, branch.guid()).parentId(), received_office.id());
    EXPECT_TRUE(unsyncedGroups(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A move changes the group a record names, and the other device moves its group the same way.
TEST_F(SyncLocalGroupsTest, MovedGroupIsMovedOnTheOtherDevice)
{
    const QString office_guid = addSharedGroup("office");
    const QString archive_guid = addSharedGroup("archive");

    moveGroup(laptop_, office_guid, archive_guid);

    EXPECT_EQ(sentContent(laptop_sync_, office_guid).parent_guid(), archive_guid.toStdString());

    receiveRecords(phone_sync_, sendRecords(laptop_sync_));

    EXPECT_EQ(storedGroup(phone_, office_guid).parentId(), storedGroup(phone_, archive_guid).id());
    EXPECT_TRUE(unsyncedGroups(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Removing a group removes what lies in it, and so it does on the other device.
TEST_F(SyncLocalGroupsTest, RemovalReachesTheOtherDeviceWithWhatLiesBelow)
{
    const QString office_guid = addSharedGroup("office");
    const QString branch_guid = addSharedGroup("branch", office_guid);

    removeGroup(laptop_, office_guid);

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_sync_.readUnsyncedRecords(&request), SyncLocalGroups::SyncResult::DONE);
    ASSERT_EQ(request.record_size(), 2);
    EXPECT_TRUE(request.record(0).deleted());
    EXPECT_TRUE(request.record(0).data().empty());
    EXPECT_TRUE(request.record(1).deleted());

    receiveRecords(phone_sync_, sendRecords(laptop_sync_));

    EXPECT_FALSE(storedGroup(phone_, office_guid).isValid());
    EXPECT_FALSE(storedGroup(phone_, branch_guid).isValid());
    EXPECT_TRUE(unsyncedGroups(laptop_).isEmpty());
    EXPECT_TRUE(unsyncedGroups(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A group removed on one device is removed with what lies in it, a group another device has put
// there meanwhile included.
TEST_F(SyncLocalGroupsTest, GroupAddedIntoAGroupRemovedOnAnotherDeviceIsRemovedOnBoth)
{
    const QString office_guid = addSharedGroup("office");

    const QString branch_guid = addGroup(phone_, "branch", storedGroup(phone_, office_guid).id()).guid();
    removeGroup(laptop_, office_guid);

    receiveRecords(laptop_sync_, sendRecords(phone_sync_));
    EXPECT_FALSE(storedGroup(laptop_, branch_guid).isValid());

    receiveRecords(phone_sync_, sendRecords(laptop_sync_));

    EXPECT_FALSE(storedGroup(phone_, office_guid).isValid());
    EXPECT_FALSE(storedGroup(phone_, branch_guid).isValid());
    EXPECT_TRUE(unsyncedGroups(laptop_).isEmpty());
    EXPECT_TRUE(unsyncedGroups(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A group removed on one device and changed on another meanwhile stays removed, since what it held
// is removed already. Its removal goes to the router on top of the change.
TEST_F(SyncLocalGroupsTest, RemovalOfAGroupWinsOverItsChangeOnAnotherDevice)
{
    const QString office_guid = addSharedGroup("office");
    const QString branch_guid = addSharedGroup("branch", office_guid);

    removeGroup(phone_, office_guid);
    renameGroup(laptop_, office_guid, "renamed");

    const proto::router::SyncRecordList list = sendRecords(laptop_sync_);
    ASSERT_EQ(list.record_size(), 1);
    receiveRecords(phone_sync_, list);

    EXPECT_FALSE(storedGroup(phone_, office_guid).isValid());

    const QList<LocalGroupStore::Unsynced> unsynced = unsyncedGroups(phone_);
    ASSERT_EQ(unsynced.size(), 2);

    for (const LocalGroupStore::Unsynced& item : unsynced)
    {
        EXPECT_TRUE(item.deleted);

        if (item.group.guid() == office_guid)
            EXPECT_EQ(item.sync_revision, list.record(0).revision());
    }

    receiveRecords(laptop_sync_, sendRecords(phone_sync_));

    EXPECT_FALSE(storedGroup(laptop_, office_guid).isValid());
    EXPECT_FALSE(storedGroup(laptop_, branch_guid).isValid());
    EXPECT_TRUE(unsyncedGroups(laptop_).isEmpty());
    EXPECT_TRUE(unsyncedGroups(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Each device put one group into the other, which closes a loop once both moves meet. The device
// that receives the second move puts its group at the root, and both end with the same tree.
TEST_F(SyncLocalGroupsTest, LoopMadeOnTwoDevicesIsBrokenOnBoth)
{
    const QString first_guid = addSharedGroup("first");
    const QString second_guid = addSharedGroup("second");

    moveGroup(laptop_, first_guid, second_guid);
    moveGroup(phone_, second_guid, first_guid);

    receiveRecords(phone_sync_, sendRecords(laptop_sync_));
    receiveRecords(laptop_sync_, sendRecords(phone_sync_));

    for (Database* db : { &laptop_, &phone_ })
    {
        const LocalGroupConfig first = storedGroup(*db, first_guid);
        ASSERT_TRUE(first.isValid());
        EXPECT_EQ(first.parentId(), 0);
        EXPECT_EQ(storedGroup(*db, second_guid).parentId(), first.id());
        EXPECT_TRUE(unsyncedGroups(*db).isEmpty());
    }
}

//--------------------------------------------------------------------------------------------------
// Both devices renamed the group. The name the router holds replaces the one of the device that
// came second, and that one is not sent.
TEST_F(SyncLocalGroupsTest, LocalChangeGivesWayToTheRouter)
{
    const QString guid = addSharedGroup("office");

    renameGroup(phone_, guid, "phone");
    renameGroup(laptop_, guid, "laptop");
    receiveRecords(phone_sync_, sendRecords(laptop_sync_));

    EXPECT_EQ(storedGroup(phone_, guid).name(), QString("laptop"));
    EXPECT_TRUE(unsyncedGroups(phone_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A received group that does not open loses to the one of the device, and the reading goes on. A
// record that is refused leaves no row for the group it names as its parent.
TEST_F(SyncLocalGroupsTest, ReceivedGroupThatDoesNotOpenLoses)
{
    const QString guid = addSharedGroup("office");

    proto::router::SyncRecordList list;

    proto::router::SyncRecord* damaged = list.add_record();
    damaged->set_revision(5);
    damaged->set_type(proto::router::SYNC_DATA_TYPE_LOCAL_GROUPS);
    damaged->set_id(guid.toStdString());
    damaged->set_data("damaged");

    *list.add_record() = newerRecord(kGuid, 6, "", kParentGuid, std::string());

    receiveRecords(phone_sync_, list);

    EXPECT_EQ(storedGroup(phone_, guid).name(), QString("office"));
    EXPECT_EQ(SyncStore(phone_).receivedRevision(proto::router::SYNC_DATA_TYPE_LOCAL_GROUPS), 6);

    qint64 row_id = 0;
    EXPECT_EQ(LocalGroupStore(phone_).findRowIdByGuid(kGuid, &row_id), Database::FindResult::NOT_FOUND);
    EXPECT_EQ(LocalGroupStore(phone_).findRowIdByGuid(kParentGuid, &row_id), Database::FindResult::NOT_FOUND);
}

//--------------------------------------------------------------------------------------------------
// A newer version of the client has put into the group a field this one does not know.
TEST_F(SyncLocalGroupsTest, FieldUnknownToTheDeviceComesBackWithItsChange)
{
    const std::string kUnknownField = std::string("\x2a\x06") + "future";

    proto::router::SyncRecordList list;
    *list.add_record() = newerRecord(kGuid, 1, "office", "", kUnknownField);
    receiveRecords(laptop_sync_, list);
    ASSERT_EQ(storedGroup(laptop_, kGuid).name(), QString("office"));

    renameGroup(laptop_, kGuid, "renamed");

    const proto::router::SyncLocalGroup sent = sentContent(laptop_sync_, kGuid);
    EXPECT_EQ(sent.name(), "renamed");
    EXPECT_EQ(sent.unknown_fields(), kUnknownField);
}

//--------------------------------------------------------------------------------------------------
// A group whose data does not read would reach the other devices with a part of it lost, so it is
// not sent.
TEST_F(SyncLocalGroupsTest, DamagedGroupIsNotSent)
{
    const QString guid = addGroup(laptop_, "office").guid();
    rawExec("laptop.db3", "UPDATE local_groups SET data=X'00' WHERE guid=?", guid);

    proto::router::SyncRecordRequest request;
    ASSERT_EQ(laptop_sync_.readUnsyncedRecords(&request), SyncLocalGroups::SyncResult::DONE);
    EXPECT_EQ(request.record_size(), 0);
}
