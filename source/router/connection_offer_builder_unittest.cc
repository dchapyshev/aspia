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

#include "router/connection_offer_builder.h"

#include <gtest/gtest.h>

#include "base/version_constants.h"
#include "proto/relay_peer.h"
#include "router/shared_hosts.h"
#include "router/shared_key_pool.h"

// The offer that puts a client and a host on the same relay, built from the two registries the
// workers keep. Both are process-wide, so every test starts from empty ones.
class ConnectionOfferBuilderTest : public testing::Test
{
protected:
    void SetUp() override
    {
        SharedHosts::instance().clear();
        SharedKeyPool::instance().clear();

        client_.host_id = kHostId;
        client_.version = kVersion_3_0_0;
        client_.address = "203.0.113.5";
        client_.user_name = "operator";
    }

    void TearDown() override
    {
        SharedHosts::instance().clear();
        SharedKeyPool::instance().clear();
    }

    void addHost(const QVersionNumber& version, const std::string& address = "198.51.100.7")
    {
        SharedHosts::instance().add(kHostId, version, address);
    }

    void addRelayKey(qint64 relay_session_id, quint32 key_id)
    {
        proto::router::RelayKey key;
        key.set_key_id(key_id);
        key.set_encryption(proto::router::RelayKey::ENCRYPTION_CHACHA20_POLY1305);

        SharedKeyPool::instance().add(relay_session_id, "relay.example", 8080, key);
    }

    ConnectionOfferBuilder::Result build()
    {
        return ConnectionOfferBuilder::build(SharedHosts::instance(), SharedKeyPool::instance(),
                                             client_);
    }

    static constexpr HostId kHostId = 100;

    ConnectionOfferBuilder::Client client_;
};

//--------------------------------------------------------------------------------------------------
// The offer carries everything both peers need: where the relay is, the one-time key, and the
// secret that lets the relay pair them.
TEST_F(ConnectionOfferBuilderTest, OfferCarriesTheRelayCredentialsAndTheSecret)
{
    addHost(kVersion_3_0_0, "198.51.100.7");
    addRelayKey(42, 7);

    const ConnectionOfferBuilder::Result result = build();

    ASSERT_EQ(result.offer.error_code(), proto::router::ConnectionOffer::SUCCESS);
    EXPECT_EQ(result.relay_session_id, 42);
    EXPECT_EQ(result.relay_key_id, 7u);

    const proto::router::RelayCredentials& relay = result.offer.relay();
    EXPECT_EQ(relay.host(), "relay.example");
    EXPECT_EQ(relay.port(), 8080);
    EXPECT_EQ(relay.key().key_id(), 7u);

    proto::relay::PeerToRelay::Secret secret;
    ASSERT_TRUE(secret.ParseFromString(relay.secret()));
    EXPECT_EQ(secret.host_id(), kHostId);
    EXPECT_EQ(secret.client_address(), "203.0.113.5");
    EXPECT_EQ(secret.client_user_name(), "operator");
    EXPECT_EQ(secret.host_address(), "198.51.100.7");
    EXPECT_FALSE(secret.random_data().empty());
}

//--------------------------------------------------------------------------------------------------
// Two offers must never hand out the same key or the same pairing secret.
TEST_F(ConnectionOfferBuilderTest, EveryOfferGetsItsOwnKeyAndSecret)
{
    addHost(kVersion_3_0_0);
    addRelayKey(42, 7);
    addRelayKey(42, 8);

    const ConnectionOfferBuilder::Result first = build();
    const ConnectionOfferBuilder::Result second = build();

    ASSERT_EQ(first.offer.error_code(), proto::router::ConnectionOffer::SUCCESS);
    ASSERT_EQ(second.offer.error_code(), proto::router::ConnectionOffer::SUCCESS);

    EXPECT_NE(first.relay_key_id, second.relay_key_id);
    EXPECT_NE(first.offer.relay().secret(), second.offer.relay().secret());
    EXPECT_EQ(SharedKeyPool::instance().count(42), 0u);
}

//--------------------------------------------------------------------------------------------------
// The host is not connected: the client is told so, and no key is burned on an offer nobody can
// answer.
TEST_F(ConnectionOfferBuilderTest, OfflineHostSpendsNoKey)
{
    addRelayKey(42, 7);

    const ConnectionOfferBuilder::Result result = build();

    EXPECT_EQ(result.offer.error_code(), proto::router::ConnectionOffer::PEER_NOT_FOUND);
    EXPECT_FALSE(result.offer.has_relay());
    EXPECT_EQ(result.relay_session_id, 0);
    EXPECT_EQ(SharedKeyPool::instance().count(42), 1u);
}

//--------------------------------------------------------------------------------------------------
// No relay announced any keys (none is connected, or they are all drained): an offer without a key
// would be useless, so the client is told to try again.
TEST_F(ConnectionOfferBuilderTest, EmptyKeyPoolIsReported)
{
    addHost(kVersion_3_0_0);

    const ConnectionOfferBuilder::Result result = build();

    EXPECT_EQ(result.offer.error_code(), proto::router::ConnectionOffer::KEY_POOL_EMPTY);
    EXPECT_FALSE(result.offer.has_relay());
    EXPECT_EQ(result.relay_key_id, 0u);
}

//--------------------------------------------------------------------------------------------------
// AES is used only when both ends understand it. A pair where either side is older falls back to
// the cipher the legacy relay code speaks - otherwise the session would fail after the handshake.
TEST_F(ConnectionOfferBuilderTest, CipherIsTheBestBothPeersUnderstand)
{
    const QVersionNumber legacy(2, 7, 0);

    addHost(kVersion_3_0_0);
    addRelayKey(42, 7);
    client_.version = kVersion_3_0_0;
    EXPECT_EQ(build().offer.relay().key().encryption(),
              proto::router::RelayKey::ENCRYPTION_AES256_GCM);

    SharedHosts::instance().clear();
    addHost(legacy);
    addRelayKey(42, 8);
    client_.version = kVersion_3_0_0;
    EXPECT_EQ(build().offer.relay().key().encryption(),
              proto::router::RelayKey::ENCRYPTION_CHACHA20_POLY1305);

    SharedHosts::instance().clear();
    addHost(kVersion_3_0_0);
    addRelayKey(42, 9);
    client_.version = legacy;
    EXPECT_EQ(build().offer.relay().key().encryption(),
              proto::router::RelayKey::ENCRYPTION_CHACHA20_POLY1305);
}

//--------------------------------------------------------------------------------------------------
// The client is told upfront whether it is about to talk to an old host, so it can speak the
// protocol that host understands.
TEST_F(ConnectionOfferBuilderTest, LegacyHostIsAnnouncedToTheClient)
{
    addHost(QVersionNumber(2, 7, 0));
    addRelayKey(42, 7);

    EXPECT_TRUE(build().offer.peer_info().is_legacy());

    SharedHosts::instance().clear();
    addHost(kVersion_3_0_0);
    addRelayKey(42, 8);

    EXPECT_FALSE(build().offer.peer_info().is_legacy());
}

//--------------------------------------------------------------------------------------------------
// The STUN endpoint is attached only when this router actually runs one; the peers try a direct
// connection with it before falling back to the relay.
TEST_F(ConnectionOfferBuilderTest, StunInfoIsAttachedOnlyWhenTheServerRuns)
{
    addHost(kVersion_3_0_0);
    addRelayKey(42, 7);

    EXPECT_FALSE(build().offer.has_stun_info());

    addRelayKey(42, 8);
    client_.stun_port = 8065;

    const ConnectionOfferBuilder::Result result = build();
    ASSERT_TRUE(result.offer.has_stun_info());
    EXPECT_EQ(result.offer.stun_info().port(), 8065);
    EXPECT_EQ(result.offer.stun_info().version(), 1u);

    // An empty host means "the address of this router" - the client already knows it.
    EXPECT_TRUE(result.offer.stun_info().host().empty());
}
