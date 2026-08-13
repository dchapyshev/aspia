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

#include <QTemporaryDir>
#include <QThread>

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"

class DatabaseTest : public testing::Test
{
protected:
    void SetUp() override
    {
        // A record of the address book keeps its text encrypted with a key of the process, which
        // the application sets once the master password is entered.
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

        ASSERT_TRUE(dir_.isValid());
        ASSERT_TRUE(db_.open(dir_.filePath("client.db3")));
    }

    qint64 addGroup(const QString& name, qint64 parent_id)
    {
        GroupConfig group;
        group.setName(name);
        group.setParentId(parent_id);

        EXPECT_TRUE(db_.addGroup(group));
        return group.id();
    }

    qint64 addHost(const QString& name, qint64 group_id)
    {
        HostConfig host;
        host.setName(name);
        host.setAddress("192.168.0.1");
        host.setGroupId(group_id);

        EXPECT_TRUE(db_.addHost(host));
        return host.id();
    }

    QStringList groupNames()
    {
        QStringList names;
        for (const GroupConfig& group : db_.allGroups())
            names.append(group.name());
        names.sort();
        return names;
    }

    QStringList hostNamesOfGroup(qint64 group_id)
    {
        QStringList names;
        for (const HostConfig& host : db_.hostList(group_id))
            names.append(host.name());
        names.sort();
        return names;
    }

    Database db_;

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

    ASSERT_TRUE(db_.removeGroup(parent));

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

    ASSERT_TRUE(db_.removeGroup(parent));

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

    EXPECT_FALSE(db_.removeGroup(0));

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

    EXPECT_FALSE(db_.moveGroup(parent, grandchild));
    EXPECT_FALSE(db_.moveGroup(parent, parent));

    EXPECT_EQ(db_.findGroup(parent)->parentId(), 0);
    EXPECT_EQ(db_.groupList(0).size(), 1);
}

//--------------------------------------------------------------------------------------------------
// Editing a group is the other way its parent is written, and it is no different.
TEST_F(DatabaseTest, EditedGroupIsNotMadeAChildOfItsOwnChild)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 child = addGroup("child", parent);

    GroupConfig group = *db_.findGroup(parent);
    group.setParentId(child);

    EXPECT_FALSE(db_.modifyGroup(group));

    EXPECT_EQ(db_.findGroup(parent)->parentId(), 0);
}

//--------------------------------------------------------------------------------------------------
// A move to a group that is not below it is what the check is there to allow.
TEST_F(DatabaseTest, GroupIsMovedUnderAGroupOutsideItsSubtree)
{
    const qint64 first = addGroup("first", 0);
    addGroup("child", first);
    const qint64 second = addGroup("second", 0);

    EXPECT_TRUE(db_.moveGroup(first, second));
    EXPECT_EQ(db_.findGroup(first)->parentId(), second);

    // And back to the root, which is not a group and cannot be below anything.
    EXPECT_TRUE(db_.moveGroup(first, 0));
    EXPECT_EQ(db_.findGroup(first)->parentId(), 0);
}

//--------------------------------------------------------------------------------------------------
// Changing the master password rewrites every record under the new key. The record itself did not
// change, so the moment it was last edited must survive: it is a column of the list the user reads.
TEST_F(DatabaseTest, ReencryptionKeepsTheMomentARecordWasEdited)
{
    const qint64 group = addGroup("group", 0);
    const qint64 entry_id = addHost("host", group);

    const qint64 modify_time = db_.findHost(entry_id)->modifyTime();

    // The stamp counts whole seconds, so a rewrite within the same second is indistinguishable from
    // one that left it alone.
    QThread::msleep(1100);

    ASSERT_TRUE(db_.reencryptAll(db_.allHosts(), db_.routerList(), "salt", "verifier", 1));

    EXPECT_EQ(db_.findHost(entry_id)->modifyTime(), modify_time);
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

    ASSERT_TRUE(db_.removeGroup(first));

    EXPECT_EQ(groupNames(), QStringList({ "parent", "second" }));
    EXPECT_EQ(hostNamesOfGroup(second), QStringList({ "host-of-second" }));
    EXPECT_EQ(hostNamesOfGroup(0), QStringList({ "host-of-first" }));
}

//--------------------------------------------------------------------------------------------------
// The secret part of a record goes to the database as one encrypted column. What comes back out has
// to be what went in, every field of it.
TEST_F(DatabaseTest, EncryptedDataSurvivesARoundTrip)
{
    const qint64 group = addGroup("group", 0);

    HostConfig host;
    host.setName("host");
    host.setGroupId(group);
    host.setAddress("192.168.0.1");
    host.setUsername("user");
    host.setPassword(SecureString("secret"));
    ASSERT_TRUE(db_.addHost(host));

    std::optional<HostConfig> stored = db_.findHost(host.id());
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

    HostConfig host;
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
    HostConfig host;
    host.setAddress("192.168.0.1");
    host.setUsername("user");
    host.setPassword(SecureString("secret"));

    std::optional<QByteArray> sealed = host.encryptedData();
    ASSERT_TRUE(sealed.has_value());

    QByteArray tampered = *sealed;
    tampered[tampered.size() - 1] = static_cast<char>(tampered[tampered.size() - 1] ^ 0x01);

    HostConfig target;
    EXPECT_FALSE(target.setEncryptedData(tampered));
    EXPECT_TRUE(target.address().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Changing the master password reseals every record. The fields must read the same afterwards,
// under the key the new password derives.
TEST_F(DatabaseTest, ReencryptionKeepsDataReadable)
{
    const qint64 group = addGroup("group", 0);

    HostConfig host;
    host.setName("host");
    host.setGroupId(group);
    host.setAddress("192.168.0.1");
    host.setUsername("user");
    host.setPassword(SecureString("secret"));
    ASSERT_TRUE(db_.addHost(host));

    QList<HostConfig> hosts = db_.allHosts();
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

    ASSERT_TRUE(db_.reencryptAll(hosts, db_.routerList(), "salt", "verifier", 1));

    std::optional<HostConfig> stored = db_.findHost(host.id());
    ASSERT_TRUE(stored.has_value());

    EXPECT_EQ(stored->address(), QString("192.168.0.1"));
    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));
}
