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

#include "client/storage/local_group_store.h"

#include <QDateTime>

#include "client/storage/database_test_base.h"

namespace {

const char kOfficeGuid[] = "11111111-1111-1111-1111-111111111111";
const char kBranchGuid[] = "22222222-2222-2222-2222-222222222222";
const char kRoomGuid[] = "33333333-3333-3333-3333-333333333333";
const char kHostGuid[] = "44444444-4444-4444-4444-444444444444";

//--------------------------------------------------------------------------------------------------
LocalGroupConfig receivedGroup(const QString& guid, const QString& name, qint64 parent_id)
{
    LocalGroupConfig group;
    group.setGuid(guid);
    group.setName(name);
    group.setParentId(parent_id);
    return group;
}

//--------------------------------------------------------------------------------------------------
LocalHostConfig receivedHost(const QString& guid, const QString& name, qint64 group_id)
{
    LocalHostConfig host;
    host.setGuid(guid);
    host.setName(name);
    host.setAddress("192.168.0.1");
    host.setGroupId(group_id);
    return host;
}

} // namespace

using LocalGroupStoreTest = DatabaseTestBase;

//--------------------------------------------------------------------------------------------------
// A group holds groups of its own, and deleting it deletes them too. A child left behind keeps
// pointing at a parent that is gone: the tree is walked from the root down, so such a group is in
// the book but in no place the user can ever reach.
TEST_F(LocalGroupStoreTest, RemovedGroupTakesItsChildGroupsWithIt)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 child = addGroup("child", parent);
    addGroup("grandchild", child);
    addGroup("other", 0);

    ASSERT_TRUE(local_groups_.remove(parent));

    EXPECT_EQ(groupNames(), QStringList({ "other" }));
}

//--------------------------------------------------------------------------------------------------
// The hosts of the subtree are deleted with it: the user deleted the group with what it holds.
// Moved to the root, they would come back in a place the user never put them.
TEST_F(LocalGroupStoreTest, HostsOfARemovedGroupAreRemovedWithIt)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 child = addGroup("child", parent);

    addHost("host-of-parent", parent);
    addHost("host-of-child", child);
    addHost("host-of-root", 0);

    ASSERT_TRUE(local_groups_.remove(parent));

    EXPECT_EQ(hostNamesOfGroup(0), QStringList({ "host-of-root" }));
    EXPECT_EQ(rowCount("local_hosts"), 1);
}

//--------------------------------------------------------------------------------------------------
// The root is not a record of the table but the parent every group of the top level names. Asked to
// delete it, the book would answer by emptying itself.
TEST_F(LocalGroupStoreTest, RootIsNotAGroupAndIsNotRemoved)
{
    const qint64 group = addGroup("group", 0);
    addHost("host", group);

    EXPECT_FALSE(local_groups_.remove(0));

    EXPECT_EQ(groupNames(), QStringList({ "group" }));
    EXPECT_EQ(hostNamesOfGroup(group), QStringList({ "host" }));
}

//--------------------------------------------------------------------------------------------------
// A group put inside its own subtree closes a loop. The tree is walked from the root down, so the
// group and everything under it drop out of it for good; and a path built by walking parents upward
// from a host inside the loop never ends.
TEST_F(LocalGroupStoreTest, GroupIsNotMovedIntoItsOwnSubtree)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 child = addGroup("child", parent);
    const qint64 grandchild = addGroup("grandchild", child);

    EXPECT_FALSE(local_groups_.move(parent, grandchild));
    EXPECT_FALSE(local_groups_.move(parent, parent));

    EXPECT_EQ(findLocalGroup(db_, parent)->parentId(), 0);
    EXPECT_EQ(localGroupList(0).size(), 1);
}

//--------------------------------------------------------------------------------------------------
// Editing a group is the other way its parent is written, and it is no different.
TEST_F(LocalGroupStoreTest, EditedGroupIsNotMadeAChildOfItsOwnChild)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 child = addGroup("child", parent);

    LocalGroupConfig group = *findLocalGroup(db_, parent);
    group.setParentId(child);

    EXPECT_FALSE(local_groups_.modify(group));

    EXPECT_EQ(findLocalGroup(db_, parent)->parentId(), 0);
}

//--------------------------------------------------------------------------------------------------
// A move to a group that is not below it is what the check is there to allow.
TEST_F(LocalGroupStoreTest, GroupIsMovedUnderAGroupOutsideItsSubtree)
{
    const qint64 first = addGroup("first", 0);
    addGroup("child", first);
    const qint64 second = addGroup("second", 0);

    EXPECT_TRUE(local_groups_.move(first, second));
    EXPECT_EQ(findLocalGroup(db_, first)->parentId(), second);

    // And back to the root, which is not a group and cannot be below anything.
    EXPECT_TRUE(local_groups_.move(first, 0));
    EXPECT_EQ(findLocalGroup(db_, first)->parentId(), 0);
}

//--------------------------------------------------------------------------------------------------
// Groups come by name whatever order they were added in, and the case of a letter does not move a
// group, Cyrillic included.
TEST_F(LocalGroupStoreTest, GroupsAreListedByName)
{
    addGroup(QString::fromUtf8("Сервера"), 0);
    addGroup("beta", 0);
    addGroup(QString::fromUtf8("бухгалтерия"), 0);
    addGroup("Alpha", 0);

    const QList<LocalGroupConfig> groups = localGroupList(0);
    ASSERT_EQ(groups.size(), 4);
    EXPECT_EQ(groups[0].name(), "Alpha");
    EXPECT_EQ(groups[1].name(), "beta");
    EXPECT_EQ(groups[2].name(), QString::fromUtf8("бухгалтерия"));
    EXPECT_EQ(groups[3].name(), QString::fromUtf8("Сервера"));
}

//--------------------------------------------------------------------------------------------------
// The same for a group, and a move counts as an edit the way it does for a host.
TEST_F(LocalGroupStoreTest, EditMarksTheMomentAGroupChanged)
{
    const qint64 start_time = QDateTime::currentSecsSinceEpoch();

    const qint64 parent = addGroup("parent", 0);
    const qint64 group_id = addGroup("group", 0);
    EXPECT_GE(modifyTime("local_groups", "id", group_id), start_time);

    ASSERT_TRUE(setModifyTime("local_groups", "id", group_id, 1));

    LocalGroupConfig edited = *findLocalGroup(db_, group_id);
    edited.setName("renamed");
    ASSERT_TRUE(local_groups_.modify(edited));

    EXPECT_GE(modifyTime("local_groups", "id", group_id), start_time);

    ASSERT_TRUE(setModifyTime("local_groups", "id", group_id, 1));
    ASSERT_TRUE(local_groups_.move(group_id, parent));

    EXPECT_GE(modifyTime("local_groups", "id", group_id), start_time);
}

//--------------------------------------------------------------------------------------------------
// Only the subtree of the group goes. A neighbour that hangs off the same parent stays as it is,
// with the hosts it holds.
TEST_F(LocalGroupStoreTest, RemovedGroupLeavesItsNeighbourAlone)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 first = addGroup("first", parent);
    const qint64 second = addGroup("second", parent);

    addHost("host-of-first", first);
    addHost("host-of-second", second);

    ASSERT_TRUE(local_groups_.remove(first));

    EXPECT_EQ(groupNames(), QStringList({ "parent", "second" }));
    EXPECT_EQ(hostNamesOfGroup(second), QStringList({ "host-of-second" }));
    EXPECT_EQ(rowCount("local_hosts"), 1);
}

//--------------------------------------------------------------------------------------------------
// A group is named by its id inside this database only. The guid names the same group in any
// database that ever hears about it, so every group gets one the moment it is created.
TEST_F(LocalGroupStoreTest, GroupGetsAGuidOfItsOwn)
{
    const qint64 first = addGroup("first", 0);
    const qint64 second = addGroup("second", 0);

    const std::optional<LocalGroupConfig> stored = findLocalGroup(db_, first);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->guid().isEmpty());

    const std::optional<LocalGroupConfig> other = findLocalGroup(db_, second);
    ASSERT_TRUE(other.has_value());
    EXPECT_NE(stored->guid(), other->guid());
}

//--------------------------------------------------------------------------------------------------
// A guid an import brings from elsewhere is kept as it is. The group arriving here is the group
// the file names, not a new one.
TEST_F(LocalGroupStoreTest, GroupKeepsTheGuidItWasGiven)
{
    LocalGroupConfig group;
    group.setName("from-a-file");
    group.setGuid("7b0f1d18-0e3a-4a0e-9a4e-2a1c6a0f0c11");

    ASSERT_TRUE(local_groups_.add(group));

    const std::optional<LocalGroupConfig> stored = findLocalGroup(db_, group.id());
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->guid(), "7b0f1d18-0e3a-4a0e-9a4e-2a1c6a0f0c11");
}

//--------------------------------------------------------------------------------------------------
// Renaming a group or moving it elsewhere in the tree leaves it the same group.
TEST_F(LocalGroupStoreTest, GroupGuidSurvivesEveryEdit)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 group = addGroup("group", 0);

    const std::optional<LocalGroupConfig> before = findLocalGroup(db_, group);
    ASSERT_TRUE(before.has_value());

    LocalGroupConfig edited = *before;
    edited.setName("renamed");
    edited.setComment("comment");
    ASSERT_TRUE(local_groups_.modify(edited));
    ASSERT_TRUE(local_groups_.move(group, parent));

    const std::optional<LocalGroupConfig> after = findLocalGroup(db_, group);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->name(), "renamed");
    EXPECT_EQ(after->parentId(), parent);
    EXPECT_EQ(after->guid(), before->guid());
}

//--------------------------------------------------------------------------------------------------
// A guid names one group, and the record naming it is the one an import looks for before it decides
// whether the group of the file is already here.
TEST_F(LocalGroupStoreTest, TwoGroupsDoNotShareOneGuid)
{
    const qint64 group = addGroup("group", 0);

    const std::optional<LocalGroupConfig> stored = findLocalGroup(db_, group);
    ASSERT_TRUE(stored.has_value());
    ASSERT_FALSE(stored->guid().isEmpty());

    LocalGroupConfig copy;
    copy.setName("copy");
    copy.setParentId(0);
    copy.setGuid(stored->guid());

    EXPECT_FALSE(local_groups_.add(copy));
    EXPECT_EQ(groupNames(), QStringList({ "group" }));
}

//--------------------------------------------------------------------------------------------------
// A group the user has just added is one the router does not hold yet.
TEST_F(LocalGroupStoreTest, NewGroupIsToBeSent)
{
    const qint64 group = addGroup("group", 0);

    EXPECT_EQ(syncDirty("local_groups", group), 1);
}

//--------------------------------------------------------------------------------------------------
// Every change the user makes to a group is counted, the way it is for a host.
TEST_F(LocalGroupStoreTest, ChangeOfAGroupIsCounted)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 group_id = addGroup("group", 0);

    ASSERT_TRUE(markSynced("local_groups", group_id, 7));

    std::optional<LocalGroupConfig> group = findLocalGroup(db_, group_id);
    ASSERT_TRUE(group.has_value());

    group->setName("renamed");
    ASSERT_TRUE(local_groups_.modify(*group));
    EXPECT_EQ(syncDirty("local_groups", group_id), 1);

    ASSERT_TRUE(local_groups_.move(group_id, parent));
    EXPECT_EQ(syncDirty("local_groups", group_id), 2);
}

//--------------------------------------------------------------------------------------------------
// The router has never held the group, so there is no removal to tell it of.
TEST_F(LocalGroupStoreTest, RemovedGroupThatWasNeverSentLeavesNoRow)
{
    const qint64 group = addGroup("group", 0);

    ASSERT_TRUE(local_groups_.remove(group));

    EXPECT_EQ(rowCount("local_groups"), 0);
}

//--------------------------------------------------------------------------------------------------
// The router holds the group, so its row stays as a mark of the removal until the router is told.
// The mark keeps the guid and nothing of what the group held.
TEST_F(LocalGroupStoreTest, RemovedGroupThatWasSentLeavesAMark)
{
    const qint64 parent = addGroup("parent", 0);

    LocalGroupConfig group;
    group.setName("group");
    group.setComment("comment");
    group.setParentId(parent);
    ASSERT_TRUE(local_groups_.add(group));

    ASSERT_TRUE(markSynced("local_groups", group.id(), 7));

    ASSERT_TRUE(local_groups_.remove(group.id()));

    EXPECT_TRUE(isGroupMarkedDeleted(group.id()));
    EXPECT_EQ(syncDirty("local_groups", group.id()), 1);

    SqlDatabase raw;
    ASSERT_TRUE(raw.open(file_path_));

    SqlQuery query(raw, "SELECT guid FROM local_groups WHERE id=?");
    query.addInt64(group.id());
    ASSERT_EQ(query.next(), SqlQuery::StepResult::ROW);
    EXPECT_EQ(query.columnText(0), group.guid());

    EXPECT_EQ(groupNames(), QStringList({ "parent" }));
}

//--------------------------------------------------------------------------------------------------
// Removing a group removes what is below it. The groups and the hosts the router holds leave
// marks, since the router is to be told of each of them, and the ones it never held are gone.
TEST_F(LocalGroupStoreTest, RemovedGroupLeavesMarksOfWhatTheRouterHoldsBelowIt)
{
    const qint64 group = addGroup("group", 0);
    const qint64 sent_group = addGroup("sent", group);
    addGroup("new", group);

    const qint64 direct_host = addHost("direct", group);
    const qint64 sent_host = addHost("sent", sent_group);
    addHost("new", sent_group);

    ASSERT_TRUE(markSynced("local_groups", group, 7));
    ASSERT_TRUE(markSynced("local_groups", sent_group, 8));
    ASSERT_TRUE(markSynced("local_hosts", direct_host, 9));
    ASSERT_TRUE(markSynced("local_hosts", sent_host, 10));

    ASSERT_TRUE(local_groups_.remove(group));

    EXPECT_EQ(rowCount("local_groups"), 2);
    EXPECT_TRUE(isGroupMarkedDeleted(group));
    EXPECT_TRUE(isGroupMarkedDeleted(sent_group));
    EXPECT_EQ(syncDirty("local_groups", group), 1);
    EXPECT_EQ(syncDirty("local_groups", sent_group), 1);

    EXPECT_EQ(rowCount("local_hosts"), 2);
    EXPECT_TRUE(isMarkedDeleted("local_hosts", direct_host));
    EXPECT_TRUE(isMarkedDeleted("local_hosts", sent_host));
    EXPECT_EQ(syncDirty("local_hosts", direct_host), 1);
    EXPECT_EQ(syncDirty("local_hosts", sent_host), 1);

    EXPECT_TRUE(allLocalGroups().isEmpty());
    EXPECT_TRUE(allLocalHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A group the router holds may lie in a group it has never held. Removing the outer one erases its
// row, and the row of the inner one must not go with it, or the router would never be told that the
// inner group and its host are removed.
TEST_F(LocalGroupStoreTest, SentGroupBelowARemovedGroupThatWasNeverSentLeavesAMark)
{
    const qint64 archive = addGroup("archive", 0);
    const qint64 office = addGroup("office", archive);
    const qint64 host = addHost("host", office);

    ASSERT_TRUE(markSynced("local_groups", office, 7));
    ASSERT_TRUE(markSynced("local_hosts", host, 8));

    ASSERT_TRUE(local_groups_.remove(archive));

    EXPECT_EQ(rowCount("local_groups"), 1);
    EXPECT_TRUE(isGroupMarkedDeleted(office));
    EXPECT_EQ(syncDirty("local_groups", office), 1);

    EXPECT_EQ(rowCount("local_hosts"), 1);
    EXPECT_TRUE(isMarkedDeleted("local_hosts", host));
    EXPECT_EQ(syncDirty("local_hosts", host), 1);
}

//--------------------------------------------------------------------------------------------------
// The mark of a removed group is not a group: it is not listed, not found, and nothing changes it.
TEST_F(LocalGroupStoreTest, MarkOfARemovedGroupIsNotShownAndNotChanged)
{
    const qint64 other = addGroup("other", 0);
    const qint64 group_id = addGroup("group", 0);

    const std::optional<LocalGroupConfig> group = findLocalGroup(db_, group_id);
    ASSERT_TRUE(group.has_value());

    ASSERT_TRUE(markSynced("local_groups", group_id, 7));
    ASSERT_TRUE(local_groups_.remove(group_id));

    const QList<LocalGroupConfig> root_groups = localGroupList(0);
    ASSERT_EQ(root_groups.size(), 1);
    EXPECT_EQ(root_groups.front().id(), other);

    EXPECT_EQ(groupNames(), QStringList({ "other" }));
    EXPECT_FALSE(findLocalGroup(db_, group_id).has_value());

    LocalGroupConfig changed = *group;
    changed.setName("renamed");
    EXPECT_FALSE(local_groups_.modify(changed));

    EXPECT_FALSE(local_groups_.move(group_id, other));

    local_groups_.remove(group_id);

    EXPECT_TRUE(isGroupMarkedDeleted(group_id));
    EXPECT_EQ(syncDirty("local_groups", group_id), 1);
}

//--------------------------------------------------------------------------------------------------
// The fields a newer version put into a group stay with it through an edit and are sent back.
TEST_F(LocalGroupStoreTest, UnknownFieldsOfAGroupSurviveItsEdit)
{
    attachSyncStorage();

    LocalGroupConfig received = receivedGroup(kOfficeGuid, "office", 0);
    received.setUnknownData("unknown");
    ASSERT_TRUE(storeReceivedGroup(received, 7));

    qint64 group_id = 0;
    ASSERT_EQ(local_groups_.findRowIdByGuid(kOfficeGuid, &group_id), Database::FindResult::FOUND);

    LocalGroupConfig edited = *findLocalGroup(db_, group_id);
    edited.setName("renamed");
    ASSERT_TRUE(local_groups_.modify(edited));

    const QList<LocalGroupStore::Unsynced> unsynced = unsyncedGroups();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].group.name(), "renamed");
    EXPECT_EQ(unsynced[0].group.unknownData(), QByteArray("unknown"));
    EXPECT_EQ(unsynced[0].sync_revision, 7);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);
}

//--------------------------------------------------------------------------------------------------
// A group whose data does not read is listed among the unsynced ones as damaged. The lists show it
// as before, since they show nothing of the data.
TEST_F(LocalGroupStoreTest, GroupWhoseDataDoesNotParseIsDamaged)
{
    const qint64 broken_id = addGroup("office", 0);
    addGroup("home", 0);
    ASSERT_TRUE(corruptRecordData("local_groups", broken_id));

    QList<LocalGroupStore::Unsynced> unsynced;
    ASSERT_EQ(local_groups_.unsynced(&unsynced), Database::ReadResult::INCOMPLETE);
    ASSERT_EQ(unsynced.size(), 2);

    for (const LocalGroupStore::Unsynced& item : std::as_const(unsynced))
        EXPECT_EQ(item.damaged, item.group.id() == broken_id);

    EXPECT_EQ(groupNames(), QStringList({ "home", "office" }));
}

//--------------------------------------------------------------------------------------------------
// The mark of a removed group keeps nothing of the group, the fields of a newer version included.
TEST_F(LocalGroupStoreTest, MarkOfARemovedGroupKeepsNoData)
{
    attachSyncStorage();

    LocalGroupConfig received = receivedGroup(kOfficeGuid, "office", 0);
    received.setUnknownData("unknown");
    ASSERT_TRUE(storeReceivedGroup(received, 7));

    qint64 group_id = 0;
    ASSERT_EQ(local_groups_.findRowIdByGuid(kOfficeGuid, &group_id), Database::FindResult::FOUND);
    ASSERT_TRUE(local_groups_.remove(group_id));

    EXPECT_TRUE(isGroupMarkedDeleted(group_id));

    const QList<LocalGroupStore::Unsynced> unsynced = unsyncedGroups();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_TRUE(unsynced[0].deleted);
    EXPECT_EQ(unsynced[0].group.guid(), kOfficeGuid);
    EXPECT_EQ(unsynced[0].sync_revision, 7);
}

//--------------------------------------------------------------------------------------------------
// A received group is what the router holds, so it keeps the time of the router and is not sent.
TEST_F(LocalGroupStoreTest, ReceivedGroupIsStoredAsTheRouterHoldsIt)
{
    attachSyncStorage();

    const qint64 parent = addGroup("parent", 0);
    ASSERT_TRUE(markSynced("local_groups", parent, 3));

    LocalGroupConfig received = receivedGroup(kOfficeGuid, "office", parent);
    received.setComment("comment");
    received.setModifyTime(1000);
    ASSERT_TRUE(storeReceivedGroup(received, 7));

    const QList<LocalGroupConfig> groups = localGroupList(parent);
    ASSERT_EQ(groups.size(), 1);
    EXPECT_EQ(groups[0].name(), "office");
    EXPECT_EQ(groups[0].comment(), "comment");
    EXPECT_EQ(groups[0].guid(), kOfficeGuid);

    EXPECT_EQ(modifyTime("local_groups", "id", groups[0].id()), 1000);
    EXPECT_TRUE(unsyncedGroups().isEmpty());

    ASSERT_TRUE(local_groups_.move(groups[0].id(), 0));

    const QList<LocalGroupStore::Unsynced> unsynced = unsyncedGroups();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, 7);
}

//--------------------------------------------------------------------------------------------------
// A newer revision replaces what the row of its guid holds. The revision the row already has
// changes nothing, so an edit made since stays.
TEST_F(LocalGroupStoreTest, ReceivedGroupTakesTheRowOfItsGuid)
{
    attachSyncStorage();

    const qint64 group_id = addGroup("group", 0);
    const QString guid = findLocalGroup(db_, group_id)->guid();
    ASSERT_TRUE(markSynced("local_groups", group_id, 7));

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(guid, "renamed", 0), 8));

    EXPECT_EQ(rowCount("local_groups"), 1);
    EXPECT_EQ(findLocalGroup(db_, group_id)->name(), "renamed");
    EXPECT_EQ(syncDirty("local_groups", group_id), 0);

    LocalGroupConfig edited = *findLocalGroup(db_, group_id);
    edited.setName("edited");
    ASSERT_TRUE(local_groups_.modify(edited));

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(guid, "renamed", 0), 8));

    EXPECT_EQ(findLocalGroup(db_, group_id)->name(), "edited");
    EXPECT_EQ(syncDirty("local_groups", group_id), 1);
}

//--------------------------------------------------------------------------------------------------
// An empty row holds the guid of a group the device does not have yet, for what lies in that
// group. The group that comes with this guid takes the row and keeps its id.
TEST_F(LocalGroupStoreTest, EmptyRowIsTakenByTheGroupThatComesWithItsGuid)
{
    attachSyncStorage();

    qint64 row_id = 0;
    ASSERT_TRUE(local_groups_.addEmptyRow(kOfficeGuid, &row_id));
    EXPECT_GT(row_id, 0);

    QString guid;
    ASSERT_EQ(local_groups_.findGuidByRowId(row_id, &guid), Database::FindResult::FOUND);
    EXPECT_EQ(guid, kOfficeGuid);

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(kOfficeGuid, "office", 0), 12));

    const std::optional<LocalGroupConfig> stored = findLocalGroup(db_, row_id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->name(), "office");
    EXPECT_EQ(rowCount("local_groups"), 1);
}

//--------------------------------------------------------------------------------------------------
// An empty row is not a group. It is neither listed nor found by its id nor sent.
TEST_F(LocalGroupStoreTest, EmptyRowOfAGroupIsNotShown)
{
    qint64 row_id = 0;
    ASSERT_TRUE(local_groups_.addEmptyRow(kOfficeGuid, &row_id));
    EXPECT_TRUE(isEmptyRow("local_groups", row_id));

    EXPECT_TRUE(localGroupList(0).isEmpty());
    EXPECT_TRUE(allLocalGroups().isEmpty());
    EXPECT_FALSE(findLocalGroup(db_, row_id).has_value());
    EXPECT_TRUE(unsyncedGroups().isEmpty());

    qint64 found_id = 0;
    ASSERT_EQ(local_groups_.findRowIdByGuid(kOfficeGuid, &found_id), Database::FindResult::FOUND);
    EXPECT_EQ(found_id, row_id);

    EXPECT_EQ(local_groups_.findRowIdByGuid(kBranchGuid, &found_id), Database::FindResult::NOT_FOUND);
}

//--------------------------------------------------------------------------------------------------
// A group may come before the group it lies in. It is stored under the empty row of that group,
// is in no list and is not found until that group comes, and is in its place after that.
TEST_F(LocalGroupStoreTest, GroupReceivedIntoAnEmptyRowWaitsThere)
{
    attachSyncStorage();

    qint64 row_id = 0;
    ASSERT_TRUE(local_groups_.addEmptyRow(kOfficeGuid, &row_id));

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(kBranchGuid, "branch", row_id), 7));

    qint64 branch = 0;
    ASSERT_EQ(local_groups_.findRowIdByGuid(kBranchGuid, &branch), Database::FindResult::FOUND);

    EXPECT_TRUE(localGroupList(0).isEmpty());
    EXPECT_TRUE(localGroupList(row_id).isEmpty());
    EXPECT_TRUE(allLocalGroups().isEmpty());
    EXPECT_FALSE(findLocalGroup(db_, branch).has_value());
    EXPECT_TRUE(unsyncedGroups().isEmpty());
    EXPECT_EQ(rowCount("local_groups"), 2);

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(kOfficeGuid, "office", 0), 8));

    const QList<LocalGroupConfig> root_groups = localGroupList(0);
    ASSERT_EQ(root_groups.size(), 1);
    EXPECT_EQ(root_groups[0].name(), "office");

    const QList<LocalGroupConfig> child_groups = localGroupList(row_id);
    ASSERT_EQ(child_groups.size(), 1);
    EXPECT_EQ(child_groups[0].name(), "branch");
    EXPECT_EQ(groupNames(), QStringList({ "branch", "office" }));
}

//--------------------------------------------------------------------------------------------------
// A group that waits for its parent keeps what lies below it out of the lists too. All of it is
// shown once the parent comes.
TEST_F(LocalGroupStoreTest, WhatLiesBelowAWaitingGroupWaitsWithIt)
{
    attachSyncStorage();

    qint64 office = 0;
    ASSERT_TRUE(local_groups_.addEmptyRow(kOfficeGuid, &office));

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(kBranchGuid, "branch", office), 7));

    qint64 branch = 0;
    ASSERT_EQ(local_groups_.findRowIdByGuid(kBranchGuid, &branch), Database::FindResult::FOUND);

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(kRoomGuid, "room", branch), 8));

    qint64 room = 0;
    ASSERT_EQ(local_groups_.findRowIdByGuid(kRoomGuid, &room), Database::FindResult::FOUND);

    ASSERT_TRUE(storeReceivedLocalHost(receivedHost(kHostGuid, "host", room), 9));

    EXPECT_TRUE(allLocalGroups().isEmpty());
    EXPECT_TRUE(allLocalHosts().isEmpty());
    EXPECT_TRUE(localHostList(room).isEmpty());

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(kOfficeGuid, "office", 0), 10));

    EXPECT_EQ(groupNames(), QStringList({ "branch", "office", "room" }));
    EXPECT_EQ(hostNamesOfGroup(room), QStringList({ "host" }));
    EXPECT_EQ(allLocalHosts().size(), 1);
}

//--------------------------------------------------------------------------------------------------
// The router may bring a group moved into a group the device does not have yet. The group leaves
// the lists with what lies below it, and all of it is back under the new parent once that one comes.
TEST_F(LocalGroupStoreTest, GroupMovedIntoAGroupThatHasNotComeWaitsWithWhatLiesBelowIt)
{
    attachSyncStorage();

    const qint64 branch = addGroup("branch", 0);
    const qint64 room = addGroup("room", branch);
    const qint64 host = addHost("host", room);
    const QString branch_guid = findLocalGroup(db_, branch)->guid();

    ASSERT_TRUE(markSynced("local_groups", branch, 7));

    qint64 office = 0;
    ASSERT_TRUE(local_groups_.addEmptyRow(kOfficeGuid, &office));

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(branch_guid, "branch", office), 8));

    EXPECT_TRUE(allLocalGroups().isEmpty());
    EXPECT_TRUE(allLocalHosts().isEmpty());
    EXPECT_FALSE(findLocalHost(db_, host).has_value());

    QList<LocalHostConfig> found;
    EXPECT_EQ(local_hosts_.search("host", &found), Database::ReadResult::OK);
    EXPECT_TRUE(found.isEmpty());

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(kOfficeGuid, "office", 0), 9));

    EXPECT_EQ(groupNames(), QStringList({ "branch", "office", "room" }));
    EXPECT_EQ(findLocalGroup(db_, branch)->parentId(), office);
    EXPECT_EQ(findLocalGroup(db_, room)->parentId(), branch);
    EXPECT_EQ(hostNamesOfGroup(room), QStringList({ "host" }));

    EXPECT_EQ(local_hosts_.search("host", &found), Database::ReadResult::OK);
    EXPECT_EQ(found.size(), 1);
}

//--------------------------------------------------------------------------------------------------
// A received group that names a group below it as its parent would close a loop. It is put at the
// root and waits to be sent.
TEST_F(LocalGroupStoreTest, ReceivedGroupThatWouldCloseALoopGoesToTheRoot)
{
    attachSyncStorage();

    const qint64 first = addGroup("first", 0);
    const qint64 second = addGroup("second", first);
    const QString guid = findLocalGroup(db_, first)->guid();

    ASSERT_TRUE(markSynced("local_groups", first, 7));
    ASSERT_TRUE(markSynced("local_groups", second, 8));

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(guid, "first", second), 9));

    EXPECT_EQ(findLocalGroup(db_, first)->parentId(), 0);
    EXPECT_EQ(findLocalGroup(db_, second)->parentId(), first);

    const QList<LocalGroupStore::Unsynced> unsynced = unsyncedGroups();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].group.id(), first);
    EXPECT_EQ(unsynced[0].sync_revision, 9);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);
}

//--------------------------------------------------------------------------------------------------
// The same for a group whose record names the group itself as its parent.
TEST_F(LocalGroupStoreTest, ReceivedGroupThatNamesItselfGoesToTheRoot)
{
    attachSyncStorage();

    qint64 row_id = 0;
    ASSERT_TRUE(local_groups_.addEmptyRow(kOfficeGuid, &row_id));

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(kOfficeGuid, "office", row_id), 7));

    const std::optional<LocalGroupConfig> stored = findLocalGroup(db_, row_id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->parentId(), 0);
    EXPECT_EQ(syncDirty("local_groups", row_id), 1);
}

//--------------------------------------------------------------------------------------------------
// A group removed on the device is removed with what lies in it, a group another device has put
// there since included. The router holds that group, so it leaves a mark.
TEST_F(LocalGroupStoreTest, GroupReceivedIntoARemovedGroupIsRemovedToo)
{
    attachSyncStorage();

    const qint64 parent = addGroup("parent", 0);
    ASSERT_TRUE(markSynced("local_groups", parent, 7));
    ASSERT_TRUE(local_groups_.remove(parent));

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(kOfficeGuid, "office", parent), 8));

    EXPECT_TRUE(allLocalGroups().isEmpty());

    qint64 group_id = 0;
    ASSERT_EQ(local_groups_.findRowIdByGuid(kOfficeGuid, &group_id), Database::FindResult::FOUND);
    EXPECT_TRUE(isGroupMarkedDeleted(group_id));

    const QList<LocalGroupStore::Unsynced> unsynced = unsyncedGroups();
    ASSERT_EQ(unsynced.size(), 2);

    for (const LocalGroupStore::Unsynced& item : unsynced)
    {
        EXPECT_TRUE(item.deleted);
        EXPECT_EQ(item.sync_revision, item.group.id() == group_id ? 8 : 7);
        EXPECT_EQ(item.sync_dirty, 1);
    }
}

//--------------------------------------------------------------------------------------------------
// A removal that comes from the router removes the group with what lies below it. The router
// knows of the group itself and is yet to be told of the rest it holds.
TEST_F(LocalGroupStoreTest, ReceivedRemovalRemovesTheGroupWithWhatLiesBelow)
{
    attachSyncStorage();

    const qint64 group = addGroup("group", 0);
    const qint64 sent_group = addGroup("sent", group);
    addGroup("new", group);
    addGroup("other", 0);

    const qint64 sent_host = addHost("sent", sent_group);
    addHost("new", group);

    const QString guid = findLocalGroup(db_, group)->guid();

    ASSERT_TRUE(markSynced("local_groups", group, 7));
    ASSERT_TRUE(markSynced("local_groups", sent_group, 8));
    ASSERT_TRUE(markSynced("local_hosts", sent_host, 9));

    ASSERT_TRUE(eraseReceivedGroup(guid));

    EXPECT_EQ(groupNames(), QStringList({ "other" }));
    EXPECT_TRUE(allLocalHosts().isEmpty());

    EXPECT_EQ(rowCount("local_groups"), 2);
    EXPECT_TRUE(isGroupMarkedDeleted(sent_group));
    EXPECT_EQ(syncDirty("local_groups", sent_group), 1);

    EXPECT_EQ(rowCount("local_hosts"), 1);
    EXPECT_TRUE(isMarkedDeleted("local_hosts", sent_host));
    EXPECT_EQ(syncDirty("local_hosts", sent_host), 1);
}

//--------------------------------------------------------------------------------------------------
// The group something waits for may turn out to be removed. What waits under its empty row is
// removed with it.
TEST_F(LocalGroupStoreTest, ReceivedRemovalOfAnEmptyRowRemovesWhatWaitsUnderIt)
{
    attachSyncStorage();

    qint64 row_id = 0;
    ASSERT_TRUE(local_groups_.addEmptyRow(kOfficeGuid, &row_id));

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(kBranchGuid, "branch", row_id), 7));

    ASSERT_TRUE(eraseReceivedGroup(kOfficeGuid));

    EXPECT_TRUE(allLocalGroups().isEmpty());
    EXPECT_EQ(rowCount("local_groups"), 1);

    qint64 branch_id = 0;
    ASSERT_EQ(local_groups_.findRowIdByGuid(kBranchGuid, &branch_id), Database::FindResult::FOUND);
    EXPECT_TRUE(isGroupMarkedDeleted(branch_id));
    EXPECT_EQ(syncDirty("local_groups", branch_id), 1);
}

//--------------------------------------------------------------------------------------------------
// A removal of a group the device never had leaves the groups as they are.
TEST_F(LocalGroupStoreTest, ReceivedRemovalOfAnUnknownGroupDoesNothing)
{
    attachSyncStorage();

    addGroup("group", 0);

    ASSERT_TRUE(eraseReceivedGroup(kOfficeGuid));

    EXPECT_EQ(groupNames(), QStringList({ "group" }));
    EXPECT_EQ(rowCount("local_groups"), 1);
}

//--------------------------------------------------------------------------------------------------
// A device that finds another storage made by a reset in place of the one it was attached to gives
// up its groups and its hosts and takes the ones the storage holds.
TEST_F(LocalGroupStoreTest, GroupsAndHostsAreDiscardedForAnotherStorage)
{
    attachSyncStorage();

    const qint64 group = addGroup("group", 0);
    addGroup("child", group);
    addHost("host", group);
    addHost("root-host", 0);
    ASSERT_TRUE(markSynced("local_groups", group, 7));

    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), true),
              Database::SyncResult::DONE);

    EXPECT_EQ(rowCount("local_groups"), 0);
    EXPECT_EQ(rowCount("local_hosts"), 0);
}

//--------------------------------------------------------------------------------------------------
// Another storage that is not made by a reset takes every group and host of the device as new. A
// removal that was not sent has no one to be sent to, and what waits for a group that has not come
// goes with the empty row of that group.
TEST_F(LocalGroupStoreTest, GroupsAndHostsAreNewToAnotherStorageNotMadeByAReset)
{
    attachSyncStorage();

    const qint64 kept_group = addGroup("kept", 0);
    const qint64 removed_group = addGroup("removed", 0);
    const qint64 kept_host = addHost("kept", kept_group);
    const qint64 removed_host = addHost("removed", 0);
    addHost("new", 0);

    ASSERT_TRUE(markSynced("local_groups", kept_group, 7));
    ASSERT_TRUE(markSynced("local_groups", removed_group, 8));
    ASSERT_TRUE(markSynced("local_hosts", kept_host, 9));
    ASSERT_TRUE(markSynced("local_hosts", removed_host, 10));
    ASSERT_TRUE(local_groups_.remove(removed_group));
    ASSERT_TRUE(local_hosts_.remove(removed_host));

    qint64 office = 0;
    ASSERT_TRUE(local_groups_.addEmptyRow(kOfficeGuid, &office));
    ASSERT_TRUE(storeReceivedGroup(receivedGroup(kBranchGuid, "branch", office), 11));

    qint64 branch = 0;
    ASSERT_EQ(local_groups_.findRowIdByGuid(kBranchGuid, &branch), Database::FindResult::FOUND);
    ASSERT_TRUE(storeReceivedLocalHost(receivedHost(kHostGuid, "waiting", branch), 12));

    ASSERT_EQ(rowCount("local_groups"), 4);
    ASSERT_EQ(rowCount("local_hosts"), 4);

    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DONE);

    EXPECT_EQ(groupNames(), QStringList({ "kept" }));
    EXPECT_EQ(rowCount("local_groups"), 1);
    EXPECT_EQ(hostNamesOfGroup(kept_group), QStringList({ "kept" }));
    EXPECT_EQ(hostNamesOfGroup(0), QStringList({ "new" }));
    EXPECT_EQ(rowCount("local_hosts"), 2);

    const QList<LocalGroupStore::Unsynced> groups = unsyncedGroups();
    ASSERT_EQ(groups.size(), 1);
    EXPECT_FALSE(groups[0].deleted);
    EXPECT_EQ(groups[0].sync_revision, 0);
    EXPECT_EQ(groups[0].sync_dirty, 1);

    const QList<LocalHostStore::Unsynced> hosts = unsyncedLocalHosts();
    ASSERT_EQ(hosts.size(), 2);

    for (const LocalHostStore::Unsynced& item : hosts)
    {
        EXPECT_FALSE(item.deleted);
        EXPECT_EQ(item.sync_revision, 0);
        EXPECT_EQ(item.sync_dirty, 1);
    }
}
