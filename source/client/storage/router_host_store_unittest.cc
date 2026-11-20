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

#include "client/storage/router_host_store.h"

#include <QDateTime>

#include "client/storage/database_test_base.h"

using RouterHostStoreTest = DatabaseTestBase;

//--------------------------------------------------------------------------------------------------
// The same for what is kept for a host of the router, whichever of its parts is written.
TEST_F(RouterHostStoreTest, EditMarksTheMomentARouterHostChanged)
{
    const qint64 start_time = QDateTime::currentSecsSinceEpoch();

    const HostId host_id = 100500;
    const qint64 credential_id = addCredential("name", "user", "secret");
    const qint64 profile_id = addProfile("fast");

    addRouterHost(host_id, "user", "secret");
    EXPECT_GE(modifyTime("router_hosts", "host_id", host_id), start_time);

    ASSERT_TRUE(setModifyTime("router_hosts", "host_id", host_id, 1));
    ASSERT_TRUE(router_hosts_.modify(routerHost(host_id, "user", "other-secret")));
    EXPECT_GE(modifyTime("router_hosts", "host_id", host_id), start_time);

    ASSERT_TRUE(setModifyTime("router_hosts", "host_id", host_id, 1));
    ASSERT_TRUE(router_hosts_.setOsCredentialId(host_id, credential_id));
    EXPECT_GE(modifyTime("router_hosts", "host_id", host_id), start_time);

    ASSERT_TRUE(setModifyTime("router_hosts", "host_id", host_id, 1));
    ASSERT_TRUE(router_hosts_.setProfileId(host_id, profile_id));
    EXPECT_GE(modifyTime("router_hosts", "host_id", host_id), start_time);

    ASSERT_TRUE(setModifyTime("router_hosts", "host_id", host_id, 1));
    ASSERT_TRUE(router_hosts_.setProfileId(host_id, 0));
    EXPECT_GE(modifyTime("router_hosts", "host_id", host_id), start_time);

    ASSERT_TRUE(router_hosts_.setProfileId(100600, profile_id));
    EXPECT_GE(modifyTime("router_hosts", "host_id", 100600), start_time);
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterHostStoreTest, RouterHostThatDoesNotOpenLeavesTheListIncomplete)
{
    addRouterHost(1, "user-1", "secret");
    addRouterHost(2, "user-2", "secret");

    ASSERT_TRUE(corruptRouterHostData(1));

    // The row is in the list whether or not it opened, so a caller counting what the base
    // holds does not take a record that did not open for one that is not there.
    QList<RouterHostConfig> hosts;
    EXPECT_EQ(router_hosts_.list(&hosts), Database::ReadResult::INCOMPLETE);
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
TEST_F(RouterHostStoreTest, RouterHostThatDoesNotOpenIsNotTakenForAMissingOne)
{
    addRouter();
    addRouterHost(1, "user-1", "secret");
    addRouterHost(2, "user-2", "secret");

    ASSERT_TRUE(corruptRouterHostData(1));

    RouterHostConfig broken;
    EXPECT_EQ(router_hosts_.find(1, &broken), Database::FindResult::UNREADABLE);
    EXPECT_FALSE(router_hosts_.credentials(1).has_value());

    RouterHostConfig readable;
    EXPECT_EQ(router_hosts_.find(2, &readable), Database::FindResult::FOUND);
    EXPECT_EQ(readable.username(), QString("user-2"));

    RouterHostConfig missing;
    EXPECT_EQ(router_hosts_.find(3, &missing), Database::FindResult::NOT_FOUND);
}

//--------------------------------------------------------------------------------------------------
// A search that never ran must not pass for a search that found nothing.
TEST_F(RouterHostStoreTest, SearchOfAClosedDatabaseFails)
{
    Database closed;

    RouterHostConfig host;
    EXPECT_EQ(RouterHostStore(closed).find(100500, &host), Database::FindResult::FAILED);
}

//--------------------------------------------------------------------------------------------------
// The credentials the user saved for a host of a router come back as they were saved.
TEST_F(RouterHostStoreTest, RouterHostCredentialsSurviveARoundTrip)
{
    addRouter();

    ASSERT_TRUE(router_hosts_.add(routerHost(100500, "user", "secret")));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());

    EXPECT_EQ(stored->username(), QString("user"));
    EXPECT_EQ(stored->password().toString(), QString("secret"));
}

//--------------------------------------------------------------------------------------------------
// The row of a host whose credentials did not open is rewritten in place. An insert over it
// breaks on the primary key, so a caller that takes such a row for a missing one loses what the
// user entered.
TEST_F(RouterHostStoreTest, RouterHostThatDoesNotOpenIsRewrittenInPlace)
{
    addRouter();
    addRouterHost(100500, "user", "secret");

    ASSERT_TRUE(corruptRouterHostData(100500));

    RouterHostConfig broken;
    ASSERT_EQ(router_hosts_.find(100500, &broken), Database::FindResult::UNREADABLE);

    EXPECT_FALSE(router_hosts_.add(routerHost(100500, "new-user", "new-secret")));
    ASSERT_TRUE(router_hosts_.modify(routerHost(100500, "new-user", "new-secret")));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->username(), QString("new-user"));
    EXPECT_EQ(stored->password().toString(), QString("new-secret"));
    EXPECT_EQ(allRouterHosts().size(), 1);
}

//--------------------------------------------------------------------------------------------------
// Credentials are added once and edited afterwards. Adding twice for the same host is refused,
// and removing takes the row out.
TEST_F(RouterHostStoreTest, RouterHostCredentialsAreEditedAndRemoved)
{
    addRouter();

    addRouterHost(100500, "user", "secret");

    EXPECT_FALSE(router_hosts_.add(routerHost(100500, "other-user", "other-secret")));
    EXPECT_TRUE(router_hosts_.modify(routerHost(100500, "other-user", "other-secret")));

    // A host the user never saved anything for has no row to edit.
    EXPECT_FALSE(router_hosts_.modify(routerHost(100501, "user", "secret")));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->username(), QString("other-user"));
    EXPECT_EQ(stored->password().toString(), QString("other-secret"));
    EXPECT_EQ(allRouterHosts().size(), 1);

    ASSERT_TRUE(router_hosts_.remove(100500));
    EXPECT_FALSE(findRouterHost(db_, 100500).has_value());
    EXPECT_TRUE(allRouterHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Every column of the table opens with the same key, so what tells one row from another is the host
// id the column was sealed for. A column moved to the row of another host does not open.
TEST_F(RouterHostStoreTest, RouterHostCredentialsDoNotOpenForAnotherHost)
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
TEST_F(RouterHostStoreTest, OutdatedRouterHostsAreTheOnesNotCheckedForLong)
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

    ASSERT_TRUE(router_hosts_.updateCheckTime(100500));
    ASSERT_TRUE(router_hosts_.updateCheckTime(100501));
    ASSERT_TRUE(router_hosts_.updateCheckTime(100502));

    EXPECT_TRUE(outdatedRouterHosts().isEmpty());

    // The answer of the router is trusted for a week. A row asked about a day ago is not offered
    // again, and one asked about eight days ago is.
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 day = 24 * 60 * 60;

    ASSERT_TRUE(setRouterHostCheckTime(100501, now - day));
    ASSERT_TRUE(setRouterHostCheckTime(100502, now - 8 * day));

    EXPECT_EQ(outdatedRouterHosts(), QList<HostId>({ HostId(100502) }));

    // There is nothing to remember for a host the user saved nothing for.
    EXPECT_FALSE(router_hosts_.updateCheckTime(100503));
}

//--------------------------------------------------------------------------------------------------
// Every offered row costs a request to the router, so a single call never takes more than a handful
// of them. What one call left behind is what the next one takes.
TEST_F(RouterHostStoreTest, OutdatedRouterHostsAreTakenInBoundedBatches)
{
    addRouter();

    for (HostId host_id = 100500; host_id < 100512; ++host_id)
        addRouterHost(host_id, "user", "secret");

    // Twelve rows are waiting, and no answer carries more than ten.
    const QList<HostId> first = outdatedRouterHosts();
    EXPECT_EQ(first.size(), 10);

    for (HostId host_id : first)
        ASSERT_TRUE(router_hosts_.updateCheckTime(host_id));

    const QList<HostId> second = outdatedRouterHosts();
    EXPECT_EQ(second.size(), 2);

    for (HostId host_id : second)
    {
        EXPECT_FALSE(first.contains(host_id));
        ASSERT_TRUE(router_hosts_.updateCheckTime(host_id));
    }

    EXPECT_TRUE(outdatedRouterHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A temporary id is handed out again to another host once the one holding it is gone, so what was
// saved under it would be sent to a host the user never gave it to.
TEST_F(RouterHostStoreTest, CredentialsOfATemporaryHostAreNotStored)
{
    RouterHostConfig credentials;
    credentials.setHostId(kMinTempHostId);
    credentials.setUsername("user");
    credentials.setPassword(SecureString(QString("secret")));

    EXPECT_FALSE(router_hosts_.add(credentials));
    EXPECT_FALSE(router_hosts_.modify(credentials));
    EXPECT_TRUE(allRouterHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Credentials are a pair here as well. The row exists to answer a host that asks, and half an
// answer is no answer.
TEST_F(RouterHostStoreTest, CredentialsWithoutAPasswordAreNotStored)
{
    RouterHostConfig credentials;
    credentials.setHostId(100500);
    credentials.setUsername("user");

    EXPECT_FALSE(router_hosts_.add(credentials));
    EXPECT_TRUE(allRouterHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A profile is kept for a host of a router whether or not credentials are saved for it, and the
// credentials come and go without touching it.
TEST_F(RouterHostStoreTest, RouterHostKeepsTheProfileChosenForIt)
{
    addRouter();
    const qint64 profile_id = addProfile("office");

    ASSERT_TRUE(router_hosts_.setProfileId(100500, profile_id));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->profileId(), profile_id);
    EXPECT_TRUE(stored->username().isEmpty());
    EXPECT_EQ(stored->credentialId(), 0);

    std::optional<std::pair<QString, SecureString>> credentials =
        router_hosts_.credentials(100500);
    ASSERT_TRUE(credentials.has_value());
    EXPECT_TRUE(credentials->first.isEmpty());
    EXPECT_TRUE(credentials->second.isEmpty());

    ASSERT_TRUE(router_hosts_.modify(routerHost(100500, "user", "secret")));
    stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->profileId(), profile_id);
    EXPECT_EQ(stored->username(), QString("user"));

    ASSERT_TRUE(router_hosts_.modify(routerHost(100500, QString(), QString())));
    stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->profileId(), profile_id);
    EXPECT_TRUE(stored->username().isEmpty());

    ASSERT_TRUE(router_hosts_.setProfileId(100500, 0));
    EXPECT_FALSE(findRouterHost(db_, 100500).has_value());

    ASSERT_TRUE(router_hosts_.setProfileId(100501, 0));
    EXPECT_FALSE(findRouterHost(db_, 100501).has_value());

    EXPECT_FALSE(router_hosts_.setProfileId(kMinTempHostId, profile_id));
    EXPECT_TRUE(allRouterHosts().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A row that is left with no pair, no links and no profile is gone, whichever of them was the last
// to go. A row that still holds a pair stays when its links go.
TEST_F(RouterHostStoreTest, EmptiedRouterHostRowIsGone)
{
    addRouter();

    CredentialConfig os = credential("workstation", "DOMAIN\\user", "secret");
    os.setType(CredentialConfig::Type::OS);
    ASSERT_TRUE(credentials_.add(os));

    addRouterHost(100500, "user", "secret");
    ASSERT_TRUE(router_hosts_.modify(routerHost(100500, QString(), QString())));
    EXPECT_FALSE(findRouterHost(db_, 100500).has_value());

    addRouterHost(100501, "user", "secret");
    ASSERT_TRUE(router_hosts_.setOsCredentialId(100501, os.id()));
    ASSERT_TRUE(router_hosts_.modify(routerHost(100501, QString(), QString())));
    ASSERT_TRUE(findRouterHost(db_, 100501).has_value());

    ASSERT_TRUE(router_hosts_.setOsCredentialId(100501, 0));
    EXPECT_FALSE(findRouterHost(db_, 100501).has_value());

    addRouterHost(100502, "user", "secret");
    ASSERT_TRUE(router_hosts_.setOsCredentialId(100502, os.id()));
    ASSERT_TRUE(credentials_.remove(os.id()));

    const std::optional<RouterHostConfig> stored = findRouterHost(db_, 100502);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->osCredentialId(), 0);
    EXPECT_EQ(stored->username(), QString("user"));
}

//--------------------------------------------------------------------------------------------------
// The row of a router host links credentials the same way.
TEST_F(RouterHostStoreTest, RouterHostLinksItsCredentials)
{
    addRouter();
    const qint64 credential_id = addCredential("office", "user", "secret");

    RouterHostConfig host = routerHost(100500, "user", "secret");
    host.setCredentialId(credential_id);
    ASSERT_TRUE(router_hosts_.add(host));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->credentialId(), credential_id);

    ASSERT_TRUE(credentials_.remove(credential_id));
    EXPECT_EQ(findRouterHost(db_, 100500)->credentialId(), 0);
}

//--------------------------------------------------------------------------------------------------
// A host of a router remembered by the record of credentials it refers to comes back with the link
// and an empty pair of its own.
TEST_F(RouterHostStoreTest, RouterHostIsKeptByItsLinkAlone)
{
    addRouter();
    const qint64 credential_id = addCredential("office", "user", "secret");

    RouterHostConfig host = routerHost(100500, QString(), QString());
    host.setCredentialId(credential_id);
    ASSERT_TRUE(router_hosts_.add(host));

    std::optional<RouterHostConfig> stored = findRouterHost(db_, 100500);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->credentialId(), credential_id);
    EXPECT_TRUE(stored->username().isEmpty());
    EXPECT_TRUE(stored->password().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The same for a host of a router. A host without a row is entered with nothing.
TEST_F(RouterHostStoreTest, RouterHostIsEnteredWithTheCredentialsItRefersToOrItsOwnPair)
{
    addRouter();
    const qint64 credential_id = addCredential("office", "shared-user", "shared-secret");

    ASSERT_TRUE(router_hosts_.add(routerHost(100500, "user", "secret")));

    RouterHostConfig linked = routerHost(100501, "user", "secret");
    linked.setCredentialId(credential_id);
    ASSERT_TRUE(router_hosts_.add(linked));

    std::optional<std::pair<QString, SecureString>> credentials =
        router_hosts_.credentials(100500);
    ASSERT_TRUE(credentials.has_value());
    EXPECT_EQ(credentials->first, QString("user"));
    EXPECT_EQ(credentials->second.toString(), QString("secret"));

    credentials = router_hosts_.credentials(100501);
    ASSERT_TRUE(credentials.has_value());
    EXPECT_EQ(credentials->first, QString("shared-user"));
    EXPECT_EQ(credentials->second.toString(), QString("shared-secret"));

    EXPECT_FALSE(router_hosts_.credentials(100502).has_value());
}
