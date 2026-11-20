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

#include "client/storage/local_host_store.h"

#include <QThread>

#include "client/storage/database_test_base.h"

using LocalHostStoreTest = DatabaseTestBase;

//--------------------------------------------------------------------------------------------------
// Hosts of a group and the hosts a search found come by name the same way.
TEST_F(LocalHostStoreTest, HostsAreListedByName)
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
    EXPECT_EQ(local_hosts_.search("office", &found), Database::ReadResult::OK);
    ASSERT_EQ(found.size(), 2);
    EXPECT_EQ(found[0].name(), "Office-1");
    EXPECT_EQ(found[1].name(), "office-2");
}

//--------------------------------------------------------------------------------------------------
TEST_F(LocalHostStoreTest, SearchMatchesComment)
{
    addHost("office", 0);

    LocalHostConfig host;
    host.setName("backup");
    host.setAddress("192.168.0.2");
    host.setComment("Second Floor");
    ASSERT_TRUE(local_hosts_.add(host));

    QList<LocalHostConfig> found;
    EXPECT_EQ(local_hosts_.search("second", &found), Database::ReadResult::OK);
    ASSERT_EQ(found.size(), 1);
    EXPECT_EQ(found[0].id(), host.id());
}

//--------------------------------------------------------------------------------------------------
// The move writes the group and nothing else. A record whose sealed column did not open goes to
// another group like any other, and one that opens is not resealed on the way.
TEST_F(LocalHostStoreTest, HostThatDoesNotOpenIsMovedLikeAnyOther)
{
    const qint64 first = addGroup("first", 0);
    const qint64 second = addGroup("second", 0);
    const qint64 readable_id = addHost("readable", first);
    const qint64 broken_id = addHost("broken", first);

    ASSERT_TRUE(corruptRecordData("local_hosts", broken_id));

    ASSERT_TRUE(local_hosts_.move(broken_id, second));
    ASSERT_TRUE(local_hosts_.move(readable_id, second));

    LocalHostConfig host;
    EXPECT_EQ(local_hosts_.find(broken_id, &host), Database::FindResult::UNREADABLE);
    EXPECT_EQ(host.groupId(), second);
    EXPECT_EQ(host.name(), QString("broken"));

    ASSERT_EQ(local_hosts_.find(readable_id, &host), Database::FindResult::FOUND);
    EXPECT_EQ(host.groupId(), second);
    EXPECT_EQ(host.address(), QString("192.168.0.1"));

    EXPECT_TRUE(localHostList(first).isEmpty());

    // And to the root, which is not a group of its own.
    ASSERT_TRUE(local_hosts_.move(readable_id, 0));
    ASSERT_EQ(local_hosts_.find(readable_id, &host), Database::FindResult::FOUND);
    EXPECT_EQ(host.groupId(), 0);
}

//--------------------------------------------------------------------------------------------------
// The group a host lies in is part of what the list shows, so a move counts as an edit: the
// moment the record last changed is a column of that list and what it can be sorted by.
TEST_F(LocalHostStoreTest, MoveMarksTheMomentTheRecordChanged)
{
    const qint64 first = addGroup("first", 0);
    const qint64 second = addGroup("second", 0);
    const qint64 entry_id = addHost("host", first);

    const qint64 modify_time = findLocalHost(db_, entry_id)->modifyTime();

    // The stamp counts whole seconds, so a move within the same second is indistinguishable
    // from one that left it alone.
    QThread::msleep(1100);

    ASSERT_TRUE(local_hosts_.move(entry_id, second));

    EXPECT_GT(findLocalHost(db_, entry_id)->modifyTime(), modify_time);
}

//--------------------------------------------------------------------------------------------------
// A guid names one host, and the deep links the user hands out are built on it.
TEST_F(LocalHostStoreTest, TwoLocalHostsDoNotShareOneGuid)
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

    EXPECT_FALSE(local_hosts_.add(copy));
    EXPECT_EQ(hostNamesOfGroup(group), QStringList({ "host" }));
}

//--------------------------------------------------------------------------------------------------
// The secret part of a record goes to the database as one encrypted column. What comes back out has
// to be what went in, every field of it.
TEST_F(LocalHostStoreTest, EncryptedDataSurvivesARoundTrip)
{
    const qint64 group = addGroup("group", 0);

    LocalHostConfig host;
    host.setName("host");
    host.setGroupId(group);
    host.setAddress("192.168.0.1");
    host.setUsername("user");
    host.setPassword(SecureString("secret"));
    ASSERT_TRUE(local_hosts_.add(host));

    std::optional<LocalHostConfig> stored = findLocalHost(db_, host.id());
    ASSERT_TRUE(stored.has_value());

    EXPECT_EQ(stored->address(), QString("192.168.0.1"));
    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// A record that does not open is not the same as a list that could not be read: the records that
// do open come back, and the caller is told the list is not everything the database holds.
TEST_F(LocalHostStoreTest, HostThatDoesNotOpenLeavesTheListIncomplete)
{
    const qint64 group = addGroup("group", 0);
    const qint64 broken_id = addHost("broken", group);
    addHost("readable", group);

    ASSERT_TRUE(corruptRecordData("local_hosts", broken_id));

    // The record that did not open is in the list all the same: without it the user would have
    // no way to reach the row, and the result says the list is not whole.
    QList<LocalHostConfig> hosts;
    EXPECT_EQ(local_hosts_.list(group, &hosts), Database::ReadResult::INCOMPLETE);
    ASSERT_EQ(hosts.size(), 2);

    const LocalHostConfig& broken = hosts.front().id() == broken_id ? hosts.front() : hosts.back();
    const LocalHostConfig& readable = hosts.front().id() == broken_id ? hosts.back() : hosts.front();

    EXPECT_EQ(broken.name(), QString("broken"));
    EXPECT_TRUE(broken.address().isEmpty());
    EXPECT_EQ(readable.name(), QString("readable"));
    EXPECT_EQ(readable.address(), QString("192.168.0.1"));

    QList<LocalHostConfig> all_hosts;
    EXPECT_EQ(local_hosts_.all(&all_hosts), Database::ReadResult::INCOMPLETE);
    EXPECT_EQ(all_hosts.size(), 2);
}

//--------------------------------------------------------------------------------------------------
// The result says the list is not whole when a record of that list did not open. A record the
// search left out is not part of it.
TEST_F(LocalHostStoreTest, HostThatDoesNotOpenLeavesTheSearchIncomplete)
{
    const qint64 group = addGroup("group", 0);
    const qint64 broken_id = addHost("office-1", group);
    addHost("office-2", group);

    ASSERT_TRUE(corruptRecordData("local_hosts", broken_id));

    // The name of a record that did not open is read, so it still matches by name.
    QList<LocalHostConfig> hosts;
    EXPECT_EQ(local_hosts_.search("office", &hosts), Database::ReadResult::INCOMPLETE);

    ASSERT_EQ(hosts.size(), 2);

    QStringList names;
    for (const LocalHostConfig& host : std::as_const(hosts))
        names.append(host.name());
    names.sort();

    EXPECT_EQ(names, QStringList({ "office-1", "office-2" }));

    QList<LocalHostConfig> matched;
    EXPECT_EQ(local_hosts_.search("office-2", &matched), Database::ReadResult::OK);
    ASSERT_EQ(matched.size(), 1);
    EXPECT_EQ(matched.front().name(), QString("office-2"));
}

//--------------------------------------------------------------------------------------------------
// The credentials of a host are kept as a pair. An empty pair means the host is asked for them at
// every connection, and half a pair says neither.
TEST_F(LocalHostStoreTest, HostWithHalfItsCredentialsIsNotStored)
{
    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setUsername("user");

    EXPECT_FALSE(local_hosts_.add(host));
    EXPECT_TRUE(allLocalHosts().isEmpty());

    host.setPassword(SecureString(QString("secret")));
    ASSERT_TRUE(local_hosts_.add(host));

    LocalHostConfig edited = host;
    edited.setPassword(SecureString());
    EXPECT_FALSE(local_hosts_.modify(edited));

    const std::optional<LocalHostConfig> after = findLocalHost(db_, host.id());
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->password().toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// A local host is connected with the profile chosen for it, and an edit of the host is what changes
// the choice. Once the profile is removed the host is back on the default one.
TEST_F(LocalHostStoreTest, LocalHostKeepsTheProfileChosenForIt)
{
    const qint64 office_id = addProfile("office");
    const qint64 home_id = addProfile("home");

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setProfileId(office_id);
    ASSERT_TRUE(local_hosts_.add(host));
    EXPECT_EQ(findLocalHost(db_, host.id())->profileId(), office_id);
    EXPECT_EQ(allLocalHosts().front().profileId(), office_id);

    host.setProfileId(home_id);
    ASSERT_TRUE(local_hosts_.modify(host));
    EXPECT_EQ(findLocalHost(db_, host.id())->profileId(), home_id);

    ASSERT_TRUE(profiles_.remove(home_id));
    EXPECT_EQ(findLocalHost(db_, host.id())->profileId(), 0);

    host.setProfileId(office_id);
    ASSERT_TRUE(local_hosts_.modify(host));
    host.setProfileId(0);
    ASSERT_TRUE(local_hosts_.modify(host));
    EXPECT_EQ(findLocalHost(db_, host.id())->profileId(), 0);
}

//--------------------------------------------------------------------------------------------------
// A host names the credentials it is entered with. The link is kept through an edit and comes back
// with the record. A record of credentials that is removed leaves the host without one, and a host
// that is removed leaves the record alone.
TEST_F(LocalHostStoreTest, LocalHostLinksItsCredentials)
{
    const qint64 group = addGroup("group", 0);
    const qint64 credential_id = addCredential("office", "user", "secret");

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setGroupId(group);
    host.setCredentialId(credential_id);
    ASSERT_TRUE(local_hosts_.add(host));

    std::optional<LocalHostConfig> stored = findLocalHost(db_, host.id());
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->credentialId(), credential_id);

    stored->setName("renamed");
    ASSERT_TRUE(local_hosts_.modify(*stored));
    EXPECT_EQ(findLocalHost(db_, host.id())->credentialId(), credential_id);

    ASSERT_TRUE(local_hosts_.remove(host.id()));
    EXPECT_TRUE(findCredential(db_, credential_id).has_value());

    LocalHostConfig other;
    other.setName("other");
    other.setAddress("192.168.0.2");
    other.setGroupId(group);
    other.setCredentialId(credential_id);
    ASSERT_TRUE(local_hosts_.add(other));

    ASSERT_TRUE(credentials_.remove(credential_id));
    EXPECT_EQ(findLocalHost(db_, other.id())->credentialId(), 0);
}

//--------------------------------------------------------------------------------------------------
// A host is entered with the pair of the record of credentials it refers to, whatever pair of its
// own it keeps, or with its own pair when it refers to none. Once the record is removed the host
// refers to none and is entered with its own pair again.
TEST_F(LocalHostStoreTest, LocalHostIsEnteredWithTheCredentialsItRefersToOrItsOwnPair)
{
    const qint64 group = addGroup("group", 0);
    const qint64 credential_id = addCredential("office", "shared-user", "shared-secret");

    LocalHostConfig own;
    own.setName("own");
    own.setAddress("192.168.0.1");
    own.setGroupId(group);
    own.setUsername("user");
    own.setPassword(SecureString("secret"));
    ASSERT_TRUE(local_hosts_.add(own));

    LocalHostConfig linked;
    linked.setName("linked");
    linked.setAddress("192.168.0.2");
    linked.setGroupId(group);
    linked.setUsername("user");
    linked.setPassword(SecureString("secret"));
    linked.setCredentialId(credential_id);
    ASSERT_TRUE(local_hosts_.add(linked));

    LocalHostConfig bare;
    bare.setName("bare");
    bare.setAddress("192.168.0.3");
    bare.setGroupId(group);
    ASSERT_TRUE(local_hosts_.add(bare));

    std::optional<std::pair<QString, SecureString>> credentials = local_hosts_.credentials(own.id());
    ASSERT_TRUE(credentials.has_value());
    EXPECT_EQ(credentials->first, QString("user"));
    EXPECT_EQ(credentials->second.toString(), QString("secret"));

    credentials = local_hosts_.credentials(linked.id());
    ASSERT_TRUE(credentials.has_value());
    EXPECT_EQ(credentials->first, QString("shared-user"));
    EXPECT_EQ(credentials->second.toString(), QString("shared-secret"));

    credentials = local_hosts_.credentials(bare.id());
    ASSERT_TRUE(credentials.has_value());
    EXPECT_TRUE(credentials->first.isEmpty());
    EXPECT_TRUE(credentials->second.isEmpty());

    EXPECT_FALSE(local_hosts_.credentials(bare.id() + 1).has_value());

    ASSERT_TRUE(credentials_.remove(credential_id));

    credentials = local_hosts_.credentials(linked.id());
    ASSERT_TRUE(credentials.has_value());
    EXPECT_EQ(credentials->first, QString("user"));
    EXPECT_EQ(credentials->second.toString(), QString("secret"));
}
