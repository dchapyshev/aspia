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

#include "client/database.h"

#include <QDateTime>
#include <QTemporaryDir>
#include <QThread>

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "proto/router.h"

class DatabaseTest : public testing::Test
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

    // Writes a check time the public API cannot produce, since it always stamps the current one.
    bool setRouterHostCheckTime(qint64 router_id, HostId host_id, qint64 check_time)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        SqlQuery query(raw, "UPDATE router_hosts SET check_time=? WHERE router_id=? AND host_id=?");
        query.addInt64(check_time);
        query.addInt64(router_id);
        query.addUInt64(host_id);

        return query.exec();
    }

    qint64 addGroup(const QString& name, qint64 parent_id)
    {
        LocalGroupConfig group;
        group.setName(name);
        group.setParentId(parent_id);

        EXPECT_TRUE(db_.addLocalGroup(group));
        return group.id();
    }

    qint64 addHost(const QString& name, qint64 group_id)
    {
        LocalHostConfig host;
        host.setName(name);
        host.setAddress("192.168.0.1");
        host.setGroupId(group_id);

        EXPECT_TRUE(db_.addLocalHost(host));
        return host.id();
    }

    qint64 addRouter(const QString& name)
    {
        RouterConfig router;
        router.setDisplayName(name);
        router.setAddress("router.example.com");
        router.setUsername("router-user");
        router.setPassword(SecureString("router-secret"));

        EXPECT_TRUE(db_.addRouter(router));
        return router.routerId();
    }

    RouterHostConfig routerHost(qint64 router_id, HostId host_id, const QString& username,
                                const QString& password)
    {
        RouterHostConfig host;
        host.setRouterId(router_id);
        host.setHostId(host_id);
        host.setUsername(username);
        host.setPassword(SecureString(password));
        return host;
    }

    void addRouterHost(qint64 router_id, HostId host_id, const QString& username,
                       const QString& password)
    {
        EXPECT_TRUE(db_.addRouterHost(routerHost(router_id, host_id, username, password)));
    }

    QStringList groupNames()
    {
        QStringList names;
        for (const LocalGroupConfig& group : db_.allLocalGroups())
            names.append(group.name());
        names.sort();
        return names;
    }

    QStringList hostNamesOfGroup(qint64 group_id)
    {
        QStringList names;
        for (const LocalHostConfig& host : db_.localHostList(group_id))
            names.append(host.name());
        names.sort();
        return names;
    }

    Database db_;
    QString file_path_;

private:
    QTemporaryDir dir_;
};

//--------------------------------------------------------------------------------------------------
// A group holds groups of its own, and deleting it deletes them too. A child left behind keeps
// pointing at a parent that is gone: the tree is walked from the root down, so such a group is in
// the book but in no place the user can ever reach.
TEST_F(DatabaseTest, RemovedGroupTakesItsChildGroupsWithIt)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 child = addGroup("child", parent);
    addGroup("grandchild", child);
    addGroup("other", 0);

    ASSERT_TRUE(db_.removeLocalGroup(parent));

    EXPECT_EQ(groupNames(), QStringList({ "other" }));
}

//--------------------------------------------------------------------------------------------------
// The hosts of the subtree are not deleted with it - they move to the root, where the user can see
// them and decide what to do. Left in a group that no longer exists they would show up in a search
// and nowhere else.
TEST_F(DatabaseTest, HostsOfARemovedGroupMoveToTheRoot)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 child = addGroup("child", parent);

    addHost("host-of-parent", parent);
    addHost("host-of-child", child);
    addHost("host-of-root", 0);

    ASSERT_TRUE(db_.removeLocalGroup(parent));

    EXPECT_EQ(hostNamesOfGroup(0),
              QStringList({ "host-of-child", "host-of-parent", "host-of-root" }));
}

//--------------------------------------------------------------------------------------------------
// The root is not a record of the table but the parent every group of the top level names. Asked to
// delete it, the book would answer by emptying itself.
TEST_F(DatabaseTest, RootIsNotAGroupAndIsNotRemoved)
{
    const qint64 group = addGroup("group", 0);
    addHost("host", group);

    EXPECT_FALSE(db_.removeLocalGroup(0));

    EXPECT_EQ(groupNames(), QStringList({ "group" }));
    EXPECT_EQ(hostNamesOfGroup(group), QStringList({ "host" }));
}

//--------------------------------------------------------------------------------------------------
// A group put inside its own subtree closes a loop. The tree is walked from the root down, so the
// group and everything under it drop out of it for good; and a path built by walking parents upward
// from a host inside the loop never ends.
TEST_F(DatabaseTest, GroupIsNotMovedIntoItsOwnSubtree)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 child = addGroup("child", parent);
    const qint64 grandchild = addGroup("grandchild", child);

    EXPECT_FALSE(db_.moveLocalGroup(parent, grandchild));
    EXPECT_FALSE(db_.moveLocalGroup(parent, parent));

    EXPECT_EQ(db_.findLocalGroup(parent)->parentId(), 0);
    EXPECT_EQ(db_.localGroupList(0).size(), 1);
}

//--------------------------------------------------------------------------------------------------
// Editing a group is the other way its parent is written, and it is no different.
TEST_F(DatabaseTest, EditedGroupIsNotMadeAChildOfItsOwnChild)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 child = addGroup("child", parent);

    LocalGroupConfig group = *db_.findLocalGroup(parent);
    group.setParentId(child);

    EXPECT_FALSE(db_.modifyLocalGroup(group));

    EXPECT_EQ(db_.findLocalGroup(parent)->parentId(), 0);
}

//--------------------------------------------------------------------------------------------------
// A move to a group that is not below it is what the check is there to allow.
TEST_F(DatabaseTest, GroupIsMovedUnderAGroupOutsideItsSubtree)
{
    const qint64 first = addGroup("first", 0);
    addGroup("child", first);
    const qint64 second = addGroup("second", 0);

    EXPECT_TRUE(db_.moveLocalGroup(first, second));
    EXPECT_EQ(db_.findLocalGroup(first)->parentId(), second);

    // And back to the root, which is not a group and cannot be below anything.
    EXPECT_TRUE(db_.moveLocalGroup(first, 0));
    EXPECT_EQ(db_.findLocalGroup(first)->parentId(), 0);
}

//--------------------------------------------------------------------------------------------------
// Changing the master password rewrites every record under the new key. The record itself did not
// change, so the moment it was last edited must survive: it is a column of the list the user reads.
TEST_F(DatabaseTest, ReencryptionKeepsTheMomentARecordWasEdited)
{
    const qint64 group = addGroup("group", 0);
    const qint64 entry_id = addHost("host", group);

    const qint64 modify_time = db_.findLocalHost(entry_id)->modifyTime();

    // The stamp counts whole seconds, so a rewrite within the same second is indistinguishable from
    // one that left it alone.
    QThread::msleep(1100);

    ASSERT_TRUE(db_.reencryptAll(db_.allLocalHosts(), db_.routerList(), db_.allRouterHosts(),
                                 "salt", "verifier", 1));

    EXPECT_EQ(db_.findLocalHost(entry_id)->modifyTime(), modify_time);
}

//--------------------------------------------------------------------------------------------------
// Only the subtree of the group goes. A neighbour that hangs off the same parent stays as it is,
// with the hosts it holds.
TEST_F(DatabaseTest, RemovedGroupLeavesItsNeighbourAlone)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 first = addGroup("first", parent);
    const qint64 second = addGroup("second", parent);

    addHost("host-of-first", first);
    addHost("host-of-second", second);

    ASSERT_TRUE(db_.removeLocalGroup(first));

    EXPECT_EQ(groupNames(), QStringList({ "parent", "second" }));
    EXPECT_EQ(hostNamesOfGroup(second), QStringList({ "host-of-second" }));
    EXPECT_EQ(hostNamesOfGroup(0), QStringList({ "host-of-first" }));
}

//--------------------------------------------------------------------------------------------------
// A group is named by its id inside this database only. The guid names the same group in any
// database that ever hears about it, so every group gets one the moment it is created.
TEST_F(DatabaseTest, GroupGetsAGuidOfItsOwn)
{
    const qint64 first = addGroup("first", 0);
    const qint64 second = addGroup("second", 0);

    const std::optional<LocalGroupConfig> stored = db_.findLocalGroup(first);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->guid().isEmpty());

    const std::optional<LocalGroupConfig> other = db_.findLocalGroup(second);
    ASSERT_TRUE(other.has_value());
    EXPECT_NE(stored->guid(), other->guid());
}

//--------------------------------------------------------------------------------------------------
// A guid an import brings from elsewhere is kept as it is. The group arriving here is the group
// the file names, not a new one.
TEST_F(DatabaseTest, GroupKeepsTheGuidItWasGiven)
{
    LocalGroupConfig group;
    group.setName("from-a-file");
    group.setGuid("7b0f1d18-0e3a-4a0e-9a4e-2a1c6a0f0c11");

    ASSERT_TRUE(db_.addLocalGroup(group));

    const std::optional<LocalGroupConfig> stored = db_.findLocalGroup(group.id());
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->guid(), "7b0f1d18-0e3a-4a0e-9a4e-2a1c6a0f0c11");
}

//--------------------------------------------------------------------------------------------------
// Renaming a group or moving it elsewhere in the tree leaves it the same group.
TEST_F(DatabaseTest, GroupGuidSurvivesEveryEdit)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 group = addGroup("group", 0);

    const std::optional<LocalGroupConfig> before = db_.findLocalGroup(group);
    ASSERT_TRUE(before.has_value());

    LocalGroupConfig edited = *before;
    edited.setName("renamed");
    edited.setComment("comment");
    ASSERT_TRUE(db_.modifyLocalGroup(edited));
    ASSERT_TRUE(db_.moveLocalGroup(group, parent));

    const std::optional<LocalGroupConfig> after = db_.findLocalGroup(group);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->name(), "renamed");
    EXPECT_EQ(after->parentId(), parent);
    EXPECT_EQ(after->guid(), before->guid());
}

//--------------------------------------------------------------------------------------------------
// The form the user edits knows nothing about the guid, so a record built out of it carries none.
// Saving that record must not take the guid away, because the deep links naming the router are held
// by it.
TEST_F(DatabaseTest, RouterKeepsItsGuidThroughAnEdit)
{
    const qint64 router_id = addRouter("router");

    const std::optional<RouterConfig> before = db_.findRouter(router_id);
    ASSERT_TRUE(before.has_value());
    ASSERT_FALSE(before->guid().isEmpty());

    // What both editors of a router hand over: the fields of the form and nothing else.
    RouterConfig edited;
    edited.setRouterId(router_id);
    edited.setDisplayName("renamed");
    edited.setAddress("router.example.com");
    edited.setSessionType(proto::router::SESSION_TYPE_OPERATOR);
    edited.setUsername("router-user");
    edited.setPassword(SecureString("router-secret"));

    ASSERT_TRUE(db_.modifyRouter(edited));

    const std::optional<RouterConfig> after = db_.findRouter(router_id);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->displayName(), "renamed");
    EXPECT_EQ(after->guid(), before->guid());
}

//--------------------------------------------------------------------------------------------------
// A guid names one group, and the record naming it is the one an import looks for before it decides
// whether the group of the file is already here.
TEST_F(DatabaseTest, TwoGroupsDoNotShareOneGuid)
{
    const qint64 group = addGroup("group", 0);

    const std::optional<LocalGroupConfig> stored = db_.findLocalGroup(group);
    ASSERT_TRUE(stored.has_value());
    ASSERT_FALSE(stored->guid().isEmpty());

    LocalGroupConfig copy;
    copy.setName("copy");
    copy.setParentId(0);
    copy.setGuid(stored->guid());

    EXPECT_FALSE(db_.addLocalGroup(copy));
    EXPECT_EQ(groupNames(), QStringList({ "group" }));
}

//--------------------------------------------------------------------------------------------------
// A guid names one host, and the deep links the user hands out are built on it.
TEST_F(DatabaseTest, TwoLocalHostsDoNotShareOneGuid)
{
    const qint64 group = addGroup("group", 0);
    const qint64 host = addHost("host", group);

    const std::optional<LocalHostConfig> stored = db_.findLocalHost(host);
    ASSERT_TRUE(stored.has_value());
    ASSERT_FALSE(stored->guid().isEmpty());

    LocalHostConfig copy;
    copy.setName("copy");
    copy.setAddress("192.168.0.2");
    copy.setGroupId(group);
    copy.setGuid(stored->guid());

    EXPECT_FALSE(db_.addLocalHost(copy));
    EXPECT_EQ(hostNamesOfGroup(group), QStringList({ "host" }));
}

//--------------------------------------------------------------------------------------------------
// A router is named by its id inside this database only. The guid names the same record in every
// database that hears about it, and the deep links are built on it, so every router gets one the
// moment it is added.
TEST_F(DatabaseTest, RouterGetsAGuidOfItsOwn)
{
    const qint64 first = addRouter("first");
    const qint64 second = addRouter("second");

    const std::optional<RouterConfig> stored = db_.findRouter(first);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->guid().isEmpty());

    const std::optional<RouterConfig> other = db_.findRouter(second);
    ASSERT_TRUE(other.has_value());
    EXPECT_NE(stored->guid(), other->guid());
}

//--------------------------------------------------------------------------------------------------
// The secret part of a record goes to the database as one encrypted column. What comes back out has
// to be what went in, every field of it.
TEST_F(DatabaseTest, EncryptedDataSurvivesARoundTrip)
{
    const qint64 group = addGroup("group", 0);

    LocalHostConfig host;
    host.setName("host");
    host.setGroupId(group);
    host.setAddress("192.168.0.1");
    host.setUsername("user");
    host.setPassword(SecureString("secret"));
    ASSERT_TRUE(db_.addLocalHost(host));

    std::optional<LocalHostConfig> stored = db_.findLocalHost(host.id());
    ASSERT_TRUE(stored.has_value());

    EXPECT_EQ(stored->address(), QString("192.168.0.1"));
    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// A host and a router number their fields the same way, so a router column would parse as a host one
// and hand out an address that was never a host. The seal names the table it was made for, and a
// column carried across tables does not open.
TEST_F(DatabaseTest, RouterDataDoesNotOpenAsHostData)
{
    RouterConfig router;
    router.setDisplayName("router");
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
    ASSERT_TRUE(db_.addLocalHost(host));

    QList<LocalHostConfig> hosts = db_.allLocalHosts();
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

    ASSERT_TRUE(db_.reencryptAll(hosts, db_.routerList(), db_.allRouterHosts(),
                                 "salt", "verifier", 1));

    std::optional<LocalHostConfig> stored = db_.findLocalHost(host.id());
    ASSERT_TRUE(stored.has_value());

    EXPECT_EQ(stored->address(), QString("192.168.0.1"));
    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// The credentials the user saved for a host of a router come back as they were saved.
TEST_F(DatabaseTest, RouterHostCredentialsSurviveARoundTrip)
{
    const qint64 router_id = addRouter("router");

    ASSERT_TRUE(db_.addRouterHost(routerHost(router_id, 100500, "user", "secret")));

    std::optional<RouterHostConfig> stored = db_.findRouterHost(router_id, 100500);
    ASSERT_TRUE(stored.has_value());

    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// Credentials are added once and edited afterwards. Adding twice for the same host is refused,
// and removing takes the row out.
TEST_F(DatabaseTest, RouterHostCredentialsAreEditedAndRemoved)
{
    const qint64 router_id = addRouter("router");

    addRouterHost(router_id, 100500, "user", "secret");

    EXPECT_FALSE(db_.addRouterHost(routerHost(router_id, 100500, "other-user", "other-secret")));
    EXPECT_TRUE(db_.modifyRouterHost(routerHost(router_id, 100500, "other-user", "other-secret")));

    // A host the user never saved anything for has no row to edit.
    EXPECT_FALSE(db_.modifyRouterHost(routerHost(router_id, 100501, "user", "secret")));

    std::optional<RouterHostConfig> stored = db_.findRouterHost(router_id, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->username(), QString("other-user"));
    EXPECT_EQ(stored->password().toString(), QString("other-secret"));
    EXPECT_EQ(db_.allRouterHosts().size(), 1);

    ASSERT_TRUE(db_.removeRouterHost(router_id, 100500));
    EXPECT_FALSE(db_.findRouterHost(router_id, 100500).has_value());
    EXPECT_TRUE(db_.allRouterHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A router record is what the credentials hang on, so removing it takes them along. The credentials
// of another router are not touched.
TEST_F(DatabaseTest, RemovedRouterTakesItsHostCredentialsWithIt)
{
    const qint64 first = addRouter("first");
    const qint64 second = addRouter("second");

    addRouterHost(first, 100500, "user", "secret");
    addRouterHost(second, 100501, "other-user", "other-secret");

    ASSERT_TRUE(db_.removeRouter(first));

    EXPECT_FALSE(db_.findRouterHost(first, 100500).has_value());
    EXPECT_TRUE(db_.findRouterHost(second, 100501).has_value());
}

//--------------------------------------------------------------------------------------------------
// Every column of the table opens with the same key, so what tells one row from another is the host
// id the column was sealed for. A column moved to the row of another host does not open.
TEST_F(DatabaseTest, RouterHostCredentialsDoNotOpenForAnotherHost)
{
    RouterHostConfig host;
    host.setRouterId(1);
    host.setHostId(100500);
    host.setUsername("user");
    host.setPassword(SecureString("secret"));

    std::optional<QByteArray> sealed = host.encryptedData();
    ASSERT_TRUE(sealed.has_value());

    RouterHostConfig other;
    other.setRouterId(1);
    other.setHostId(100501);

    EXPECT_FALSE(other.setEncryptedData(*sealed));
    EXPECT_TRUE(other.username().isEmpty());
    EXPECT_TRUE(other.password().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The row is keyed by the router and the host together, so the credentials do not open for the same
// host reached through another router either.
TEST_F(DatabaseTest, RouterHostCredentialsDoNotOpenForAnotherRouter)
{
    RouterHostConfig host;
    host.setRouterId(1);
    host.setHostId(100500);
    host.setUsername("user");
    host.setPassword(SecureString("secret"));

    std::optional<QByteArray> sealed = host.encryptedData();
    ASSERT_TRUE(sealed.has_value());

    RouterHostConfig other;
    other.setRouterId(2);
    other.setHostId(100500);

    EXPECT_FALSE(other.setEncryptedData(*sealed));
    EXPECT_TRUE(other.username().isEmpty());
    EXPECT_TRUE(other.password().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The router is the only one who knows that a host is gone, so the credentials kept for it are
// checked against the router now and then. Offered are the rows the router was not asked about for
// long enough, and asking is remembered.
TEST_F(DatabaseTest, OutdatedRouterHostsAreTheOnesNotCheckedForLong)
{
    const qint64 router_id = addRouter("router");

    addRouterHost(router_id, 100500, "user", "secret");
    addRouterHost(router_id, 100501, "other-user", "other-secret");
    addRouterHost(router_id, 100502, "third-user", "third-secret");

    // The router was never asked about any of them.
    QList<HostId> outdated = db_.outdatedRouterHosts(router_id);
    EXPECT_EQ(outdated.size(), 3);
    EXPECT_TRUE(outdated.contains(100500));
    EXPECT_TRUE(outdated.contains(100501));
    EXPECT_TRUE(outdated.contains(100502));

    ASSERT_TRUE(db_.updateRouterHostCheckTime(router_id, 100500));
    ASSERT_TRUE(db_.updateRouterHostCheckTime(router_id, 100501));
    ASSERT_TRUE(db_.updateRouterHostCheckTime(router_id, 100502));

    EXPECT_TRUE(db_.outdatedRouterHosts(router_id).isEmpty());

    // The answer of the router is trusted for a week. A row asked about a day ago is not offered
    // again, and one asked about eight days ago is.
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 day = 24 * 60 * 60;

    ASSERT_TRUE(setRouterHostCheckTime(router_id, 100501, now - day));
    ASSERT_TRUE(setRouterHostCheckTime(router_id, 100502, now - 8 * day));

    EXPECT_EQ(db_.outdatedRouterHosts(router_id), QList<HostId>({ HostId(100502) }));

    // There is nothing to remember for a host the user saved nothing for.
    EXPECT_FALSE(db_.updateRouterHostCheckTime(router_id, 100503));
}

//--------------------------------------------------------------------------------------------------
// Every offered row costs a request to the router, so a single call never takes more than a handful
// of them. What one call left behind is what the next one takes.
TEST_F(DatabaseTest, OutdatedRouterHostsAreTakenInBoundedBatches)
{
    const qint64 router_id = addRouter("router");

    for (HostId host_id = 100500; host_id < 100512; ++host_id)
        addRouterHost(router_id, host_id, "user", "secret");

    // Twelve rows are waiting, and no answer carries more than ten.
    const QList<HostId> first = db_.outdatedRouterHosts(router_id);
    EXPECT_EQ(first.size(), 10);

    for (HostId host_id : first)
        ASSERT_TRUE(db_.updateRouterHostCheckTime(router_id, host_id));

    const QList<HostId> second = db_.outdatedRouterHosts(router_id);
    EXPECT_EQ(second.size(), 2);

    for (HostId host_id : second)
    {
        EXPECT_FALSE(first.contains(host_id));
        ASSERT_TRUE(db_.updateRouterHostCheckTime(router_id, host_id));
    }

    EXPECT_TRUE(db_.outdatedRouterHosts(router_id).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A session asks its own router about its own rows: the rows of another router are none of its
// business, and the same host id under another router is another row.
TEST_F(DatabaseTest, OutdatedRouterHostsBelongToTheirOwnRouter)
{
    const qint64 first = addRouter("first");
    const qint64 second = addRouter("second");

    addRouterHost(first, 100500, "user", "secret");
    addRouterHost(second, 100501, "other-user", "other-secret");

    EXPECT_EQ(db_.outdatedRouterHosts(first), QList<HostId>({ HostId(100500) }));
    EXPECT_EQ(db_.outdatedRouterHosts(second), QList<HostId>({ HostId(100501) }));

    ASSERT_TRUE(db_.updateRouterHostCheckTime(first, 100500));

    EXPECT_TRUE(db_.outdatedRouterHosts(first).isEmpty());
    EXPECT_EQ(db_.outdatedRouterHosts(second), QList<HostId>({ HostId(100501) }));

    EXPECT_FALSE(db_.updateRouterHostCheckTime(second, 100500));
}

//--------------------------------------------------------------------------------------------------
// Editing the credentials, and resealing them under a new master password, says nothing about the
// host still being there. Neither may pass for an answer of the router.
TEST_F(DatabaseTest, EditedRouterHostCredentialsKeepTheirCheckTime)
{
    const qint64 router_id = addRouter("router");

    // One row the router has answered about and one it has not. An edit that moves the check time
    // in either direction shows up as a change of what is offered for checking.
    addRouterHost(router_id, 100500, "user", "secret");
    addRouterHost(router_id, 100501, "other-user", "other-secret");

    ASSERT_TRUE(db_.updateRouterHostCheckTime(router_id, 100500));
    ASSERT_EQ(db_.outdatedRouterHosts(router_id), QList<HostId>({ HostId(100501) }));

    ASSERT_TRUE(db_.modifyRouterHost(routerHost(router_id, 100500, "edited-user", "edited-secret")));
    ASSERT_TRUE(db_.modifyRouterHost(routerHost(router_id, 100501, "third-user", "third-secret")));

    EXPECT_EQ(db_.outdatedRouterHosts(router_id), QList<HostId>({ HostId(100501) }));

    QList<RouterHostConfig> router_hosts = db_.allRouterHosts();
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

    ASSERT_TRUE(db_.reencryptAll(db_.allLocalHosts(), QList<RouterConfig>(), router_hosts,
                                 "salt", "verifier", 1));

    EXPECT_EQ(db_.outdatedRouterHosts(router_id), QList<HostId>({ HostId(100501) }));
}

//--------------------------------------------------------------------------------------------------
// Changing the master password reseals the credentials of router hosts as well. Left out, they would
// stay under the old key and never open again.
TEST_F(DatabaseTest, ReencryptionKeepsRouterHostCredentialsReadable)
{
    const qint64 router_id = addRouter("router");
    addRouterHost(router_id, 100500, "user", "secret");

    QList<RouterHostConfig> router_hosts = db_.allRouterHosts();
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

    ASSERT_TRUE(db_.reencryptAll(db_.allLocalHosts(), QList<RouterConfig>(), router_hosts,
                                 "salt", "verifier", 1));

    std::optional<RouterHostConfig> stored = db_.findRouterHost(router_id, 100500);
    ASSERT_TRUE(stored.has_value());

    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));
}
//--------------------------------------------------------------------------------------------------
// A router is entered with an account of its own, so one without a password could never be
// connected to. What the readers of the address book call invalid does not get in.
TEST_F(DatabaseTest, RouterWithoutAPasswordIsNotStored)
{
    RouterConfig router;
    router.setDisplayName("router");
    router.setAddress("router.example.com");
    router.setUsername("router-user");

    EXPECT_FALSE(db_.addRouter(router));
    EXPECT_TRUE(db_.routerList().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// An edit cannot leave behind what an insert would have refused.
TEST_F(DatabaseTest, RouterCannotBeEditedIntoOneWithoutAPassword)
{
    const qint64 router_id = addRouter("router");

    RouterConfig edited;
    edited.setRouterId(router_id);
    edited.setDisplayName("renamed");
    edited.setAddress("router.example.com");
    edited.setUsername("router-user");

    EXPECT_FALSE(db_.modifyRouter(edited));

    const std::optional<RouterConfig> after = db_.findRouter(router_id);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->displayName(), "router");
    EXPECT_FALSE(after->password().isEmpty());
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
    EXPECT_FALSE(db_.addLocalGroup(group));

    LocalGroupConfig unnamed;
    EXPECT_FALSE(db_.addLocalGroup(unnamed));

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setComment(long_comment);
    EXPECT_FALSE(db_.addLocalHost(host));

    // An address is what the host is reached at, and there is no host without one.
    LocalHostConfig without_address;
    without_address.setName("host");
    without_address.setUsername("user");
    without_address.setPassword(SecureString(QString("secret")));
    EXPECT_FALSE(db_.addLocalHost(without_address));

    RouterConfig router;
    router.setDisplayName(long_name);
    router.setAddress("router.example.com");
    router.setUsername("router-user");
    router.setPassword(SecureString(QString("router-secret")));
    EXPECT_FALSE(db_.addRouter(router));

    EXPECT_TRUE(db_.allLocalGroups().isEmpty());
    EXPECT_TRUE(db_.allLocalHosts().isEmpty());
    EXPECT_TRUE(db_.routerList().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The credentials of a host are kept as a pair. An empty pair means the host is asked for them at
// every connection, and half a pair says neither.
TEST_F(DatabaseTest, HostWithHalfItsCredentialsIsNotStored)
{
    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setUsername("user");

    EXPECT_FALSE(db_.addLocalHost(host));
    EXPECT_TRUE(db_.allLocalHosts().isEmpty());

    host.setPassword(SecureString(QString("secret")));
    ASSERT_TRUE(db_.addLocalHost(host));

    LocalHostConfig edited = host;
    edited.setPassword(SecureString());
    EXPECT_FALSE(db_.modifyLocalHost(edited));

    const std::optional<LocalHostConfig> after = db_.findLocalHost(host.id());
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->password().toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// A temporary id is handed out again to another host once the one holding it is gone, so what was
// saved under it would be sent to a host the user never gave it to.
TEST_F(DatabaseTest, CredentialsOfATemporaryHostAreNotStored)
{
    const qint64 router_id = addRouter("router");

    RouterHostConfig credentials;
    credentials.setRouterId(router_id);
    credentials.setHostId(kMinTempHostId);
    credentials.setUsername("user");
    credentials.setPassword(SecureString(QString("secret")));

    EXPECT_FALSE(db_.addRouterHost(credentials));
    EXPECT_FALSE(db_.modifyRouterHost(credentials));
    EXPECT_TRUE(db_.allRouterHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Credentials are a pair here as well. The row exists to answer a host that asks, and half an
// answer is no answer.
TEST_F(DatabaseTest, CredentialsWithoutAPasswordAreNotStored)
{
    const qint64 router_id = addRouter("router");

    RouterHostConfig credentials;
    credentials.setRouterId(router_id);
    credentials.setHostId(100500);
    credentials.setUsername("user");

    EXPECT_FALSE(db_.addRouterHost(credentials));
    EXPECT_TRUE(db_.allRouterHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A batch is written whole. The records name each other by their place in it, because the ids they
// will have do not exist until they are written.
TEST_F(DatabaseTest, BatchIsWrittenWithItsOwnLinks)
{
    RouterConfig router;
    router.setRouterId(-1);
    router.setDisplayName("router");
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
    host.setRouterId(-1);

    RouterHostConfig credentials;
    credentials.setRouterId(-1);
    credentials.setHostId(100500);
    credentials.setUsername("user");
    credentials.setPassword(SecureString(QString("secret")));

    ASSERT_TRUE(db_.import({ router }, { parent, child }, { host }, { credentials }));

    const QList<RouterConfig> routers = db_.routerList();
    ASSERT_EQ(routers.size(), 1);

    const QList<LocalGroupConfig> groups = db_.allLocalGroups();
    ASSERT_EQ(groups.size(), 2);

    const std::optional<LocalGroupConfig> stored_parent = db_.findLocalGroup(groups.front().id());
    const std::optional<LocalGroupConfig> stored_child = db_.findLocalGroup(groups.back().id());
    ASSERT_TRUE(stored_parent.has_value() && stored_child.has_value());

    EXPECT_EQ(stored_parent->parentId(), 0);
    EXPECT_EQ(stored_child->parentId(), stored_parent->id());

    const QList<LocalHostConfig> hosts = db_.allLocalHosts();
    ASSERT_EQ(hosts.size(), 1);
    EXPECT_EQ(hosts.front().groupId(), stored_child->id());
    EXPECT_EQ(hosts.front().routerId(), routers.front().routerId());

    const QList<RouterHostConfig> stored_credentials = db_.allRouterHosts();
    ASSERT_EQ(stored_credentials.size(), 1);
    EXPECT_EQ(stored_credentials.front().routerId(), routers.front().routerId());
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

    EXPECT_FALSE(db_.import({}, { group }, { host, broken }, {}));

    EXPECT_EQ(db_.allLocalGroups().size(), 1);
    EXPECT_TRUE(db_.allLocalHosts().isEmpty());
}
