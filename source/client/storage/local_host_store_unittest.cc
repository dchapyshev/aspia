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

namespace {

const char kHostGuid[] = "11111111-1111-1111-1111-111111111111";
const char kGroupGuid[] = "22222222-2222-2222-2222-222222222222";
const char kParentGuid[] = "33333333-3333-3333-3333-333333333333";

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

//--------------------------------------------------------------------------------------------------
LocalGroupConfig receivedGroup(const QString& guid, const QString& name, qint64 parent_id)
{
    LocalGroupConfig group;
    group.setGuid(guid);
    group.setName(name);
    group.setParentId(parent_id);
    return group;
}

} // namespace

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
    EXPECT_EQ(local_hosts_.all(LocalHostStore::Scope::VISIBLE, &all_hosts), Database::ReadResult::INCOMPLETE);
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

//--------------------------------------------------------------------------------------------------
// A host the user has just added is one the router does not hold yet.
TEST_F(LocalHostStoreTest, NewHostIsToBeSent)
{
    const qint64 entry_id = addHost("host", 0);

    EXPECT_EQ(syncDirty("local_hosts", entry_id), 1);
}

//--------------------------------------------------------------------------------------------------
// Every change the user makes to a host is counted, so that the answer of the router to an earlier
// change is not taken for the answer to a later one.
TEST_F(LocalHostStoreTest, ChangeOfAHostIsCounted)
{
    const qint64 group = addGroup("group", 0);
    const qint64 credential_id = addCredential("office", "shared-user", "shared-secret");
    const qint64 entry_id = addHost("host", 0);

    ASSERT_TRUE(markSynced("local_hosts", entry_id, 7));

    std::optional<LocalHostConfig> host = findLocalHost(db_, entry_id);
    ASSERT_TRUE(host.has_value());

    host->setName("renamed");
    ASSERT_TRUE(local_hosts_.modify(*host));
    EXPECT_EQ(syncDirty("local_hosts", entry_id), 1);

    ASSERT_TRUE(local_hosts_.move(entry_id, group));
    EXPECT_EQ(syncDirty("local_hosts", entry_id), 2);

    ASSERT_TRUE(local_hosts_.setOsCredentialId(entry_id, credential_id));
    EXPECT_EQ(syncDirty("local_hosts", entry_id), 3);
}

//--------------------------------------------------------------------------------------------------
// The moment of the last connection is kept by the device alone, so a connection is not a change
// to send.
TEST_F(LocalHostStoreTest, ConnectionDoesNotCountAsAChange)
{
    const qint64 entry_id = addHost("host", 0);

    ASSERT_TRUE(markSynced("local_hosts", entry_id, 7));
    ASSERT_TRUE(local_hosts_.setConnectTime(entry_id, 1700000000));

    const std::optional<LocalHostConfig> host = findLocalHost(db_, entry_id);
    ASSERT_TRUE(host.has_value());
    EXPECT_EQ(host->connectTime(), 1700000000);

    EXPECT_EQ(syncDirty("local_hosts", entry_id), 0);
}

//--------------------------------------------------------------------------------------------------
// The router has never held the host, so there is no removal to tell it of.
TEST_F(LocalHostStoreTest, RemovedHostThatWasNeverSentLeavesNoRow)
{
    const qint64 entry_id = addHost("host", 0);

    ASSERT_TRUE(local_hosts_.remove(entry_id));

    EXPECT_EQ(rowCount("local_hosts"), 0);
}

//--------------------------------------------------------------------------------------------------
// The router holds the host, so its row stays as a mark of the removal until the router is told.
// The mark keeps the guid and nothing of what the host held.
TEST_F(LocalHostStoreTest, RemovedHostThatWasSentLeavesAMark)
{
    const qint64 group = addGroup("group", 0);
    const qint64 credential_id = addCredential("office", "shared-user", "shared-secret");

    LocalHostConfig host;
    host.setName("host");
    host.setAddress("192.168.0.1");
    host.setComment("comment");
    host.setGroupId(group);
    host.setCredentialId(credential_id);
    ASSERT_TRUE(local_hosts_.add(host));

    ASSERT_TRUE(local_hosts_.setOsCredentialId(host.id(), credential_id));
    ASSERT_TRUE(markSynced("local_hosts", host.id(), 7));

    ASSERT_TRUE(local_hosts_.remove(host.id()));

    EXPECT_TRUE(isMarkedDeleted("local_hosts", host.id()));
    EXPECT_EQ(syncDirty("local_hosts", host.id()), 1);

    SqlDatabase raw;
    ASSERT_TRUE(raw.open(file_path_));

    SqlQuery query(raw, "SELECT COUNT(*) FROM local_hosts WHERE id=? AND guid=? AND group_id IS NULL AND "
                        "comment='' AND credential_id IS NULL AND os_credential_id IS NULL AND "
                        "profile_id IS NULL");
    query.addInt64(host.id());
    query.addText(host.guid());
    ASSERT_EQ(query.next(), SqlQuery::StepResult::ROW);
    EXPECT_EQ(query.columnInt64(0), 1);
}

//--------------------------------------------------------------------------------------------------
// The mark of a removed host is not a host: it is not listed, not found, and nothing changes it.
TEST_F(LocalHostStoreTest, MarkOfARemovedHostIsNotShownAndNotChanged)
{
    const qint64 group = addGroup("group", 0);
    const qint64 entry_id = addHost("host", 0);

    const std::optional<LocalHostConfig> host = findLocalHost(db_, entry_id);
    ASSERT_TRUE(host.has_value());

    ASSERT_TRUE(markSynced("local_hosts", entry_id, 7));
    ASSERT_TRUE(local_hosts_.remove(entry_id));

    EXPECT_TRUE(localHostList(0).isEmpty());
    EXPECT_TRUE(allLocalHosts().isEmpty());

    QList<LocalHostConfig> found;
    EXPECT_EQ(local_hosts_.search(QString(), &found), Database::ReadResult::OK);
    EXPECT_TRUE(found.isEmpty());

    EXPECT_FALSE(findLocalHost(db_, entry_id).has_value());
    EXPECT_FALSE(local_hosts_.credentials(entry_id).has_value());

    LocalHostConfig changed = *host;
    changed.setName("renamed");
    EXPECT_FALSE(local_hosts_.modify(changed));

    EXPECT_FALSE(local_hosts_.move(entry_id, group));

    local_hosts_.setOsCredentialId(entry_id, 0);
    local_hosts_.setConnectTime(entry_id, 1700000000);
    local_hosts_.remove(entry_id);

    EXPECT_TRUE(isMarkedDeleted("local_hosts", entry_id));
    EXPECT_EQ(syncDirty("local_hosts", entry_id), 1);

    SqlDatabase raw;
    ASSERT_TRUE(raw.open(file_path_));

    SqlQuery query(raw, "SELECT COUNT(*) FROM local_hosts WHERE id=? AND group_id IS NULL AND connect_time=0");
    query.addInt64(entry_id);
    ASSERT_EQ(query.next(), SqlQuery::StepResult::ROW);
    EXPECT_EQ(query.columnInt64(0), 1);
}

//--------------------------------------------------------------------------------------------------
// The fields a newer version put into a host stay with it through an edit and are sent back.
TEST_F(LocalHostStoreTest, UnknownFieldsOfAHostSurviveItsEdit)
{
    attachSyncStorage();

    LocalHostConfig received = receivedHost(kHostGuid, "host", 0);
    received.setUnknownData("unknown");
    ASSERT_TRUE(storeReceivedLocalHost(received, 7));

    const QList<LocalHostConfig> hosts = localHostList(0);
    ASSERT_EQ(hosts.size(), 1);

    LocalHostConfig edited;
    edited.setId(hosts[0].id());
    edited.setName("renamed");
    edited.setAddress("192.168.0.2");
    ASSERT_TRUE(local_hosts_.modify(edited));

    const QList<LocalHostStore::Unsynced> unsynced = unsyncedLocalHosts();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].host.name(), "renamed");
    EXPECT_EQ(unsynced[0].host.address(), "192.168.0.2");
    EXPECT_EQ(unsynced[0].host.unknownData(), QByteArray("unknown"));
    EXPECT_EQ(unsynced[0].sync_revision, 7);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);
}

//--------------------------------------------------------------------------------------------------
// An edit of a host that is no longer there is refused, so the one who made it is told.
TEST_F(LocalHostStoreTest, EditOfAHostThatIsGoneIsRefused)
{
    const qint64 entry_id = addHost("host", 0);

    std::optional<LocalHostConfig> host = findLocalHost(db_, entry_id);
    ASSERT_TRUE(host.has_value());

    ASSERT_TRUE(local_hosts_.remove(entry_id));

    host->setName("renamed");
    EXPECT_FALSE(local_hosts_.modify(*host));
    EXPECT_EQ(rowCount("local_hosts"), 0);
}

//--------------------------------------------------------------------------------------------------
// A host whose data does not open is listed among the unsynced ones as damaged.
TEST_F(LocalHostStoreTest, UnsyncedHostThatDoesNotOpenIsDamaged)
{
    const qint64 broken_id = addHost("office", 0);
    addHost("home", 0);
    ASSERT_TRUE(corruptRecordData("local_hosts", broken_id));

    QList<LocalHostStore::Unsynced> unsynced;
    ASSERT_EQ(local_hosts_.unsynced(&unsynced), Database::ReadResult::INCOMPLETE);
    ASSERT_EQ(unsynced.size(), 2);

    for (const LocalHostStore::Unsynced& item : std::as_const(unsynced))
        EXPECT_EQ(item.damaged, item.host.id() == broken_id);
}

//--------------------------------------------------------------------------------------------------
// A received host is what the router holds, so it keeps the moments it came with and is not sent.
TEST_F(LocalHostStoreTest, ReceivedHostIsStoredAsTheRouterHoldsIt)
{
    attachSyncStorage();

    const qint64 group = addGroup("group", 0);
    const qint64 credential_id = addCredential("office", "user", "secret");
    const qint64 os_credential_id = addCredential("system", "admin", "secret");
    const qint64 profile_id = addProfile("office");

    LocalHostConfig received = receivedHost(kHostGuid, "host", group);
    received.setComment("comment");
    received.setCredentialId(credential_id);
    received.setOsCredentialId(os_credential_id);
    received.setProfileId(profile_id);
    received.setCreateTime(500);
    received.setModifyTime(1000);
    ASSERT_TRUE(storeReceivedLocalHost(received, 7));

    const QList<LocalHostConfig> hosts = localHostList(group);
    ASSERT_EQ(hosts.size(), 1);
    EXPECT_EQ(hosts[0].name(), "host");
    EXPECT_EQ(hosts[0].comment(), "comment");
    EXPECT_EQ(hosts[0].address(), "192.168.0.1");
    EXPECT_EQ(hosts[0].guid(), kHostGuid);
    EXPECT_EQ(hosts[0].credentialId(), credential_id);
    EXPECT_EQ(hosts[0].osCredentialId(), os_credential_id);
    EXPECT_EQ(hosts[0].profileId(), profile_id);
    EXPECT_EQ(hosts[0].createTime(), 500);
    EXPECT_EQ(hosts[0].modifyTime(), 1000);
    EXPECT_EQ(hosts[0].connectTime(), 0);
    EXPECT_TRUE(unsyncedLocalHosts().isEmpty());

    ASSERT_TRUE(local_hosts_.move(hosts[0].id(), 0));

    const QList<LocalHostStore::Unsynced> unsynced = unsyncedLocalHosts();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].sync_revision, 7);
}

//--------------------------------------------------------------------------------------------------
// A newer revision replaces what the row of its guid holds. The revision the row already has
// changes nothing, so an edit made since stays.
TEST_F(LocalHostStoreTest, ReceivedHostTakesTheRowOfItsGuid)
{
    attachSyncStorage();

    const qint64 entry_id = addHost("host", 0);
    const QString guid = findLocalHost(db_, entry_id)->guid();
    ASSERT_TRUE(markSynced("local_hosts", entry_id, 7));

    ASSERT_TRUE(storeReceivedLocalHost(receivedHost(guid, "renamed", 0), 8));

    EXPECT_EQ(rowCount("local_hosts"), 1);
    EXPECT_EQ(findLocalHost(db_, entry_id)->name(), "renamed");
    EXPECT_EQ(syncDirty("local_hosts", entry_id), 0);

    LocalHostConfig edited = *findLocalHost(db_, entry_id);
    edited.setName("edited");
    ASSERT_TRUE(local_hosts_.modify(edited));

    ASSERT_TRUE(storeReceivedLocalHost(receivedHost(guid, "renamed", 0), 8));

    EXPECT_EQ(findLocalHost(db_, entry_id)->name(), "edited");
    EXPECT_EQ(syncDirty("local_hosts", entry_id), 1);
}

//--------------------------------------------------------------------------------------------------
// The moment of the last connection belongs to the device and is not taken from a received host.
TEST_F(LocalHostStoreTest, ReceivedHostKeepsTheMomentOfItsLastConnection)
{
    attachSyncStorage();

    const qint64 entry_id = addHost("host", 0);
    const QString guid = findLocalHost(db_, entry_id)->guid();
    ASSERT_TRUE(markSynced("local_hosts", entry_id, 7));
    ASSERT_TRUE(local_hosts_.setConnectTime(entry_id, 1700000000));

    ASSERT_TRUE(storeReceivedLocalHost(receivedHost(guid, "renamed", 0), 8));

    const std::optional<LocalHostConfig> stored = findLocalHost(db_, entry_id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->name(), "renamed");
    EXPECT_EQ(stored->connectTime(), 1700000000);
}

//--------------------------------------------------------------------------------------------------
// A change of a host that comes while its removal is not sent yet brings the host back as the
// router holds it.
TEST_F(LocalHostStoreTest, ReceivedHostBringsBackTheOneRemovedOnTheDevice)
{
    attachSyncStorage();

    const qint64 entry_id = addHost("host", 0);
    const QString guid = findLocalHost(db_, entry_id)->guid();
    ASSERT_TRUE(markSynced("local_hosts", entry_id, 7));

    ASSERT_TRUE(local_hosts_.remove(entry_id));
    ASSERT_TRUE(isMarkedDeleted("local_hosts", entry_id));

    ASSERT_TRUE(storeReceivedLocalHost(receivedHost(guid, "renamed", 0), 8));

    const std::optional<LocalHostConfig> stored = findLocalHost(db_, entry_id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->name(), "renamed");
    EXPECT_TRUE(unsyncedLocalHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A group removed on the device is removed with what lies in it, a host another device has put
// there meanwhile included. The router holds that host, so it leaves a mark.
TEST_F(LocalHostStoreTest, HostReceivedIntoARemovedGroupIsRemovedToo)
{
    attachSyncStorage();

    const qint64 group = addGroup("group", 0);
    ASSERT_TRUE(markSynced("local_groups", group, 7));
    ASSERT_TRUE(local_groups_.remove(group));

    ASSERT_TRUE(storeReceivedLocalHost(receivedHost(kHostGuid, "host", group), 8));

    EXPECT_TRUE(allLocalHosts().isEmpty());

    const QList<LocalHostStore::Unsynced> unsynced = unsyncedLocalHosts();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_TRUE(unsynced[0].deleted);
    EXPECT_EQ(unsynced[0].host.guid(), kHostGuid);
    EXPECT_EQ(unsynced[0].sync_revision, 8);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);
    EXPECT_TRUE(isMarkedDeleted("local_hosts", unsynced[0].host.id()));
}

//--------------------------------------------------------------------------------------------------
// A host may come before the group it lies in. It is stored under the row kept for that group, is
// in no list and is not found until the group comes, and is in the group after that.
TEST_F(LocalHostStoreTest, HostReceivedIntoAnEmptyRowOfAGroupWaitsThere)
{
    attachSyncStorage();

    qint64 row_id = 0;
    ASSERT_TRUE(local_groups_.addEmptyRow(kGroupGuid, &row_id));

    ASSERT_TRUE(storeReceivedLocalHost(receivedHost(kHostGuid, "host", row_id), 7));

    EXPECT_TRUE(localHostList(0).isEmpty());
    EXPECT_TRUE(localHostList(row_id).isEmpty());
    EXPECT_TRUE(allLocalHosts().isEmpty());
    EXPECT_TRUE(unsyncedLocalHosts().isEmpty());

    QList<LocalHostConfig> stored;
    EXPECT_EQ(local_hosts_.all(LocalHostStore::Scope::WITH_WAITING, &stored), Database::ReadResult::OK);
    EXPECT_EQ(stored.size(), 1);

    QList<LocalHostConfig> found;
    EXPECT_EQ(local_hosts_.search("host", &found), Database::ReadResult::OK);
    EXPECT_TRUE(found.isEmpty());

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(kGroupGuid, "group", 0), 8));

    EXPECT_EQ(hostNamesOfGroup(row_id), QStringList({ "host" }));
    EXPECT_EQ(allLocalHosts().size(), 1);

    EXPECT_EQ(local_hosts_.search("host", &found), Database::ReadResult::OK);
    EXPECT_EQ(found.size(), 1);
}

//--------------------------------------------------------------------------------------------------
// The same for a host whose group has come but waits for a group of its own.
TEST_F(LocalHostStoreTest, HostReceivedIntoAWaitingGroupWaitsToo)
{
    attachSyncStorage();

    qint64 parent = 0;
    ASSERT_TRUE(local_groups_.addEmptyRow(kParentGuid, &parent));

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(kGroupGuid, "group", parent), 7));

    qint64 group = 0;
    ASSERT_EQ(local_groups_.findRowIdByGuid(kGroupGuid, &group), Database::FindResult::FOUND);

    ASSERT_TRUE(storeReceivedLocalHost(receivedHost(kHostGuid, "host", group), 8));

    EXPECT_TRUE(localHostList(group).isEmpty());
    EXPECT_TRUE(allLocalHosts().isEmpty());

    ASSERT_TRUE(storeReceivedGroup(receivedGroup(kParentGuid, "parent", 0), 9));

    EXPECT_EQ(hostNamesOfGroup(group), QStringList({ "host" }));
}

//--------------------------------------------------------------------------------------------------
// A host the router moved into a group that has not come leaves the lists, and a host moved out
// of such a group is back in them.
TEST_F(LocalHostStoreTest, ReceivedMoveOfAHostFollowsTheStateOfItsGroup)
{
    attachSyncStorage();

    ASSERT_TRUE(storeReceivedLocalHost(receivedHost(kHostGuid, "host", 0), 7));
    ASSERT_EQ(hostNamesOfGroup(0), QStringList({ "host" }));

    qint64 row_id = 0;
    ASSERT_TRUE(local_groups_.addEmptyRow(kGroupGuid, &row_id));

    ASSERT_TRUE(storeReceivedLocalHost(receivedHost(kHostGuid, "host", row_id), 8));

    EXPECT_TRUE(allLocalHosts().isEmpty());

    ASSERT_TRUE(storeReceivedLocalHost(receivedHost(kHostGuid, "host", 0), 9));

    EXPECT_EQ(hostNamesOfGroup(0), QStringList({ "host" }));
}
