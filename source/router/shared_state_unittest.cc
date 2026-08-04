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

#include <gtest/gtest.h>

#include "base/peer/host_id.h"
#include "router/shared_hosts.h"
#include "router/shared_key_pool.h"

// The state the workers share with the client sessions: which hosts are online right now, and the
// one-time relay keys a connection offer is built from. Both are process-wide, so every test
// starts from an empty registry.

class SharedHostsTest : public testing::Test
{
protected:
    void SetUp() override { SharedHosts::instance().clear(); }
    void TearDown() override { SharedHosts::instance().clear(); }
};

class SharedKeyPoolTest : public testing::Test
{
protected:
    void SetUp() override { SharedKeyPool::instance().clear(); }
    void TearDown() override { SharedKeyPool::instance().clear(); }

    static proto::router::RelayKey makeKey(quint32 key_id)
    {
        proto::router::RelayKey key;
        key.set_key_id(key_id);
        return key;
    }
};

//--------------------------------------------------------------------------------------------------
// The online flag of every host list and the target of every connection offer come from here.
TEST_F(SharedHostsTest, HostIsVisibleWhileItsSessionLives)
{
    SharedHosts& hosts = SharedHosts::instance();

    EXPECT_FALSE(hosts.contains(HostId(1)));
    EXPECT_FALSE(hosts.find(HostId(1)).has_value());

    hosts.add(HostId(1), QVersionNumber(3, 0, 0), "192.168.1.10");

    EXPECT_TRUE(hosts.contains(HostId(1)));

    const std::optional<SharedHosts::Host> found = hosts.find(HostId(1));
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->version, QVersionNumber(3, 0, 0));
    EXPECT_EQ(found->address, "192.168.1.10");

    hosts.remove(HostId(1));
    EXPECT_FALSE(hosts.contains(HostId(1)));
}

//--------------------------------------------------------------------------------------------------
// A host that reconnects from another address (or after an update) replaces what the registry
// knew about it: the offer must go to the session that is live now.
TEST_F(SharedHostsTest, ReconnectionReplacesTheEntry)
{
    SharedHosts& hosts = SharedHosts::instance();

    hosts.add(HostId(1), QVersionNumber(2, 7, 0), "192.168.1.10");
    hosts.add(HostId(1), QVersionNumber(3, 0, 0), "10.0.0.5");

    const std::optional<SharedHosts::Host> found = hosts.find(HostId(1));
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->version, QVersionNumber(3, 0, 0));
    EXPECT_EQ(found->address, "10.0.0.5");
}

//--------------------------------------------------------------------------------------------------
TEST_F(SharedHostsTest, RemovingAnUnknownHostChangesNothing)
{
    SharedHosts& hosts = SharedHosts::instance();

    hosts.add(HostId(1), QVersionNumber(3, 0, 0), "192.168.1.10");
    hosts.remove(HostId(2));

    EXPECT_TRUE(hosts.contains(HostId(1)));
}

//--------------------------------------------------------------------------------------------------
// A key is one-time: the offer that took it is the only one that can use it.
TEST_F(SharedKeyPoolTest, KeysAreHandedOutOnce)
{
    SharedKeyPool& pool = SharedKeyPool::instance();

    EXPECT_FALSE(pool.take().has_value());

    pool.add(1, "relay.example", 8080, makeKey(10));
    pool.add(1, "relay.example", 8080, makeKey(11));
    EXPECT_EQ(pool.count(1), 2u);

    const std::optional<SharedKeyPool::Credentials> first = pool.take();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->session_id, 1);
    EXPECT_EQ(first->peer_host, "relay.example");
    EXPECT_EQ(first->peer_port, 8080);
    EXPECT_EQ(pool.count(1), 1u);

    const std::optional<SharedKeyPool::Credentials> second = pool.take();
    ASSERT_TRUE(second.has_value());
    EXPECT_NE(second->key.key_id(), first->key.key_id());

    // The pool is empty again, and an offer without a key is refused rather than built with a
    // stale one.
    EXPECT_EQ(pool.count(1), 0u);
    EXPECT_FALSE(pool.take().has_value());
}

//--------------------------------------------------------------------------------------------------
// With several relays the keys come from the one with the largest pool, so the load spreads
// instead of draining a single relay.
TEST_F(SharedKeyPoolTest, TheLargestPoolIsPreferred)
{
    SharedKeyPool& pool = SharedKeyPool::instance();

    pool.add(1, "relay-one", 8080, makeKey(10));
    pool.add(2, "relay-two", 8081, makeKey(20));
    pool.add(2, "relay-two", 8081, makeKey(21));
    pool.add(2, "relay-two", 8081, makeKey(22));

    const std::optional<SharedKeyPool::Credentials> credentials = pool.take();
    ASSERT_TRUE(credentials.has_value());
    EXPECT_EQ(credentials->session_id, 2);
    EXPECT_EQ(credentials->peer_host, "relay-two");
    EXPECT_EQ(pool.count(2), 2u);
    EXPECT_EQ(pool.count(1), 1u);
}

//--------------------------------------------------------------------------------------------------
// A relay that goes away takes its keys with it: an offer built on them would send the peers to a
// relay that is not listening any more.
TEST_F(SharedKeyPoolTest, DisconnectedRelayTakesItsKeysAway)
{
    SharedKeyPool& pool = SharedKeyPool::instance();

    pool.add(1, "relay-one", 8080, makeKey(10));
    pool.add(2, "relay-two", 8081, makeKey(20));

    pool.remove(1);

    EXPECT_EQ(pool.count(1), 0u);

    const std::optional<SharedKeyPool::Credentials> credentials = pool.take();
    ASSERT_TRUE(credentials.has_value());
    EXPECT_EQ(credentials->session_id, 2);
}

//--------------------------------------------------------------------------------------------------
// The address of a relay can change between announcements; the keys must be handed out with the
// address the relay reports now.
TEST_F(SharedKeyPoolTest, LatestRelayAddressIsUsed)
{
    SharedKeyPool& pool = SharedKeyPool::instance();

    pool.add(1, "old.example", 8080, makeKey(10));
    pool.add(1, "new.example", 9090, makeKey(11));

    const std::optional<SharedKeyPool::Credentials> credentials = pool.take();
    ASSERT_TRUE(credentials.has_value());
    EXPECT_EQ(credentials->peer_host, "new.example");
    EXPECT_EQ(credentials->peer_port, 9090);
}

//--------------------------------------------------------------------------------------------------
// A relay that announces without bound would grow the pool until the router runs out of memory.
TEST_F(SharedKeyPoolTest, PerRelayLimitIsEnforced)
{
    SharedKeyPool& pool = SharedKeyPool::instance();

    constexpr quint32 kOverLimit = 1100; // The per-relay limit is 1000.
    for (quint32 i = 0; i < kOverLimit; ++i)
        pool.add(1, "relay-one", 8080, makeKey(i));

    EXPECT_EQ(pool.count(1), 1000u);
}
