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
#include <QTemporaryDir>
#include <QThread>

#include <gtest/gtest.h>

#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/os_crypt.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/net/udp_channel.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "client/router/router_test_fixture.h"
#include "client/storage/master_password.h"
#include "proto/desktop_control.h"
#include "proto/router.h"
#include "proto/storage.h"

namespace {

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
std::optional<LocalHostConfig> findLocalHost(const Database& db, qint64 entry_id)
{
    LocalHostConfig host;
    const Database::FindResult result = db.findLocalHost(entry_id, &host);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return host;
}

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
std::optional<LocalGroupConfig> findLocalGroup(const Database& db, qint64 group_id)
{
    LocalGroupConfig group;
    const Database::FindResult result = db.findLocalGroup(group_id, &group);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return group;
}

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
std::optional<RouterConfig> findRouter(const Database& db)
{
    RouterConfig router;
    const Database::FindResult result = db.routerConfig(&router);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return router;
}

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
std::optional<CredentialConfig> findCredential(const Database& db, qint64 credential_id)
{
    CredentialConfig credential;
    const Database::FindResult result = db.findCredential(credential_id, &credential);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return credential;
}

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
std::optional<CredentialConfig> findCredentialByGuid(const Database& db, const QString& guid)
{
    CredentialConfig credential;
    const Database::FindResult result = db.findCredentialByGuid(guid, &credential);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return credential;
}

//--------------------------------------------------------------------------------------------------
// The record of the base, when it was read and the base has one.
std::optional<RouterHostConfig> findRouterHost(const Database& db, HostId host_id)
{
    RouterHostConfig host;
    const Database::FindResult result = db.findRouterHost(host_id, &host);
    if (result != Database::FindResult::FOUND)
    {
        EXPECT_EQ(result, Database::FindResult::NOT_FOUND);
        return std::nullopt;
    }

    return host;
}

} // namespace

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
    bool setRouterHostCheckTime(HostId host_id, qint64 check_time)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        SqlQuery query(raw, "UPDATE router_hosts SET check_time=? WHERE host_id=?");
        query.addInt64(check_time);
        query.addUInt64(host_id);

        return query.exec();
    }

    // Spoils the sealed text of a record the way a damaged database or one of another installation
    // has it: the row is in place, its data does not open.
    bool corruptRecordData(const QString& table, qint64 id)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        const std::string sql =
            QString("UPDATE %1 SET data=X'00' WHERE id=?").arg(table).toStdString();

        SqlQuery query(raw, sql);
        query.addInt64(id);

        return query.exec();
    }

    // The same for the router, which is kept among the settings.
    bool corruptRouterData()
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        return raw.exec("UPDATE settings SET value='AA==' WHERE name='router'");
    }

    // The same for a record of a router host, which is named by the id of its host.
    bool corruptRouterHostData(HostId host_id)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;

        SqlQuery query(raw, "UPDATE router_hosts SET data=X'00' WHERE host_id=?");
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

    void addRouter()
    {
        RouterConfig router;
        router.setAddress("router.example.com");
        router.setUsername("router-user");
        router.setPassword(SecureString("router-secret"));

        EXPECT_TRUE(db_.setRouterConfig(router));
    }

    RouterHostConfig routerHost(HostId host_id, const QString& username, const QString& password)
    {
        RouterHostConfig host;
        host.setHostId(host_id);
        host.setUsername(username);
        host.setPassword(SecureString(password));
        return host;
    }

    void addRouterHost(HostId host_id, const QString& username, const QString& password)
    {
        EXPECT_TRUE(db_.addRouterHost(routerHost(host_id, username, password)));
    }

    QStringList groupNames()
    {
        QStringList names;
        for (const LocalGroupConfig& group : allLocalGroups())
            names.append(group.name());
        names.sort();
        return names;
    }

    QStringList hostNamesOfGroup(qint64 group_id)
    {
        QStringList names;
        for (const LocalHostConfig& host : localHostList(group_id))
            names.append(host.name());
        names.sort();
        return names;
    }

    // The list scans of the fixture: the read is expected to succeed, and the list comes back by
    // value the way the tests consume it.
    QList<LocalHostConfig> localHostList(qint64 group_id)
    {
        QList<LocalHostConfig> hosts;
        EXPECT_EQ(db_.localHostList(group_id, &hosts), Database::ReadResult::OK);
        return hosts;
    }

    QList<LocalHostConfig> allLocalHosts()
    {
        QList<LocalHostConfig> hosts;
        EXPECT_EQ(db_.allLocalHosts(&hosts), Database::ReadResult::OK);
        return hosts;
    }

    QList<LocalGroupConfig> localGroupList(qint64 parent_id)
    {
        QList<LocalGroupConfig> groups;
        EXPECT_TRUE(db_.localGroupList(parent_id, &groups));
        return groups;
    }

    QList<LocalGroupConfig> allLocalGroups()
    {
        QList<LocalGroupConfig> groups;
        EXPECT_TRUE(db_.allLocalGroups(&groups));
        return groups;
    }

    QList<RouterHostConfig> allRouterHosts()
    {
        QList<RouterHostConfig> hosts;
        EXPECT_EQ(db_.allRouterHosts(&hosts), Database::ReadResult::OK);
        return hosts;
    }

    CredentialConfig credential(const QString& name, const QString& username,
                                const QString& password)
    {
        CredentialConfig config;
        config.setDisplayName(name);
        config.setUsername(username);
        config.setPassword(SecureString(password));
        return config;
    }

    qint64 addCredential(const QString& name, const QString& username, const QString& password)
    {
        CredentialConfig config = credential(name, username, password);
        EXPECT_TRUE(db_.addCredential(config));
        return config.id();
    }

    QList<CredentialConfig> credentialList()
    {
        QList<CredentialConfig> credentials;
        EXPECT_EQ(db_.credentialList(&credentials), Database::ReadResult::OK);
        return credentials;
    }

    qint64 addProfile(const QString& name)
    {
        ProfileConfig config;
        config.setName(name);
        config.setDesktopConfig(defaultDesktopConfig());
        config.setUdpMethods(UDP_METHOD_ALL);
        EXPECT_TRUE(db_.addProfile(config));
        return config.id();
    }

    // The profiles the user made, the default one left out.
    QList<ProfileConfig> profileList()
    {
        QList<ProfileConfig> profiles;
        EXPECT_EQ(db_.profileList(&profiles), Database::ReadResult::OK);
        EXPECT_TRUE(!profiles.isEmpty() && profiles.front().isDefault());
        profiles.removeFirst();
        return profiles;
    }

    QList<HostId> outdatedRouterHosts()
    {
        QList<HostId> hosts;
        EXPECT_TRUE(db_.outdatedRouterHosts(&hosts));
        return hosts;
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

    EXPECT_EQ(findLocalGroup(db_, parent)->parentId(), 0);
    EXPECT_EQ(localGroupList(0).size(), 1);
}

//--------------------------------------------------------------------------------------------------
// Editing a group is the other way its parent is written, and it is no different.
TEST_F(DatabaseTest, EditedGroupIsNotMadeAChildOfItsOwnChild)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 child = addGroup("child", parent);

    LocalGroupConfig group = *findLocalGroup(db_, parent);
    group.setParentId(child);

    EXPECT_FALSE(db_.modifyLocalGroup(group));

    EXPECT_EQ(findLocalGroup(db_, parent)->parentId(), 0);
}

//--------------------------------------------------------------------------------------------------
// A move to a group that is not below it is what the check is there to allow.
TEST_F(DatabaseTest, GroupIsMovedUnderAGroupOutsideItsSubtree)
{
    const qint64 first = addGroup("first", 0);
    addGroup("child", first);
    const qint64 second = addGroup("second", 0);

    EXPECT_TRUE(db_.moveLocalGroup(first, second));
    EXPECT_EQ(findLocalGroup(db_, first)->parentId(), second);

    // And back to the root, which is not a group and cannot be below anything.
    EXPECT_TRUE(db_.moveLocalGroup(first, 0));
    EXPECT_EQ(findLocalGroup(db_, first)->parentId(), 0);
}

//--------------------------------------------------------------------------------------------------
// Groups come by name whatever order they were added in, and the case of a letter does not move a
// group, Cyrillic included.
TEST_F(DatabaseTest, GroupsAreListedByName)
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
// Hosts of a group and the hosts a search found come by name the same way.
TEST_F(DatabaseTest, HostsAreListedByName)
{
    addHost("office-2", 0);
    addHost(QString::fromUtf8("Склад"), 0);
    addHost("Office-1", 0);
    addHost("backup", 0);

    const QList<LocalHostConfig> hosts = localHostList(0);
    ASSERT_EQ(hosts.size(), 4);
    EXPECT_EQ(hosts[0].name(), "backup");
    EXPECT_EQ(hosts[1].name(), "Office-1");
    EXPECT_EQ(hosts[2].name(), "office-2");
    EXPECT_EQ(hosts[3].name(), QString::fromUtf8("Склад"));

    QList<LocalHostConfig> found;
    EXPECT_EQ(db_.searchLocalHosts("office", &found), Database::ReadResult::OK);
    ASSERT_EQ(found.size(), 2);
    EXPECT_EQ(found[0].name(), "Office-1");
    EXPECT_EQ(found[1].name(), "office-2");
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseTest, SearchMatchesComment)
{
    addHost("office", 0);

    LocalHostConfig host;
    host.setName("backup");
    host.setAddress("192.168.0.2");
    host.setComment("Second Floor");
    ASSERT_TRUE(db_.addLocalHost(host));

    QList<LocalHostConfig> found;
    EXPECT_EQ(db_.searchLocalHosts("second", &found), Database::ReadResult::OK);
    ASSERT_EQ(found.size(), 1);
    EXPECT_EQ(found[0].id(), host.id());
}

//--------------------------------------------------------------------------------------------------
// The move writes the group and nothing else. A record whose sealed column did not open goes to
// another group like any other, and one that opens is not resealed on the way.
TEST_F(DatabaseTest, HostThatDoesNotOpenIsMovedLikeAnyOther)
{
    const qint64 first = addGroup("first", 0);
    const qint64 second = addGroup("second", 0);
    const qint64 readable_id = addHost("readable", first);
    const qint64 broken_id = addHost("broken", first);

    ASSERT_TRUE(corruptRecordData("local_hosts", broken_id));

    ASSERT_TRUE(db_.moveLocalHost(broken_id, second));
    ASSERT_TRUE(db_.moveLocalHost(readable_id, second));

    LocalHostConfig host;
    EXPECT_EQ(db_.findLocalHost(broken_id, &host), Database::FindResult::UNREADABLE);
    EXPECT_EQ(host.groupId(), second);
    EXPECT_EQ(host.name(), QString("broken"));

    ASSERT_EQ(db_.findLocalHost(readable_id, &host), Database::FindResult::FOUND);
    EXPECT_EQ(host.groupId(), second);
    EXPECT_EQ(host.address(), QString("192.168.0.1"));

    EXPECT_TRUE(localHostList(first).isEmpty());

    // And to the root, which is not a group of its own.
    ASSERT_TRUE(db_.moveLocalHost(readable_id, 0));
    ASSERT_EQ(db_.findLocalHost(readable_id, &host), Database::FindResult::FOUND);
    EXPECT_EQ(host.groupId(), 0);
}

//--------------------------------------------------------------------------------------------------
// The group a host lies in is part of what the list shows, so a move counts as an edit: the
// moment the record last changed is a column of that list and what it can be sorted by.
TEST_F(DatabaseTest, MoveMarksTheMomentTheRecordChanged)
{
    const qint64 first = addGroup("first", 0);
    const qint64 second = addGroup("second", 0);
    const qint64 entry_id = addHost("host", first);

    const qint64 modify_time = findLocalHost(db_, entry_id)->modifyTime();

    // The stamp counts whole seconds, so a move within the same second is indistinguishable
    // from one that left it alone.
    QThread::msleep(1100);

    ASSERT_TRUE(db_.moveLocalHost(entry_id, second));

    EXPECT_GT(findLocalHost(db_, entry_id)->modifyTime(), modify_time);
}

//--------------------------------------------------------------------------------------------------
// A move that finds no record writes nothing, and the answer must say so: the caller that is told
// the move went through leaves the record on screen in a group the database never put it in.
TEST_F(DatabaseTest, MoveOfARecordThatIsNotThereIsRefused)
{
    const qint64 group = addGroup("group", 0);
    const qint64 other = addGroup("other", 0);
    const qint64 entry_id = addHost("host", group);

    ASSERT_TRUE(db_.removeLocalHost(entry_id));
    EXPECT_FALSE(db_.moveLocalHost(entry_id, other));

    ASSERT_TRUE(db_.removeLocalGroup(group));
    EXPECT_FALSE(db_.moveLocalGroup(group, other));
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

    ASSERT_TRUE(db_.reencryptAll(allLocalHosts(), findRouter(db_), allRouterHosts(), {},
                                 ProxyConfig(), "salt", "verifier", 1));

    EXPECT_EQ(findLocalHost(db_, entry_id)->modifyTime(), modify_time);
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
TEST_F(DatabaseTest, GroupKeepsTheGuidItWasGiven)
{
    LocalGroupConfig group;
    group.setName("from-a-file");
    group.setGuid("7b0f1d18-0e3a-4a0e-9a4e-2a1c6a0f0c11");

    ASSERT_TRUE(db_.addLocalGroup(group));

    const std::optional<LocalGroupConfig> stored = findLocalGroup(db_, group.id());
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->guid(), "7b0f1d18-0e3a-4a0e-9a4e-2a1c6a0f0c11");
}

//--------------------------------------------------------------------------------------------------
// Renaming a group or moving it elsewhere in the tree leaves it the same group.
TEST_F(DatabaseTest, GroupGuidSurvivesEveryEdit)
{
    const qint64 parent = addGroup("parent", 0);
    const qint64 group = addGroup("group", 0);

    const std::optional<LocalGroupConfig> before = findLocalGroup(db_, group);
    ASSERT_TRUE(before.has_value());

    LocalGroupConfig edited = *before;
    edited.setName("renamed");
    edited.setComment("comment");
    ASSERT_TRUE(db_.modifyLocalGroup(edited));
    ASSERT_TRUE(db_.moveLocalGroup(group, parent));

    const std::optional<LocalGroupConfig> after = findLocalGroup(db_, group);
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
    addRouter();

    const std::optional<RouterConfig> before = findRouter(db_);
    ASSERT_TRUE(before.has_value());
    ASSERT_FALSE(before->guid().isEmpty());

    // What both editors of a router hand over: the fields of the form and nothing else.
    RouterConfig edited;
    edited.setAddress("moved.example.com");
    edited.setSessionType(proto::router::SESSION_TYPE_OPERATOR);
    edited.setUsername("router-user");
    edited.setPassword(SecureString("router-secret"));

    ASSERT_TRUE(db_.setRouterConfig(edited));

    const std::optional<RouterConfig> after = findRouter(db_);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->address(), "moved.example.com");
    EXPECT_EQ(after->guid(), before->guid());
}

//--------------------------------------------------------------------------------------------------
// A guid names one group, and the record naming it is the one an import looks for before it decides
// whether the group of the file is already here.
TEST_F(DatabaseTest, TwoGroupsDoNotShareOneGuid)
{
    const qint64 group = addGroup("group", 0);

    const std::optional<LocalGroupConfig> stored = findLocalGroup(db_, group);
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

    const std::optional<LocalHostConfig> stored = findLocalHost(db_, host);
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
TEST_F(DatabaseTest, DatabaseIsIdentifiedByTheNameOfItsFile)
{
    EXPECT_EQ(db_.guid(), QString("client"));

    const QString guid = "7b0f1d18-0e3a-4a0e-9a4e-2a1c6a0f0c11";

    Database other;
    ASSERT_TRUE(other.open(QFileInfo(file_path_).absolutePath() + '/' + guid + ".db3"));
    EXPECT_EQ(other.guid(), guid);
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseTest, DatabaseHasNoNameUntilItIsGiven)
{
    EXPECT_TRUE(db_.name().isEmpty());
    ASSERT_TRUE(db_.setName("Office"));

    Database reopened;
    ASSERT_TRUE(reopened.open(file_path_));
    EXPECT_EQ(reopened.name(), QString("Office"));
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
    ASSERT_TRUE(first.setName("First"));

    Database second;
    ASSERT_TRUE(second.open(dir.filePath(second_id + ".db3")));
    ASSERT_TRUE(second.setName("Second"));
    ASSERT_TRUE(second.setAutoUnlockBlob("blob"));
    ASSERT_TRUE(second.reencryptAll({}, {}, {}, {}, ProxyConfig(), "salt", "verifier", 1));

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
    ASSERT_TRUE(Database::instance().setName("First"));
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
    ASSERT_TRUE(Database::instance().setName("First"));

    Database::select("other");
    EXPECT_EQ(Database::filePath(), QFileInfo(file_path_).dir().filePath("other.db3"));
    EXPECT_TRUE(Database::instance().name().isEmpty());

    Database::select("client");
    EXPECT_EQ(Database::instance().name(), QString("First"));
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

    std::optional<LocalHostConfig> stored = findLocalHost(db_, host.id());
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
// A record that does not open is not the same as a list that could not be read: the records that
// do open come back, and the caller is told the list is not everything the database holds.
TEST_F(DatabaseTest, HostThatDoesNotOpenLeavesTheListIncomplete)
{
    const qint64 group = addGroup("group", 0);
    const qint64 broken_id = addHost("broken", group);
    addHost("readable", group);

    ASSERT_TRUE(corruptRecordData("local_hosts", broken_id));

    // The record that did not open is in the list all the same: without it the user would have
    // no way to reach the row, and the result says the list is not whole.
    QList<LocalHostConfig> hosts;
    EXPECT_EQ(db_.localHostList(group, &hosts), Database::ReadResult::INCOMPLETE);
    ASSERT_EQ(hosts.size(), 2);

    const LocalHostConfig& broken = hosts.front().id() == broken_id ? hosts.front() : hosts.back();
    const LocalHostConfig& readable = hosts.front().id() == broken_id ? hosts.back() : hosts.front();

    EXPECT_EQ(broken.name(), QString("broken"));
    EXPECT_TRUE(broken.address().isEmpty());
    EXPECT_EQ(readable.name(), QString("readable"));
    EXPECT_EQ(readable.address(), QString("192.168.0.1"));

    QList<LocalHostConfig> all_hosts;
    EXPECT_EQ(db_.allLocalHosts(&all_hosts), Database::ReadResult::INCOMPLETE);
    EXPECT_EQ(all_hosts.size(), 2);
}

//--------------------------------------------------------------------------------------------------
// The result says the list is not whole when a record of that list did not open. A record the
// search left out is not part of it.
TEST_F(DatabaseTest, HostThatDoesNotOpenLeavesTheSearchIncomplete)
{
    const qint64 group = addGroup("group", 0);
    const qint64 broken_id = addHost("office-1", group);
    addHost("office-2", group);

    ASSERT_TRUE(corruptRecordData("local_hosts", broken_id));

    // The name of a record that did not open is read, so it still matches by name.
    QList<LocalHostConfig> hosts;
    EXPECT_EQ(db_.searchLocalHosts("office", &hosts), Database::ReadResult::INCOMPLETE);

    ASSERT_EQ(hosts.size(), 2);

    QStringList names;
    for (const LocalHostConfig& host : std::as_const(hosts))
        names.append(host.name());
    names.sort();

    EXPECT_EQ(names, QStringList({ "office-1", "office-2" }));

    QList<LocalHostConfig> matched;
    EXPECT_EQ(db_.searchLocalHosts("office-2", &matched), Database::ReadResult::OK);
    ASSERT_EQ(matched.size(), 1);
    EXPECT_EQ(matched.front().name(), QString("office-2"));
}

//--------------------------------------------------------------------------------------------------
// A database holds one router, so the one written takes the place of the one there.
TEST_F(DatabaseTest, DatabaseHoldsOneRouter)
{
    addRouter();

    RouterConfig second;
    second.setAddress("second.example.com");
    second.setUsername("router-user");
    second.setPassword(SecureString("router-secret"));
    EXPECT_TRUE(db_.setRouterConfig(second));

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->address(), QString("second.example.com"));
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseTest, RouterStaysSwitchedOffUntilSwitchedOn)
{
    addRouter();

    std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->isEnabled());

    stored->setEnabled(false);
    ASSERT_TRUE(db_.setRouterConfig(*stored));

    stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->isEnabled());
    EXPECT_EQ(stored->address(), QString("router.example.com"));
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseTest, RouterHostThatDoesNotOpenLeavesTheListIncomplete)
{
    addRouterHost(1, "user-1", "secret");
    addRouterHost(2, "user-2", "secret");

    ASSERT_TRUE(corruptRouterHostData(1));

    // The row is in the list whether or not it opened, so a caller counting what the base
    // holds does not take a record that did not open for one that is not there.
    QList<RouterHostConfig> hosts;
    EXPECT_EQ(db_.allRouterHosts(&hosts), Database::ReadResult::INCOMPLETE);
    ASSERT_EQ(hosts.size(), 2);

    const RouterHostConfig& broken = hosts.front().hostId() == 1 ? hosts.front() : hosts.back();
    const RouterHostConfig& readable = hosts.front().hostId() == 1 ? hosts.back() : hosts.front();

    EXPECT_EQ(broken.hostId(), 1U);
    EXPECT_TRUE(broken.username().isEmpty());
    EXPECT_EQ(readable.username(), QString("user-2"));
}

//--------------------------------------------------------------------------------------------------
// A record that does not open must not pass for a missing one: a caller would take the host for
// one without credentials and delete the row that is still there.
TEST_F(DatabaseTest, RouterHostThatDoesNotOpenIsNotTakenForAMissingOne)
{
    addRouter();
    addRouterHost(1, "user-1", "secret");
    addRouterHost(2, "user-2", "secret");

    ASSERT_TRUE(corruptRouterHostData(1));

    RouterHostConfig broken;
    EXPECT_EQ(db_.findRouterHost(1, &broken), Database::FindResult::UNREADABLE);
    EXPECT_FALSE(db_.routerHostCredentials(1).has_value());

    RouterHostConfig readable;
    EXPECT_EQ(db_.findRouterHost(2, &readable), Database::FindResult::FOUND);
    EXPECT_EQ(readable.username(), QString("user-2"));

    RouterHostConfig missing;
    EXPECT_EQ(db_.findRouterHost(3, &missing), Database::FindResult::NOT_FOUND);
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseTest, CredentialThatDoesNotOpenLeavesTheListIncomplete)
{
    const qint64 broken_id = addCredential("office", "user", "secret");
    addCredential("home", "user", "secret");

    ASSERT_TRUE(corruptRecordData("credentials", broken_id));

    // The record that did not open is in the list all the same: without it the user would have
    // no way to reach the row, and the result says the list is not whole.
    QList<CredentialConfig> credentials;
    EXPECT_EQ(db_.credentialList(&credentials), Database::ReadResult::INCOMPLETE);
    ASSERT_EQ(credentials.size(), 2);

    const CredentialConfig& broken = credentials.front().id() == broken_id ?
        credentials.front() : credentials.back();
    const CredentialConfig& readable = credentials.front().id() == broken_id ?
        credentials.back() : credentials.front();

    EXPECT_EQ(broken.displayName(), QString("office"));
    EXPECT_TRUE(broken.username().isEmpty());
    EXPECT_EQ(readable.displayName(), QString("home"));
    EXPECT_EQ(readable.username(), QString("user"));
}

//--------------------------------------------------------------------------------------------------
// Credentials come by name whatever order they were added in, and the case of a letter does not
// move them.
TEST_F(DatabaseTest, CredentialsAreListedByName)
{
    addCredential("office", "user", "secret");
    addCredential("Home", "user", "secret");
    addCredential(QString::fromUtf8("Склад"), "user", "secret");
    addCredential("backup", "user", "secret");

    const QList<CredentialConfig> credentials = credentialList();
    ASSERT_EQ(credentials.size(), 4);
    EXPECT_EQ(credentials[0].displayName(), "backup");
    EXPECT_EQ(credentials[1].displayName(), "Home");
    EXPECT_EQ(credentials[2].displayName(), "office");
    EXPECT_EQ(credentials[3].displayName(), QString::fromUtf8("Склад"));
}

//--------------------------------------------------------------------------------------------------
// A list that was not read at all must not pass for a book without records.
TEST_F(DatabaseTest, ListOfAClosedDatabaseFails)
{
    Database closed;

    QList<LocalHostConfig> hosts;
    EXPECT_EQ(closed.allLocalHosts(&hosts), Database::ReadResult::FAILED);
    EXPECT_TRUE(hosts.isEmpty());

    QList<CredentialConfig> credentials;
    EXPECT_EQ(closed.credentialList(&credentials), Database::ReadResult::FAILED);
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
    ASSERT_EQ(db_.findLocalHost(entry_id, &host), Database::FindResult::FOUND);
    EXPECT_EQ(db_.findLocalHost(entry_id + 1000, &host), Database::FindResult::NOT_FOUND);
    EXPECT_EQ(host.id(), -1);
    EXPECT_TRUE(host.name().isEmpty());
    EXPECT_TRUE(host.guid().isEmpty());

    LocalGroupConfig group;
    ASSERT_EQ(db_.findLocalGroup(group_id, &group), Database::FindResult::FOUND);
    EXPECT_EQ(db_.findLocalGroup(group_id + 1000, &group), Database::FindResult::NOT_FOUND);
    EXPECT_EQ(group.id(), -1);
    EXPECT_TRUE(group.name().isEmpty());

    Database without_router;
    ASSERT_TRUE(without_router.open(QFileInfo(file_path_).absolutePath() + "/without_router.db3"));

    RouterConfig router;
    ASSERT_EQ(db_.routerConfig(&router), Database::FindResult::FOUND);
    EXPECT_EQ(without_router.routerConfig(&router), Database::FindResult::NOT_FOUND);
    EXPECT_TRUE(router.address().isEmpty());
    EXPECT_TRUE(router.guid().isEmpty());

    RouterHostConfig router_host;
    ASSERT_EQ(db_.findRouterHost(100500, &router_host), Database::FindResult::FOUND);
    EXPECT_EQ(db_.findRouterHost(100501, &router_host), Database::FindResult::NOT_FOUND);
    EXPECT_EQ(router_host.hostId(), kInvalidHostId);
    EXPECT_TRUE(router_host.username().isEmpty());

    const std::optional<CredentialConfig> credential_record = findCredential(db_, credential_id);
    ASSERT_TRUE(credential_record.has_value());

    CredentialConfig credential;
    ASSERT_EQ(db_.findCredential(credential_id, &credential), Database::FindResult::FOUND);
    EXPECT_EQ(db_.findCredential(credential_id + 1000, &credential), Database::FindResult::NOT_FOUND);
    EXPECT_EQ(credential.id(), -1);
    EXPECT_TRUE(credential.displayName().isEmpty());

    ASSERT_EQ(db_.findCredentialByGuid(credential_record->guid(), &credential),
              Database::FindResult::FOUND);
    EXPECT_EQ(db_.findCredentialByGuid("no-such-guid", &credential),
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
    ASSERT_EQ(db_.findLocalHost(broken_entry_id, &linked), Database::FindResult::FOUND);
    linked.setCredentialId(broken_credential_id);
    ASSERT_TRUE(db_.modifyLocalHost(linked));

    addRouterHost(100501, "user-1", "secret");

    RouterHostConfig broken_router_host = routerHost(100500, "user-2", "secret");
    broken_router_host.setCredentialId(broken_credential_id);
    ASSERT_TRUE(db_.addRouterHost(broken_router_host));

    ASSERT_TRUE(corruptRecordData("local_hosts", broken_entry_id));
    ASSERT_TRUE(corruptRecordData("credentials", broken_credential_id));
    ASSERT_TRUE(corruptRouterData());
    ASSERT_TRUE(corruptRouterHostData(100500));

    LocalHostConfig host;
    ASSERT_EQ(db_.findLocalHost(readable_entry_id, &host), Database::FindResult::FOUND);
    ASSERT_FALSE(host.address().isEmpty());

    EXPECT_EQ(db_.findLocalHost(broken_entry_id, &host), Database::FindResult::UNREADABLE);
    EXPECT_EQ(host.id(), broken_entry_id);
    EXPECT_EQ(host.name(), QString("broken"));
    EXPECT_EQ(host.groupId(), group_id);
    EXPECT_EQ(host.credentialId(), broken_credential_id);
    EXPECT_FALSE(host.guid().isEmpty());
    EXPECT_TRUE(host.address().isEmpty());
    EXPECT_TRUE(host.username().isEmpty());

    RouterHostConfig router_host;
    ASSERT_EQ(db_.findRouterHost(100501, &router_host), Database::FindResult::FOUND);
    ASSERT_FALSE(router_host.username().isEmpty());

    EXPECT_EQ(db_.findRouterHost(100500, &router_host), Database::FindResult::UNREADABLE);
    EXPECT_EQ(router_host.hostId(), 100500U);
    EXPECT_EQ(router_host.credentialId(), broken_credential_id);
    EXPECT_TRUE(router_host.username().isEmpty());

    CredentialConfig credential;
    ASSERT_EQ(db_.findCredential(readable_credential_id, &credential), Database::FindResult::FOUND);
    ASSERT_FALSE(credential.username().isEmpty());

    EXPECT_EQ(db_.findCredential(broken_credential_id, &credential), Database::FindResult::UNREADABLE);
    EXPECT_EQ(credential.id(), broken_credential_id);
    EXPECT_EQ(credential.displayName(), QString("office"));
    EXPECT_TRUE(credential.username().isEmpty());

    RouterConfig router;
    EXPECT_EQ(db_.routerConfig(&router), Database::FindResult::UNREADABLE);
    EXPECT_FALSE(router.guid().isEmpty());
    EXPECT_TRUE(router.address().isEmpty());
    EXPECT_TRUE(router.username().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A search that never ran must not pass for a search that found nothing.
TEST_F(DatabaseTest, SearchOfAClosedDatabaseFails)
{
    Database closed;

    RouterHostConfig host;
    EXPECT_EQ(closed.findRouterHost(100500, &host), Database::FindResult::FAILED);
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

    QList<LocalHostConfig> hosts = allLocalHosts();
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

    ASSERT_TRUE(db_.reencryptAll(hosts, findRouter(db_), allRouterHosts(), {},
                                 ProxyConfig(), "salt", "verifier", 1));

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
    ASSERT_TRUE(db_.setRouterConfig(*stored));

    const std::optional<RouterConfig> router = findRouter(db_);
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

    ASSERT_TRUE(db_.reencryptAll(allLocalHosts(), router, allRouterHosts(), {},
                                 ProxyConfig(), "salt", "verifier", 1));

    const std::optional<RouterConfig> reread = findRouter(db_);
    ASSERT_TRUE(reread.has_value());
    EXPECT_EQ(reread->deviceToken(), QByteArray("wrapped-elsewhere"));
}

//--------------------------------------------------------------------------------------------------
// Without a choice of the user the application is never locked.
TEST_F(DatabaseTest, LockTimeoutIsOffUntilChosen)
{
    EXPECT_EQ(db_.lockTimeout(), Minutes::zero());

    ASSERT_TRUE(db_.setLockTimeout(Minutes(5)));
    EXPECT_EQ(db_.lockTimeout(), Minutes(5));

    ASSERT_TRUE(db_.setLockTimeout(Minutes::zero()));
    EXPECT_EQ(db_.lockTimeout(), Minutes::zero());
}

//--------------------------------------------------------------------------------------------------
// The default profile comes with the database: the built-in parameters until the user changes
// them, a change is kept. It is not one of the profiles the user made, and it cannot be deleted.
TEST_F(DatabaseTest, DefaultProfileComesWithTheDatabase)
{
    ProfileConfig profile = db_.defaultProfile();
    EXPECT_GT(profile.id(), 0);
    EXPECT_TRUE(profile.isDefault());
    EXPECT_EQ(serialize(profile.desktopConfig()), serialize(defaultDesktopConfig()));
    EXPECT_EQ(profile.udpMethods(), static_cast<quint32>(UDP_METHOD_ALL));
    EXPECT_FALSE(profile.desktopConfig().record_sessions());
    EXPECT_FALSE(recordingPath(profile.desktopConfig()).isEmpty());

    EXPECT_TRUE(profileList().isEmpty());

    addProfile("Aaa");

    QList<ProfileConfig> all;
    ASSERT_EQ(db_.profileList(&all), Database::ReadResult::OK);
    ASSERT_EQ(all.size(), 2);
    EXPECT_EQ(all.front().id(), profile.id());
    EXPECT_EQ(all.back().name(), QString("Aaa"));

    proto::control::Config config = defaultDesktopConfig();
    config.set_audio(false);
    config.set_block_input(true);
    config.set_record_sessions(true);
    config.set_recording_path("/records");
    profile.setDesktopConfig(config);
    profile.setUdpMethods(0);
    ASSERT_TRUE(db_.modifyProfile(profile));

    profile = db_.defaultProfile();
    EXPECT_EQ(serialize(profile.desktopConfig()), serialize(config));
    EXPECT_EQ(recordingPath(profile.desktopConfig()), "/records");
    EXPECT_EQ(profile.udpMethods(), 0u);

    EXPECT_FALSE(db_.removeProfile(profile.id()));
    EXPECT_EQ(db_.defaultProfile().id(), profile.id());

    {
        Database reopened;
        ASSERT_TRUE(reopened.open(file_path_));
        EXPECT_EQ(reopened.defaultProfile().id(), profile.id());
        EXPECT_EQ(reopened.defaultProfile().udpMethods(), 0u);
    }
}

//--------------------------------------------------------------------------------------------------
// A default profile whose data does not read stands for the built-in parameters, and it is still
// the row it was: a save writes the row over, and the next read gives what was saved.
TEST_F(DatabaseTest, DamagedDefaultProfileIsRepairedByASave)
{
    const qint64 default_id = db_.defaultProfile().id();
    ASSERT_TRUE(corruptRecordData("profiles", default_id));

    ProfileConfig profile = db_.defaultProfile();
    EXPECT_EQ(profile.id(), default_id);
    EXPECT_TRUE(profile.isDefault());
    EXPECT_TRUE(profile.name().isEmpty());
    EXPECT_EQ(serialize(profile.desktopConfig()), serialize(defaultDesktopConfig()));
    EXPECT_EQ(profile.udpMethods(), static_cast<quint32>(UDP_METHOD_ALL));

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    ASSERT_TRUE(db_.addLocalHost(host));
    EXPECT_EQ(db_.hostProfile(HostConfig::forLocalHost(host)).id(), default_id);

    profile.setUdpMethods(UDP_METHOD_DIRECT);
    ASSERT_TRUE(db_.modifyProfile(profile));

    ProfileConfig repaired;
    ASSERT_EQ(db_.findProfile(default_id, &repaired), Database::FindResult::FOUND);
    EXPECT_EQ(repaired.udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));
    EXPECT_EQ(db_.defaultProfile().udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));
}

//--------------------------------------------------------------------------------------------------
// A batch replaces the default profile like every other record: a batch without one leaves the
// built-in parameters, a batch with one brings its own, and the hosts that name it land on it.
TEST_F(DatabaseTest, BatchReplacesTheDefaultProfile)
{
    ProfileConfig profile = db_.defaultProfile();
    profile.setUdpMethods(UDP_METHOD_UPNP);
    ASSERT_TRUE(db_.modifyProfile(profile));

    ASSERT_TRUE(db_.import({}, {}, {}, {}, {}, {}));
    EXPECT_GT(db_.defaultProfile().id(), 0);
    EXPECT_EQ(db_.defaultProfile().udpMethods(), static_cast<quint32>(UDP_METHOD_ALL));

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

    const ProfileConfig current = db_.defaultProfile();
    EXPECT_GT(current.id(), 0);
    EXPECT_EQ(current.udpMethods(), static_cast<quint32>(UDP_METHOD_PCP));

    const QList<ProfileConfig> profiles = profileList();
    ASSERT_EQ(profiles.size(), 1);
    EXPECT_EQ(profiles.front().name(), QString("office"));

    EXPECT_EQ(allLocalHosts().front().profileId(), current.id());
}

//--------------------------------------------------------------------------------------------------
// Until the user chooses one, no proxy is used; a choice is kept with all its fields.
TEST_F(DatabaseTest, ProxyIsOffUntilChosen)
{
    ProxyConfig proxy;
    ASSERT_EQ(db_.proxyConfig(&proxy), Database::ReadResult::OK);
    EXPECT_FALSE(proxy.settings().enabled);

    ProxySettings settings;
    settings.enabled = true;
    settings.type = ProxySettings::Type::SOCKS5H;
    settings.address = "proxy.example.com";
    settings.port = 1080;
    settings.user = "user";
    settings.password = SecureString("secret");

    proxy.setSettings(settings);
    ASSERT_TRUE(db_.setProxyConfig(proxy));

    ProxyConfig reread;
    ASSERT_EQ(db_.proxyConfig(&reread), Database::ReadResult::OK);
    EXPECT_TRUE(reread.settings().enabled);
    EXPECT_EQ(reread.settings().type, ProxySettings::Type::SOCKS5H);
    EXPECT_EQ(reread.settings().address, QString("proxy.example.com"));
    EXPECT_EQ(reread.settings().port, 1080);
    EXPECT_EQ(reread.settings().user, QString("user"));
    EXPECT_EQ(reread.settings().password.toString(), QString("secret"));
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
    ASSERT_TRUE(db_.setProxyConfig(proxy));

    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

    ProxyConfig unreadable;
    EXPECT_EQ(db_.proxyConfig(&unreadable), Database::ReadResult::INCOMPLETE);
    EXPECT_FALSE(unreadable.settings().enabled);
    EXPECT_TRUE(unreadable.settings().address.isEmpty());
    EXPECT_TRUE(unreadable.settings().password.isEmpty());

    ASSERT_TRUE(db_.reencryptAll(allLocalHosts(), findRouter(db_), allRouterHosts(), {},
                                 proxy, "salt", "verifier", 1));

    ProxyConfig reread;
    ASSERT_EQ(db_.proxyConfig(&reread), Database::ReadResult::OK);
    EXPECT_TRUE(reread.settings().enabled);
    EXPECT_EQ(reread.settings().type, ProxySettings::Type::HTTPS);
    EXPECT_EQ(reread.settings().address, QString("10.0.0.1"));
    EXPECT_EQ(reread.settings().port, 3128);
    EXPECT_EQ(reread.settings().password.toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// The credentials the user saved for a host of a router come back as they were saved.
TEST_F(DatabaseTest, RouterHostCredentialsSurviveARoundTrip)
{
    addRouter();

    ASSERT_TRUE(db_.addRouterHost(routerHost(100500, "user", "secret")));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());

    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// The row of a host whose credentials did not open is rewritten in place. An insert over it
// breaks on the primary key, so a caller that takes such a row for a missing one loses what the
// user entered.
TEST_F(DatabaseTest, RouterHostThatDoesNotOpenIsRewrittenInPlace)
{
    addRouter();
    addRouterHost(100500, "user", "secret");

    ASSERT_TRUE(corruptRouterHostData(100500));

    RouterHostConfig broken;
    ASSERT_EQ(db_.findRouterHost(100500, &broken), Database::FindResult::UNREADABLE);

    EXPECT_FALSE(db_.addRouterHost(routerHost(100500, "new-user", "new-secret")));
    ASSERT_TRUE(db_.modifyRouterHost(routerHost(100500, "new-user", "new-secret")));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->username(), QString("new-user"));
    EXPECT_EQ(stored->password().toString(), QString("new-secret"));
    EXPECT_EQ(allRouterHosts().size(), 1);
}

//--------------------------------------------------------------------------------------------------
// Credentials are added once and edited afterwards. Adding twice for the same host is refused,
// and removing takes the row out.
TEST_F(DatabaseTest, RouterHostCredentialsAreEditedAndRemoved)
{
    addRouter();

    addRouterHost(100500, "user", "secret");

    EXPECT_FALSE(db_.addRouterHost(routerHost(100500, "other-user", "other-secret")));
    EXPECT_TRUE(db_.modifyRouterHost(routerHost(100500, "other-user", "other-secret")));

    // A host the user never saved anything for has no row to edit.
    EXPECT_FALSE(db_.modifyRouterHost(routerHost(100501, "user", "secret")));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->username(), QString("other-user"));
    EXPECT_EQ(stored->password().toString(), QString("other-secret"));
    EXPECT_EQ(allRouterHosts().size(), 1);

    ASSERT_TRUE(db_.removeRouterHost(100500));
    EXPECT_FALSE(findRouterHost(db_, 100500).has_value());
    EXPECT_TRUE(allRouterHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Every column of the table opens with the same key, so what tells one row from another is the host
// id the column was sealed for. A column moved to the row of another host does not open.
TEST_F(DatabaseTest, RouterHostCredentialsDoNotOpenForAnotherHost)
{
    RouterHostConfig host;
    host.setHostId(100500);
    host.setUsername("user");
    host.setPassword(SecureString("secret"));

    std::optional<QByteArray> sealed = host.encryptedData();
    ASSERT_TRUE(sealed.has_value());

    RouterHostConfig other;
    other.setHostId(100501);

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
    addRouter();

    addRouterHost(100500, "user", "secret");
    addRouterHost(100501, "other-user", "other-secret");
    addRouterHost(100502, "third-user", "third-secret");

    // The router was never asked about any of them.
    QList<HostId> outdated = outdatedRouterHosts();
    EXPECT_EQ(outdated.size(), 3);
    EXPECT_TRUE(outdated.contains(100500));
    EXPECT_TRUE(outdated.contains(100501));
    EXPECT_TRUE(outdated.contains(100502));

    ASSERT_TRUE(db_.updateRouterHostCheckTime(100500));
    ASSERT_TRUE(db_.updateRouterHostCheckTime(100501));
    ASSERT_TRUE(db_.updateRouterHostCheckTime(100502));

    EXPECT_TRUE(outdatedRouterHosts().isEmpty());

    // The answer of the router is trusted for a week. A row asked about a day ago is not offered
    // again, and one asked about eight days ago is.
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 day = 24 * 60 * 60;

    ASSERT_TRUE(setRouterHostCheckTime(100501, now - day));
    ASSERT_TRUE(setRouterHostCheckTime(100502, now - 8 * day));

    EXPECT_EQ(outdatedRouterHosts(), QList<HostId>({ HostId(100502) }));

    // There is nothing to remember for a host the user saved nothing for.
    EXPECT_FALSE(db_.updateRouterHostCheckTime(100503));
}

//--------------------------------------------------------------------------------------------------
// Every offered row costs a request to the router, so a single call never takes more than a handful
// of them. What one call left behind is what the next one takes.
TEST_F(DatabaseTest, OutdatedRouterHostsAreTakenInBoundedBatches)
{
    addRouter();

    for (HostId host_id = 100500; host_id < 100512; ++host_id)
        addRouterHost(host_id, "user", "secret");

    // Twelve rows are waiting, and no answer carries more than ten.
    const QList<HostId> first = outdatedRouterHosts();
    EXPECT_EQ(first.size(), 10);

    for (HostId host_id : first)
        ASSERT_TRUE(db_.updateRouterHostCheckTime(host_id));

    const QList<HostId> second = outdatedRouterHosts();
    EXPECT_EQ(second.size(), 2);

    for (HostId host_id : second)
    {
        EXPECT_FALSE(first.contains(host_id));
        ASSERT_TRUE(db_.updateRouterHostCheckTime(host_id));
    }

    EXPECT_TRUE(outdatedRouterHosts().isEmpty());
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

    ASSERT_TRUE(db_.updateRouterHostCheckTime(100500));
    ASSERT_EQ(outdatedRouterHosts(), QList<HostId>({ HostId(100501) }));

    ASSERT_TRUE(db_.modifyRouterHost(routerHost(100500, "edited-user", "edited-secret")));
    ASSERT_TRUE(db_.modifyRouterHost(routerHost(100501, "third-user", "third-secret")));

    EXPECT_EQ(outdatedRouterHosts(), QList<HostId>({ HostId(100501) }));

    QList<RouterHostConfig> router_hosts = allRouterHosts();
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

    ASSERT_TRUE(db_.reencryptAll(allLocalHosts(), std::nullopt, router_hosts, {},
                                 ProxyConfig(), "salt", "verifier", 1));

    EXPECT_EQ(outdatedRouterHosts(), QList<HostId>({ HostId(100501) }));
}

//--------------------------------------------------------------------------------------------------
// Changing the master password reseals the credentials of router hosts as well. Left out, they would
// stay under the old key and never open again.
TEST_F(DatabaseTest, ReencryptionKeepsRouterHostCredentialsReadable)
{
    addRouter();
    addRouterHost(100500, "user", "secret");

    QList<RouterHostConfig> router_hosts = allRouterHosts();
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

    ASSERT_TRUE(db_.reencryptAll(allLocalHosts(), std::nullopt, router_hosts, {},
                                 ProxyConfig(), "salt", "verifier", 1));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
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
    router.setAddress("router.example.com");
    router.setUsername("router-user");

    EXPECT_FALSE(db_.setRouterConfig(router));
    EXPECT_FALSE(findRouter(db_).has_value());
}

//--------------------------------------------------------------------------------------------------
// A write over the router there cannot leave behind one without a password.
TEST_F(DatabaseTest, RouterCannotBeEditedIntoOneWithoutAPassword)
{
    addRouter();

    RouterConfig edited;
    edited.setAddress("moved.example.com");
    edited.setUsername("router-user");

    EXPECT_FALSE(db_.setRouterConfig(edited));

    const std::optional<RouterConfig> after = findRouter(db_);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->address(), "router.example.com");
    EXPECT_FALSE(after->password().isEmpty());
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
// A database without a router is given one by the same write that edits a router already there.
TEST_F(DatabaseTest, RouterIsWrittenWhereNoneIsSet)
{
    ASSERT_FALSE(findRouter(db_).has_value());

    RouterConfig router;
    router.setAddress("router.example.com");
    router.setSessionType(proto::router::SESSION_TYPE_OPERATOR);
    router.setUsername("router-user");
    router.setPassword(SecureString("router-secret"));

    EXPECT_TRUE(db_.setRouterConfig(router));

    const std::optional<RouterConfig> stored = findRouter(db_);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->address(), QString("router.example.com"));
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

    EXPECT_TRUE(allLocalGroups().isEmpty());
    EXPECT_TRUE(allLocalHosts().isEmpty());
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
    EXPECT_TRUE(allLocalHosts().isEmpty());

    host.setPassword(SecureString(QString("secret")));
    ASSERT_TRUE(db_.addLocalHost(host));

    LocalHostConfig edited = host;
    edited.setPassword(SecureString());
    EXPECT_FALSE(db_.modifyLocalHost(edited));

    const std::optional<LocalHostConfig> after = findLocalHost(db_, host.id());
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->password().toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// A temporary id is handed out again to another host once the one holding it is gone, so what was
// saved under it would be sent to a host the user never gave it to.
TEST_F(DatabaseTest, CredentialsOfATemporaryHostAreNotStored)
{
    RouterHostConfig credentials;
    credentials.setHostId(kMinTempHostId);
    credentials.setUsername("user");
    credentials.setPassword(SecureString(QString("secret")));

    EXPECT_FALSE(db_.addRouterHost(credentials));
    EXPECT_FALSE(db_.modifyRouterHost(credentials));
    EXPECT_TRUE(allRouterHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Credentials are a pair here as well. The row exists to answer a host that asks, and half an
// answer is no answer.
TEST_F(DatabaseTest, CredentialsWithoutAPasswordAreNotStored)
{
    RouterHostConfig credentials;
    credentials.setHostId(100500);
    credentials.setUsername("user");

    EXPECT_FALSE(db_.addRouterHost(credentials));
    EXPECT_TRUE(allRouterHosts().isEmpty());
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
// A credential is named by its guid everywhere it is referred to, so every one gets a guid the
// moment it is added, and no two share one.
TEST_F(DatabaseTest, CredentialGetsAGuidOfItsOwn)
{
    const qint64 first = addCredential("first", "user", "secret");
    const qint64 second = addCredential("second", "user", "secret");

    const std::optional<CredentialConfig> stored = findCredential(db_, first);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->guid().isEmpty());

    const std::optional<CredentialConfig> other = findCredential(db_, second);
    ASSERT_TRUE(other.has_value());
    EXPECT_NE(stored->guid(), other->guid());

    CredentialConfig copy = credential("copy", "user", "secret");
    copy.setGuid(stored->guid());
    EXPECT_FALSE(db_.addCredential(copy));

    EXPECT_EQ(credentialList().size(), 2);
}

//--------------------------------------------------------------------------------------------------
// The pair goes to the database as one sealed column. What comes back out has to be what went in.
TEST_F(DatabaseTest, CredentialSurvivesARoundTrip)
{
    const qint64 credential_id = addCredential("office", "user", "secret");

    const std::optional<CredentialConfig> stored = findCredential(db_, credential_id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->id(), credential_id);
    EXPECT_EQ(stored->type(), CredentialConfig::Type::ASPIA);
    EXPECT_EQ(stored->displayName(), QString("office"));
    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));

    // The guid names the same record wherever it is referred to.
    const std::optional<CredentialConfig> by_guid = findCredentialByGuid(db_, stored->guid());
    ASSERT_TRUE(by_guid.has_value());
    EXPECT_EQ(by_guid->id(), credential_id);
}

//--------------------------------------------------------------------------------------------------
TEST_F(DatabaseTest, OsCredentialKeepsItsType)
{
    CredentialConfig added = credential("workstation", "DOMAIN\\user", "secret");
    added.setType(CredentialConfig::Type::OS);
    ASSERT_TRUE(db_.addCredential(added));

    std::optional<CredentialConfig> stored = findCredential(db_, added.id());
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->type(), CredentialConfig::Type::OS);
    EXPECT_EQ(stored->username(), QString("DOMAIN\\user"));

    CredentialConfig edited = credential("renamed", "DOMAIN\\other-user", "other-secret");
    edited.setId(added.id());
    edited.setType(CredentialConfig::Type::OS);
    EXPECT_TRUE(db_.modifyCredential(edited));

    stored = findCredential(db_, added.id());
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->type(), CredentialConfig::Type::OS);
    EXPECT_EQ(stored->displayName(), QString("renamed"));
    EXPECT_EQ(stored->username(), QString("DOMAIN\\other-user"));

    const QList<CredentialConfig> credentials = credentialList();
    ASSERT_EQ(credentials.size(), 1);
    EXPECT_EQ(credentials[0].type(), CredentialConfig::Type::OS);
}

//--------------------------------------------------------------------------------------------------
// A list asked for one type holds only the records of that type, still by name.
TEST_F(DatabaseTest, CredentialsAreListedByType)
{
    addCredential("office", "user", "secret");

    CredentialConfig os = credential("workstation", "DOMAIN\\user", "secret");
    os.setType(CredentialConfig::Type::OS);
    ASSERT_TRUE(db_.addCredential(os));

    addCredential("backup", "user", "secret");

    QList<CredentialConfig> credentials;
    EXPECT_EQ(db_.credentialList(&credentials, CredentialConfig::Type::ASPIA), Database::ReadResult::OK);
    ASSERT_EQ(credentials.size(), 2);
    EXPECT_EQ(credentials[0].displayName(), "backup");
    EXPECT_EQ(credentials[1].displayName(), "office");

    EXPECT_EQ(db_.credentialList(&credentials, CredentialConfig::Type::OS), Database::ReadResult::OK);
    ASSERT_EQ(credentials.size(), 1);
    EXPECT_EQ(credentials[0].displayName(), "workstation");

    EXPECT_EQ(credentialList().size(), 3);
}

//--------------------------------------------------------------------------------------------------
// An edit changes the name and the pair and keeps the guid, since the records referring to it would
// stop resolving otherwise. A record that was never added has no row to edit, and removing takes
// the row out.
TEST_F(DatabaseTest, CredentialIsEditedAndRemoved)
{
    const qint64 credential_id = addCredential("office", "user", "secret");

    const std::optional<CredentialConfig> before = findCredential(db_, credential_id);
    ASSERT_TRUE(before.has_value());

    // What the editor hands over: the fields of the form and the id, the guid is what the record
    // already has.
    CredentialConfig edited = credential("renamed", "other-user", "other-secret");
    edited.setId(credential_id);
    edited.setGuid(before->guid());
    EXPECT_TRUE(db_.modifyCredential(edited));

    CredentialConfig unknown = credential("unknown", "user", "secret");
    unknown.setId(credential_id + 1);
    EXPECT_FALSE(db_.modifyCredential(unknown));

    const std::optional<CredentialConfig> stored = findCredential(db_, credential_id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->guid(), before->guid());
    EXPECT_EQ(stored->displayName(), QString("renamed"));
    EXPECT_EQ(stored->username(), QString("other-user"));
    EXPECT_EQ(stored->password().toString(), QString("other-secret"));
    EXPECT_EQ(credentialList().size(), 1);

    ASSERT_TRUE(db_.removeCredential(credential_id));
    EXPECT_FALSE(findCredential(db_, credential_id).has_value());
    EXPECT_TRUE(credentialList().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The guid names the record from the moment it is added and is what its column is sealed for. An
// edit changes neither: whatever guid the editor hands over, or none, the column is sealed for the
// guid of the row, and the record opens after the edit as it did before.
TEST_F(DatabaseTest, EditedCredentialStaysSealedForItsOwnGuid)
{
    const qint64 credential_id = addCredential("office", "user", "secret");

    const std::optional<CredentialConfig> before = findCredential(db_, credential_id);
    ASSERT_TRUE(before.has_value());

    CredentialConfig edited = credential("renamed", "other-user", "other-secret");
    edited.setId(credential_id);
    EXPECT_TRUE(db_.modifyCredential(edited));

    std::optional<CredentialConfig> stored = findCredential(db_, credential_id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->guid(), before->guid());
    EXPECT_EQ(stored->username(), QString("other-user"));

    edited.setGuid("not-the-guid-of-the-row");
    EXPECT_TRUE(db_.modifyCredential(edited));

    stored = findCredential(db_, credential_id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->guid(), before->guid());
    EXPECT_EQ(stored->username(), QString("other-user"));
}

//--------------------------------------------------------------------------------------------------
// Every column of the table opens with the same key, so a column is sealed for its guid and does
// not open under another one.
TEST_F(DatabaseTest, CredentialDoesNotOpenForAnotherGuid)
{
    CredentialConfig config = credential("office", "user", "secret");
    config.setGuid("first");

    std::optional<QByteArray> sealed = config.encryptedData();
    ASSERT_TRUE(sealed.has_value());

    CredentialConfig other;
    other.setGuid("second");

    EXPECT_FALSE(other.setEncryptedData(*sealed));
    EXPECT_TRUE(other.username().isEmpty());
    EXPECT_TRUE(other.password().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A credential is a name and a whole pair. A record missing any of the three is not stored.
TEST_F(DatabaseTest, CredentialWithoutANameOrHalfItsPairIsNotStored)
{
    CredentialConfig nameless = credential(QString(), "user", "secret");
    EXPECT_FALSE(db_.addCredential(nameless));

    CredentialConfig without_password = credential("office", "user", QString());
    EXPECT_FALSE(db_.addCredential(without_password));

    CredentialConfig without_user = credential("office", QString(), "secret");
    EXPECT_FALSE(db_.addCredential(without_user));

    EXPECT_TRUE(credentialList().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// What comes back out of a profile has to be what went in, and every profile gets a guid of its own.
TEST_F(DatabaseTest, ProfileSurvivesARoundTrip)
{
    proto::control::Config desktop_config = defaultDesktopConfig();
    desktop_config.set_audio(false);
    desktop_config.set_quality(kHighQuality);
    desktop_config.set_record_sessions(true);
    desktop_config.set_recording_path("/records");

    ProfileConfig profile;
    profile.setName("office");
    profile.setDesktopConfig(desktop_config);
    profile.setUdpMethods(0);
    ASSERT_TRUE(db_.addProfile(profile));
    EXPECT_GT(profile.id(), 0);
    EXPECT_FALSE(profile.guid().isEmpty());

    ProfileConfig stored;
    ASSERT_EQ(db_.findProfile(profile.id(), &stored), Database::FindResult::FOUND);
    EXPECT_EQ(stored.id(), profile.id());
    EXPECT_EQ(stored.guid(), profile.guid());
    EXPECT_EQ(stored.name(), QString("office"));
    EXPECT_EQ(serialize(stored.desktopConfig()), serialize(desktop_config));
    EXPECT_EQ(stored.udpMethods(), 0u);

    const qint64 other_id = addProfile("home");
    ProfileConfig other;
    ASSERT_EQ(db_.findProfile(other_id, &other), Database::FindResult::FOUND);
    EXPECT_NE(other.guid(), stored.guid());

    ProfileConfig copy = stored;
    EXPECT_FALSE(db_.addProfile(copy));

    ProfileConfig missing;
    EXPECT_EQ(db_.findProfile(other_id + 1, &missing), Database::FindResult::NOT_FOUND);
}

//--------------------------------------------------------------------------------------------------
// An edit changes the name and the parameters, never the guid. A removed profile is gone.
TEST_F(DatabaseTest, ProfileIsEditedAndRemoved)
{
    const qint64 profile_id = addProfile("office");

    ProfileConfig before;
    ASSERT_EQ(db_.findProfile(profile_id, &before), Database::FindResult::FOUND);

    proto::control::Config desktop_config = defaultDesktopConfig();
    desktop_config.set_disable_key_combinations(true);

    ProfileConfig edited;
    edited.setId(profile_id);
    edited.setName("renamed");
    edited.setDesktopConfig(desktop_config);
    edited.setUdpMethods(UDP_METHOD_DIRECT);
    EXPECT_TRUE(db_.modifyProfile(edited));

    ProfileConfig unknown = edited;
    unknown.setId(profile_id + 1);
    EXPECT_FALSE(db_.modifyProfile(unknown));

    ProfileConfig stored;
    ASSERT_EQ(db_.findProfile(profile_id, &stored), Database::FindResult::FOUND);
    EXPECT_EQ(stored.guid(), before.guid());
    EXPECT_EQ(stored.name(), QString("renamed"));
    EXPECT_EQ(serialize(stored.desktopConfig()), serialize(desktop_config));
    EXPECT_EQ(stored.udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));
    EXPECT_EQ(profileList().size(), 1);

    ASSERT_TRUE(db_.removeProfile(profile_id));
    EXPECT_EQ(db_.findProfile(profile_id, &stored), Database::FindResult::NOT_FOUND);
    EXPECT_TRUE(profileList().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Profiles come by name whatever order they were added in, and the case of a letter does not move
// them.
TEST_F(DatabaseTest, ProfilesAreListedByName)
{
    addProfile("office");
    addProfile("Home");
    addProfile(QString::fromUtf8("Склад"));
    addProfile("backup");

    const QList<ProfileConfig> profiles = profileList();
    ASSERT_EQ(profiles.size(), 4);
    EXPECT_EQ(profiles[0].name(), "backup");
    EXPECT_EQ(profiles[1].name(), "Home");
    EXPECT_EQ(profiles[2].name(), "office");
    EXPECT_EQ(profiles[3].name(), QString::fromUtf8("Склад"));
}

//--------------------------------------------------------------------------------------------------
// A profile is chosen by its name, so one without a name or with one too long is not stored.
TEST_F(DatabaseTest, ProfileWithoutANameIsNotStored)
{
    ProfileConfig nameless;
    EXPECT_FALSE(db_.addProfile(nameless));

    ProfileConfig long_name;
    long_name.setName(QString(ProfileConfig::kMaxNameLength + 1, QChar('a')));
    EXPECT_FALSE(db_.addProfile(long_name));

    EXPECT_TRUE(profileList().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A profile whose parameters do not read is in the list all the same, and the list says it is not
// whole.
TEST_F(DatabaseTest, ProfileThatDoesNotReadLeavesTheListIncomplete)
{
    const qint64 broken_id = addProfile("office");
    addProfile("home");

    ASSERT_TRUE(corruptRecordData("profiles", broken_id));

    QList<ProfileConfig> profiles;
    EXPECT_EQ(db_.profileList(&profiles), Database::ReadResult::INCOMPLETE);
    ASSERT_EQ(profiles.size(), 3);
    EXPECT_EQ(profiles[2].name(), QString("office"));
    EXPECT_EQ(profiles[2].id(), broken_id);

    ProfileConfig broken;
    EXPECT_EQ(db_.findProfile(broken_id, &broken), Database::FindResult::UNREADABLE);
}

//--------------------------------------------------------------------------------------------------
// A local host is connected with the profile chosen for it, and an edit of the host is what changes
// the choice. Once the profile is removed the host is back on the default one.
TEST_F(DatabaseTest, LocalHostKeepsTheProfileChosenForIt)
{
    const qint64 office_id = addProfile("office");
    const qint64 home_id = addProfile("home");

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setProfileId(office_id);
    ASSERT_TRUE(db_.addLocalHost(host));
    EXPECT_EQ(findLocalHost(db_, host.id())->profileId(), office_id);
    EXPECT_EQ(allLocalHosts().front().profileId(), office_id);

    host.setProfileId(home_id);
    ASSERT_TRUE(db_.modifyLocalHost(host));
    EXPECT_EQ(findLocalHost(db_, host.id())->profileId(), home_id);

    ASSERT_TRUE(db_.removeProfile(home_id));
    EXPECT_EQ(findLocalHost(db_, host.id())->profileId(), 0);

    host.setProfileId(office_id);
    ASSERT_TRUE(db_.modifyLocalHost(host));
    host.setProfileId(0);
    ASSERT_TRUE(db_.modifyLocalHost(host));
    EXPECT_EQ(findLocalHost(db_, host.id())->profileId(), 0);
}

//--------------------------------------------------------------------------------------------------
// A profile is kept for a host of a router whether or not credentials are saved for it, and the
// credentials come and go without touching it.
TEST_F(DatabaseTest, RouterHostKeepsTheProfileChosenForIt)
{
    addRouter();
    const qint64 profile_id = addProfile("office");

    ASSERT_TRUE(db_.setRouterHostProfileId(100500, profile_id));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->profileId(), profile_id);
    EXPECT_TRUE(stored->username().isEmpty());
    EXPECT_EQ(stored->credentialId(), 0);

    std::optional<std::pair<QString, SecureString>> credentials =
        db_.routerHostCredentials(100500);
    ASSERT_TRUE(credentials.has_value());
    EXPECT_TRUE(credentials->first.isEmpty());
    EXPECT_TRUE(credentials->second.isEmpty());

    ASSERT_TRUE(db_.modifyRouterHost(routerHost(100500, "user", "secret")));
    stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->profileId(), profile_id);
    EXPECT_EQ(stored->username(), QString("user"));

    ASSERT_TRUE(db_.modifyRouterHost(routerHost(100500, QString(), QString())));
    stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->profileId(), profile_id);
    EXPECT_TRUE(stored->username().isEmpty());

    ASSERT_TRUE(db_.setRouterHostProfileId(100500, 0));
    EXPECT_FALSE(findRouterHost(db_, 100500).has_value());

    ASSERT_TRUE(db_.setRouterHostProfileId(100501, 0));
    EXPECT_FALSE(findRouterHost(db_, 100501).has_value());

    EXPECT_FALSE(db_.setRouterHostProfileId(kMinTempHostId, profile_id));
    EXPECT_TRUE(allRouterHosts().isEmpty());
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
    ASSERT_TRUE(db_.addCredential(os));

    RouterHostConfig host = routerHost(100500, "own-user", "own-secret");
    host.setCredentialId(credential_id);
    ASSERT_TRUE(db_.addRouterHost(host));
    ASSERT_TRUE(db_.setRouterHostOsCredentialId(100500, os.id()));

    ASSERT_TRUE(db_.modifyRouterHost(routerHost(100500, QString(), QString())));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->username().isEmpty());
    EXPECT_TRUE(stored->password().isEmpty());
    EXPECT_EQ(stored->credentialId(), 0);
    EXPECT_EQ(stored->osCredentialId(), os.id());

    EXPECT_TRUE(db_.reencryptAll(allLocalHosts(), findRouter(db_), allRouterHosts(), credentialList(),
                                 ProxyConfig(), QByteArray(32, 's'), QByteArray(32, 'v'), 1));

    EXPECT_FALSE(db_.modifyRouterHost(routerHost(100501, QString(), QString())));
    EXPECT_EQ(allRouterHosts().size(), 1);

    ASSERT_TRUE(db_.removeRouterHost(100500));
    EXPECT_TRUE(allRouterHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A row that is left with no pair, no links and no profile is gone, whichever of them was the last
// to go. A row that still holds a pair stays when its links go.
TEST_F(DatabaseTest, EmptiedRouterHostRowIsGone)
{
    addRouter();

    CredentialConfig os = credential("workstation", "DOMAIN\\user", "secret");
    os.setType(CredentialConfig::Type::OS);
    ASSERT_TRUE(db_.addCredential(os));

    addRouterHost(100500, "user", "secret");
    ASSERT_TRUE(db_.modifyRouterHost(routerHost(100500, QString(), QString())));
    EXPECT_FALSE(findRouterHost(db_, 100500).has_value());

    addRouterHost(100501, "user", "secret");
    ASSERT_TRUE(db_.setRouterHostOsCredentialId(100501, os.id()));
    ASSERT_TRUE(db_.modifyRouterHost(routerHost(100501, QString(), QString())));
    ASSERT_TRUE(findRouterHost(db_, 100501).has_value());

    ASSERT_TRUE(db_.setRouterHostOsCredentialId(100501, 0));
    EXPECT_FALSE(findRouterHost(db_, 100501).has_value());

    addRouterHost(100502, "user", "secret");
    ASSERT_TRUE(db_.setRouterHostOsCredentialId(100502, os.id()));
    ASSERT_TRUE(db_.removeCredential(os.id()));

    const std::optional<RouterHostConfig> stored = findRouterHost(db_, 100502);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->osCredentialId(), 0);
    EXPECT_EQ(stored->username(), QString("user"));
}

//--------------------------------------------------------------------------------------------------
// A removed profile leaves every host that had it on the default profile. The row of a router host
// that held nothing but the profile is gone with it.
TEST_F(DatabaseTest, RemovedProfileLeavesItsHostsOnTheDefaultOne)
{
    addRouter();
    const qint64 profile_id = addProfile("office");

    ASSERT_TRUE(db_.setRouterHostProfileId(100500, profile_id));

    LocalHostConfig local_host;
    local_host.setName("host");
    local_host.setAddress("192.168.0.1");
    local_host.setProfileId(profile_id);
    ASSERT_TRUE(db_.addLocalHost(local_host));

    ASSERT_TRUE(db_.removeProfile(profile_id));

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
    ASSERT_TRUE(db_.addProfile(office));

    proto::control::Config default_desktop = defaultDesktopConfig();
    default_desktop.set_clipboard(false);
    ProfileConfig default_profile = db_.defaultProfile();
    default_profile.setDesktopConfig(default_desktop);
    default_profile.setUdpMethods(UDP_METHOD_UPNP);
    ASSERT_TRUE(db_.modifyProfile(default_profile));

    LocalHostConfig local_host;
    local_host.setName("host");
    local_host.setAddress("192.168.0.1");
    local_host.setProfileId(office.id());
    ASSERT_TRUE(db_.addLocalHost(local_host));

    ProfileConfig profile = db_.hostProfile(HostConfig::forLocalHost(local_host));
    EXPECT_EQ(profile.id(), office.id());
    EXPECT_EQ(serialize(profile.desktopConfig()), serialize(office_desktop));
    EXPECT_EQ(profile.udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));

    addRouter();
    ASSERT_TRUE(db_.setRouterHostProfileId(100500, office.id()));

    profile = db_.hostProfile(HostConfig::forRouterHost(100500, "host"));
    EXPECT_EQ(profile.id(), office.id());
    EXPECT_EQ(profile.udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));

    profile = db_.hostProfile(HostConfig::forRouterHost(100501, "other"));
    EXPECT_EQ(profile.id(), default_profile.id());
    EXPECT_EQ(serialize(profile.desktopConfig()), serialize(default_desktop));
    EXPECT_EQ(profile.udpMethods(), static_cast<quint32>(UDP_METHOD_UPNP));

    HostConfig quick_connect;
    quick_connect.setAddress("192.168.0.2");
    profile = db_.hostProfile(quick_connect);
    EXPECT_EQ(profile.id(), default_profile.id());
    EXPECT_EQ(profile.udpMethods(), static_cast<quint32>(UDP_METHOD_UPNP));

    ASSERT_TRUE(db_.removeProfile(office.id()));

    profile = db_.hostProfile(HostConfig::forLocalHost(local_host));
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

    QList<CredentialConfig> credentials = credentialList();
    DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

    ASSERT_TRUE(db_.reencryptAll(allLocalHosts(), std::nullopt, allRouterHosts(),
                                 credentials, ProxyConfig(), "salt", "verifier", 1));

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
// A host names the credentials it is entered with. The link is kept through an edit and comes back
// with the record. A record of credentials that is removed leaves the host without one, and a host
// that is removed leaves the record alone.
TEST_F(DatabaseTest, LocalHostLinksItsCredentials)
{
    const qint64 group = addGroup("group", 0);
    const qint64 credential_id = addCredential("office", "user", "secret");

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setGroupId(group);
    host.setCredentialId(credential_id);
    ASSERT_TRUE(db_.addLocalHost(host));

    std::optional<LocalHostConfig> stored = findLocalHost(db_, host.id());
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->credentialId(), credential_id);

    stored->setName("renamed");
    ASSERT_TRUE(db_.modifyLocalHost(*stored));
    EXPECT_EQ(findLocalHost(db_, host.id())->credentialId(), credential_id);

    ASSERT_TRUE(db_.removeLocalHost(host.id()));
    EXPECT_TRUE(findCredential(db_, credential_id).has_value());

    LocalHostConfig other;
    other.setName("other");
    other.setAddress("192.168.0.2");
    other.setGroupId(group);
    other.setCredentialId(credential_id);
    ASSERT_TRUE(db_.addLocalHost(other));

    ASSERT_TRUE(db_.removeCredential(credential_id));
    EXPECT_EQ(findLocalHost(db_, other.id())->credentialId(), 0);
}

//--------------------------------------------------------------------------------------------------
// The row of a router host links credentials the same way.
TEST_F(DatabaseTest, RouterHostLinksItsCredentials)
{
    addRouter();
    const qint64 credential_id = addCredential("office", "user", "secret");

    RouterHostConfig host = routerHost(100500, "user", "secret");
    host.setCredentialId(credential_id);
    ASSERT_TRUE(db_.addRouterHost(host));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->credentialId(), credential_id);

    ASSERT_TRUE(db_.removeCredential(credential_id));
    EXPECT_EQ(findRouterHost(db_, 100500)->credentialId(), 0);
}

//--------------------------------------------------------------------------------------------------
// A host of a router remembered by the record of credentials it refers to comes back with the link
// and an empty pair of its own.
TEST_F(DatabaseTest, RouterHostIsKeptByItsLinkAlone)
{
    addRouter();
    const qint64 credential_id = addCredential("office", "user", "secret");

    RouterHostConfig host = routerHost(100500, QString(), QString());
    host.setCredentialId(credential_id);
    ASSERT_TRUE(db_.addRouterHost(host));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->credentialId(), credential_id);
    EXPECT_TRUE(stored->username().isEmpty());
    EXPECT_TRUE(stored->password().isEmpty());
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
    ASSERT_TRUE(db_.addCredential(os));

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    ASSERT_TRUE(db_.addLocalHost(host));
    EXPECT_EQ(findLocalHost(db_, host.id())->osCredentialId(), 0);

    ASSERT_TRUE(db_.setLocalHostOsCredentialId(host.id(), os.id()));
    EXPECT_EQ(findLocalHost(db_, host.id())->osCredentialId(), os.id());

    LocalHostConfig edited;
    edited.setId(host.id());
    edited.setName("renamed");
    edited.setAddress("192.168.0.1");
    ASSERT_TRUE(db_.modifyLocalHost(edited));
    EXPECT_EQ(findLocalHost(db_, host.id())->osCredentialId(), os.id());

    addRouter();
    ASSERT_TRUE(db_.addRouterHost(routerHost(100500, "user", "secret")));

    ASSERT_TRUE(db_.setRouterHostOsCredentialId(100500, os.id()));
    ASSERT_TRUE(db_.modifyRouterHost(routerHost(100500, "other-user", "other-secret")));
    EXPECT_EQ(findRouterHost(db_, 100500)->osCredentialId(), os.id());

    ASSERT_TRUE(db_.setRouterHostOsCredentialId(100501, os.id()));
    EXPECT_EQ(allRouterHosts().size(), 1);

    ASSERT_TRUE(db_.removeCredential(os.id()));
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
    ASSERT_EQ(db_.credentialList(&credentials, CredentialConfig::Type::OS), Database::ReadResult::OK);
    ASSERT_EQ(credentials.size(), 1);

    const QList<LocalHostConfig> hosts = allLocalHosts();
    ASSERT_EQ(hosts.size(), 1);
    EXPECT_EQ(hosts.front().osCredentialId(), credentials.front().id());

    const QList<RouterHostConfig> router_hosts = allRouterHosts();
    ASSERT_EQ(router_hosts.size(), 1);
    EXPECT_EQ(router_hosts.front().osCredentialId(), credentials.front().id());
}

//--------------------------------------------------------------------------------------------------
// A host is entered with the pair of the record of credentials it refers to, whatever pair of its
// own it keeps, or with its own pair when it refers to none. Once the record is removed the host
// refers to none and is entered with its own pair again.
TEST_F(DatabaseTest, LocalHostIsEnteredWithTheCredentialsItRefersToOrItsOwnPair)
{
    const qint64 group = addGroup("group", 0);
    const qint64 credential_id = addCredential("office", "shared-user", "shared-secret");

    LocalHostConfig own;
    own.setName("own");
    own.setAddress("192.168.0.1");
    own.setGroupId(group);
    own.setUsername("user");
    own.setPassword(SecureString("secret"));
    ASSERT_TRUE(db_.addLocalHost(own));

    LocalHostConfig linked;
    linked.setName("linked");
    linked.setAddress("192.168.0.2");
    linked.setGroupId(group);
    linked.setUsername("user");
    linked.setPassword(SecureString("secret"));
    linked.setCredentialId(credential_id);
    ASSERT_TRUE(db_.addLocalHost(linked));

    LocalHostConfig bare;
    bare.setName("bare");
    bare.setAddress("192.168.0.3");
    bare.setGroupId(group);
    ASSERT_TRUE(db_.addLocalHost(bare));

    std::optional<std::pair<QString, SecureString>> credentials = db_.localHostCredentials(own.id());
    ASSERT_TRUE(credentials.has_value());
    EXPECT_EQ(credentials->first, QString("user"));
    EXPECT_EQ(credentials->second.toString(), QString("secret"));

    credentials = db_.localHostCredentials(linked.id());
    ASSERT_TRUE(credentials.has_value());
    EXPECT_EQ(credentials->first, QString("shared-user"));
    EXPECT_EQ(credentials->second.toString(), QString("shared-secret"));

    credentials = db_.localHostCredentials(bare.id());
    ASSERT_TRUE(credentials.has_value());
    EXPECT_TRUE(credentials->first.isEmpty());
    EXPECT_TRUE(credentials->second.isEmpty());

    EXPECT_FALSE(db_.localHostCredentials(bare.id() + 1).has_value());

    ASSERT_TRUE(db_.removeCredential(credential_id));

    credentials = db_.localHostCredentials(linked.id());
    ASSERT_TRUE(credentials.has_value());
    EXPECT_EQ(credentials->first, QString("user"));
    EXPECT_EQ(credentials->second.toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// The same for a host of a router. A host without a row is entered with nothing.
TEST_F(DatabaseTest, RouterHostIsEnteredWithTheCredentialsItRefersToOrItsOwnPair)
{
    addRouter();
    const qint64 credential_id = addCredential("office", "shared-user", "shared-secret");

    ASSERT_TRUE(db_.addRouterHost(routerHost(100500, "user", "secret")));

    RouterHostConfig linked = routerHost(100501, "user", "secret");
    linked.setCredentialId(credential_id);
    ASSERT_TRUE(db_.addRouterHost(linked));

    std::optional<std::pair<QString, SecureString>> credentials =
        db_.routerHostCredentials(100500);
    ASSERT_TRUE(credentials.has_value());
    EXPECT_EQ(credentials->first, QString("user"));
    EXPECT_EQ(credentials->second.toString(), QString("secret"));

    credentials = db_.routerHostCredentials(100501);
    ASSERT_TRUE(credentials.has_value());
    EXPECT_EQ(credentials->first, QString("shared-user"));
    EXPECT_EQ(credentials->second.toString(), QString("shared-secret"));

    EXPECT_FALSE(db_.routerHostCredentials(100502).has_value());
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
    ASSERT_TRUE(db_.addRouterHost(link_only));

    RouterHostConfig with_pair = routerHost(100501, "own-user", "own-secret");
    with_pair.setCredentialId(credential_id);
    ASSERT_TRUE(db_.addRouterHost(with_pair));

    RouterHostConfig other_link = routerHost(100502, QString(), QString());
    other_link.setCredentialId(other_id);
    ASSERT_TRUE(db_.addRouterHost(other_link));

    LocalHostConfig local_host;
    local_host.setName("host");
    local_host.setAddress("192.168.0.1");
    local_host.setGroupId(group);
    local_host.setCredentialId(credential_id);
    ASSERT_TRUE(db_.addLocalHost(local_host));

    ASSERT_TRUE(db_.removeCredential(credential_id));

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

    EXPECT_TRUE(db_.reencryptAll(allLocalHosts(), findRouter(db_), allRouterHosts(), credentialList(),
                                 ProxyConfig(), QByteArray(32, 's'), QByteArray(32, 'v'), 1));
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
    ASSERT_TRUE(db.setRouterConfig(router));

    CredentialConfig credential;
    credential.setDisplayName("office");
    credential.setUsername("user");
    credential.setPassword(SecureString("secret"));
    ASSERT_TRUE(db.addCredential(credential));

    RouterHostConfig host = routerHost(100500, "own-user", "own-secret");
    host.setCredentialId(credential.id());
    ASSERT_TRUE(db.addRouterHost(host));

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

    ASSERT_TRUE(db.setAutoUnlockBlob(Random::byteArray(64)));
    EXPECT_FALSE(MasterPassword::autoUnlock());
    EXPECT_TRUE(MasterPassword::isAutoUnlockEnabled());

    QByteArray blob;
    ASSERT_TRUE(OSCrypt::encryptBytes(Random::byteArray(32), &blob));
    ASSERT_TRUE(db.setAutoUnlockBlob(blob));
    EXPECT_FALSE(MasterPassword::autoUnlock());
    EXPECT_FALSE(MasterPassword::isAutoUnlockEnabled());

    // Closes the file before the directory goes.
    DatabaseTestPeer::setFilePath(file_path);
}
