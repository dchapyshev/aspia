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

#include "router/relay.h"

#include <functional>
#include <vector>

#include "base/serialization.h"
#include "base/version_constants.h"
#include "base/net/tcp_channel.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "proto/router_relay.h"
#include "router/fake_tcp_channel.h"
#include "router/router_test_base.h"
#include "router/router_test_worker.h"
#include "router/shared_key_pool.h"

// The relay side of the router. A relay announces one time keys, reports statistics and takes
// commands back. The session is the real Relay in a real worker thread and only the socket is a
// stand-in. The key pool is process-wide, so every test starts from an empty one.
class RelayTest : public RouterTestBase
{
protected:
    void SetUp() override
    {
        RouterTestBase::SetUp();
        SharedKeyPool::instance().clear();

        std::unique_ptr<RouterTestWorker> worker = std::make_unique<RouterTestWorker>(file_path_);
        worker_ = worker.get();

        workers_.add(std::move(worker));
        workers_.start();
    }

    void TearDown() override
    {
        SharedKeyPool::instance().clear();
    }

    // Creates the session in the worker thread, runs |body| there and destroys it there. The
    // channel belongs to the session, so it goes away with it.
    void withRelay(const std::function<void(Relay&, FakeTcpChannel*)>& body)
    {
        worker_->invoke([&]()
        {
            FakeTcpChannel* channel = new FakeTcpChannel();

            // A relay authenticates anonymously and has no user, only the peer facts the
            // authenticator collected.
            channel->setPeer(0, std::string(), proto::router::SESSION_TYPE_RELAY, kVersion_3_0_0);

            Relay relay(channel, nullptr);
            relay.start();
            channel->clearSent();

            body(relay, channel);
        });
    }

    // A key as the relay generates it, with an X25519 public key and the nonce of the AEAD.
    static proto::router::RelayKey makeKey(quint32 key_id)
    {
        proto::router::RelayKey key;
        key.set_key_id(key_id);
        key.set_type(proto::router::RelayKey::TYPE_X25519);
        key.set_public_key(std::string(32, 'p'));
        key.set_iv(std::string(12, 'i'));
        return key;
    }

    static QByteArray keyPool(const std::string& peer_host, quint32 peer_port,
                              const std::vector<proto::router::RelayKey>& keys)
    {
        proto::router::RelayToRouter message;
        proto::router::RelayKeyPool* pool = message.mutable_key_pool();
        pool->set_peer_host(peer_host);
        pool->set_peer_port(peer_port);

        for (const proto::router::RelayKey& key : keys)
            pool->add_key()->CopyFrom(key);

        return serialize(message);
    }

    // The relay list as RelayWorker::doRelayList builds it. The statistics of every relay travel
    // to the administrator inside one message.
    static QByteArray relayList(const std::vector<Relay*>& relays)
    {
        proto::router::RouterToAdmin message;

        for (const Relay* relay : relays)
        {
            proto::router::RelayInfo* item = message.mutable_relay_list()->add_relay();
            item->set_entry_id(relay->sessionId());
            item->set_ip_address(relay->address());

            const std::optional<proto::router::RelayStatistics>& statistics = relay->statistics();
            if (statistics.has_value())
            {
                item->mutable_statistics()->mutable_peer()->CopyFrom(statistics->peer());
                item->mutable_statistics()->set_uptime(statistics->uptime());
            }
        }

        return serialize(message);
    }

    WorkerManager workers_;
    RouterTestWorker* worker_ = nullptr;
};

//--------------------------------------------------------------------------------------------------
// The keys a relay announces are what every connection offer is built from, and they carry the
// endpoint the peers are sent to.
TEST_F(RelayTest, AnnouncedKeysBecomeOffers)
{
    withRelay([](Relay& relay, FakeTcpChannel* channel)
    {
        channel->receive(0, keyPool("relay.example", 8080, { makeKey(10), makeKey(11) }));

        EXPECT_EQ(SharedKeyPool::instance().count(relay.sessionId()), 2u);

        const std::optional<SharedKeyPool::Credentials> credentials =
            SharedKeyPool::instance().take();
        ASSERT_TRUE(credentials.has_value());
        EXPECT_EQ(credentials->session_id, relay.sessionId());
        EXPECT_EQ(credentials->peer_host, "relay.example");
        EXPECT_EQ(credentials->peer_port, 8080);
        EXPECT_FALSE(credentials->key.public_key().empty());
    });
}

//--------------------------------------------------------------------------------------------------
// The endpoint comes from the relay over the wire. An offer built on a pool without a host, or on
// a port that does not survive the cast to quint16, would send the peers nowhere.
TEST_F(RelayTest, PoolWithAnUnusableEndpointIsIgnored)
{
    withRelay([](Relay& relay, FakeTcpChannel* channel)
    {
        channel->receive(0, keyPool(std::string(), 8080, { makeKey(10) }));
        channel->receive(0, keyPool("relay.example", 0, { makeKey(11) }));
        channel->receive(0, keyPool("relay.example", 70000, { makeKey(12) }));

        EXPECT_EQ(SharedKeyPool::instance().count(relay.sessionId()), 0u);
    });
}

//--------------------------------------------------------------------------------------------------
// A key without usable material spends the offer that takes it and then fails at the relay, which
// cannot derive a session from it. The client sees a broken connection and has nothing better to
// retry with, because the pool holds more keys like that one.
TEST_F(RelayTest, KeyWithoutUsableMaterialIsRejected)
{
    withRelay([](Relay& relay, FakeTcpChannel* channel)
    {
        proto::router::RelayKey without_public_key = makeKey(10);
        without_public_key.clear_public_key();

        proto::router::RelayKey without_iv = makeKey(11);
        without_iv.clear_iv();

        proto::router::RelayKey without_type = makeKey(12);
        without_type.set_type(proto::router::RelayKey::TYPE_UNKNOWN);

        channel->receive(0, keyPool("relay.example", 8080,
                                    { without_public_key, without_iv, without_type }));

        EXPECT_EQ(SharedKeyPool::instance().count(relay.sessionId()), 0u);
        EXPECT_FALSE(SharedKeyPool::instance().take().has_value());
    });
}

//--------------------------------------------------------------------------------------------------
// A malformed key is dropped on its own and the usable keys of the same pool are still announced.
TEST_F(RelayTest, GoodKeysOfAPartlyMalformedPoolAreKept)
{
    withRelay([](Relay& relay, FakeTcpChannel* channel)
    {
        proto::router::RelayKey broken = makeKey(10);
        broken.clear_iv();

        channel->receive(0, keyPool("relay.example", 8080, { broken, makeKey(11) }));

        EXPECT_EQ(SharedKeyPool::instance().count(relay.sessionId()), 1u);

        const std::optional<SharedKeyPool::Credentials> credentials =
            SharedKeyPool::instance().take();
        ASSERT_TRUE(credentials.has_value());
        EXPECT_EQ(credentials->key.key_id(), 11u);
    });
}

//--------------------------------------------------------------------------------------------------
// What the relay reports about its sessions is what the administrator console shows.
TEST_F(RelayTest, StatisticsOfTheRelayIsKept)
{
    withRelay([](Relay& relay, FakeTcpChannel* channel)
    {
        proto::router::RelayToRouter message;
        proto::router::RelayStatistics* statistics = message.mutable_statistics();
        statistics->set_uptime(1234);

        proto::router::Peer* peer = statistics->add_peer();
        peer->set_peer_id(7);
        peer->set_host_id(100);
        peer->set_client_address("203.0.113.5");

        channel->receive(0, serialize(message));

        ASSERT_TRUE(relay.statistics().has_value());
        EXPECT_EQ(relay.statistics()->uptime(), 1234);
        ASSERT_EQ(relay.statistics()->peer_size(), 1);
        EXPECT_EQ(relay.statistics()->peer(0).peer_id(), 7);
    });
}

//--------------------------------------------------------------------------------------------------
// The report of a relay working at its capacity is kept whole and still fits the message the
// administrator is sent.
TEST_F(RelayTest, StatisticsOfAFullRelayIsKept)
{
    withRelay([](Relay& relay, FakeTcpChannel* channel)
    {
        constexpr int kMaxPeerCount = 1000; // The capacity limit of the relay.

        proto::router::RelayToRouter message;
        proto::router::RelayStatistics* statistics = message.mutable_statistics();

        for (int i = 0; i < kMaxPeerCount; ++i)
        {
            proto::router::Peer* peer = statistics->add_peer();
            peer->set_peer_id(i);
            peer->set_client_address("203.0.113.5");
            peer->set_host_address("198.51.100.7");
            peer->set_client_user_name("operator");
        }

        channel->receive(0, serialize(message));

        ASSERT_TRUE(relay.statistics().has_value());
        EXPECT_EQ(relay.statistics()->peer_size(), kMaxPeerCount);
        EXPECT_LE(relayList({ &relay }).size(), qsizetype(TcpChannel::kMaxMessageSize));
    });
}

//--------------------------------------------------------------------------------------------------
// The relay learns that one of its keys is gone, or it would keep announcing a key the router has
// already handed to a pair of peers.
TEST_F(RelayTest, KeyUsedIsForwardedToTheRelay)
{
    withRelay([](Relay& relay, FakeTcpChannel* channel)
    {
        relay.sendKeyUsed(42);

        ASSERT_EQ(channel->sent().size(), 1);

        proto::router::RouterToRelay message;
        ASSERT_TRUE(parse(channel->sent().back().buffer, &message));
        ASSERT_TRUE(message.has_key_used());
        EXPECT_EQ(message.key_used().key_id(), 42u);
    });
}

//--------------------------------------------------------------------------------------------------
// The administrator drops a peer session of a relay and this command carries that decision.
TEST_F(RelayTest, PeerRequestIsForwardedToTheRelay)
{
    withRelay([](Relay& relay, FakeTcpChannel* channel)
    {
        proto::router::PeerRequest request;
        request.set_command_name(proto::router::kCommandPeerDisconnect);
        request.set_peer_id(7);

        relay.disconnectPeerSession(request);

        ASSERT_EQ(channel->sent().size(), 1);

        proto::router::RouterToRelay message;
        ASSERT_TRUE(parse(channel->sent().back().buffer, &message));
        ASSERT_TRUE(message.has_peer_request());
        EXPECT_EQ(message.peer_request().command_name(), proto::router::kCommandPeerDisconnect);
        EXPECT_EQ(message.peer_request().peer_id(), 7);
    });
}

//--------------------------------------------------------------------------------------------------
// Anything the router cannot make sense of leaves no keys, no statistics and no answer behind.
TEST_F(RelayTest, GarbageFromTheRelayIsIgnored)
{
    withRelay([](Relay& relay, FakeTcpChannel* channel)
    {
        proto::router::RelayToRouter empty;
        channel->receive(0, serialize(empty));
        channel->receive(0, QByteArray::fromHex("ffffffffffffffff"));

        EXPECT_EQ(SharedKeyPool::instance().count(relay.sessionId()), 0u);
        EXPECT_FALSE(relay.statistics().has_value());
        EXPECT_TRUE(channel->nothingSent());
    });
}
