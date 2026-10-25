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

#include "router/handlers/connection_request_handler.h"

#include <gtest/gtest.h>

#include "base/version_constants.h"
#include "base/crypto/random.h"
#include "proto/relay_peer.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "router/router_test_base.h"
#include "router/shared_hosts.h"
#include "router/shared_key_pool.h"

// The offer that puts a client and a host on the same relay, built from the two registries the
// workers keep. Both are process-wide, so every test starts from empty ones.
class ConnectionRequestHandlerTest : public RouterTestBase
{
protected:
    void SetUp() override
    {
        RouterTestBase::SetUp();

        SharedHosts::instance().clear();
        SharedKeyPool::instance().clear();

        client_.host_id = kHostId;
        client_.session_type = proto::peer::SESSION_TYPE_DESKTOP;
        client_.address = "203.0.113.5";
        client_.user_id = admin_.entry_id;
        client_.user_name = "operator";
        client_.computer_name = "operator-pc";
        client_.os_name = "Windows 11";
        client_.version = "3.2.0";
    }

    void TearDown() override
    {
        SharedHosts::instance().clear();
        SharedKeyPool::instance().clear();
    }

    void addHost(const QVersionNumber& version, const std::string& address = "198.51.100.7")
    {
        SharedHosts::instance().add(kHostId, version, address, "host-pc");
    }

    void addRelayKey(qint64 relay_session_id, quint32 key_id)
    {
        proto::router::RelayKey key;
        key.set_key_id(key_id);

        SharedKeyPool::instance().add(relay_session_id, "relay.example", 8080, key);
    }

    ConnectionRequestResult build()
    {
        return handleConnectionRequest(access_log_, SharedHosts::instance(), SharedKeyPool::instance(), client_);
    }

    proto::router::AccessLogList accessLog()
    {
        proto::router::AccessLogList list;
        access_log_.entries(proto::router::AccessLogFilter(), 0, 0, proto::router::kMaxAccessLogPageSize, &list);
        return list;
    }

    static constexpr HostId kHostId = 100;

    ConnectionRequestClient client_;
};

//--------------------------------------------------------------------------------------------------
// The offer carries everything both peers need: where the relay is, the one-time key, and the
// secret that lets the relay pair them.
TEST_F(ConnectionRequestHandlerTest, OfferCarriesTheRelayCredentialsAndTheSecret)
{
    addHost(kVersion_3_0_0, "198.51.100.7");
    addRelayKey(42, 7);

    const ConnectionRequestResult result = build();

    ASSERT_EQ(result.offer.error_code(), proto::router::kErrorOk);
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
TEST_F(ConnectionRequestHandlerTest, EveryOfferGetsItsOwnKeyAndSecret)
{
    addHost(kVersion_3_0_0);
    addRelayKey(42, 7);
    addRelayKey(42, 8);

    const ConnectionRequestResult first = build();
    const ConnectionRequestResult second = build();

    ASSERT_EQ(first.offer.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(second.offer.error_code(), proto::router::kErrorOk);

    EXPECT_NE(first.relay_key_id, second.relay_key_id);
    EXPECT_NE(first.offer.relay().secret(), second.offer.relay().secret());
    EXPECT_EQ(SharedKeyPool::instance().count(42), 0u);
}

//--------------------------------------------------------------------------------------------------
// The host is not connected: the client is told so, and no key is burned on an offer nobody can
// answer.
TEST_F(ConnectionRequestHandlerTest, OfflineHostSpendsNoKey)
{
    addRelayKey(42, 7);

    const ConnectionRequestResult result = build();

    EXPECT_EQ(result.offer.error_code(), proto::router::kErrorHostOffline);
    EXPECT_FALSE(result.offer.has_relay());
    EXPECT_EQ(result.relay_session_id, 0);
    EXPECT_EQ(SharedKeyPool::instance().count(42), 1u);
}

//--------------------------------------------------------------------------------------------------
// No relay announced any keys (none is connected, or they are all drained): an offer without a key
// would be useless, so the client is told to try again.
TEST_F(ConnectionRequestHandlerTest, EmptyKeyPoolIsReported)
{
    addHost(kVersion_3_0_0);

    const ConnectionRequestResult result = build();

    EXPECT_EQ(result.offer.error_code(), proto::router::kErrorKeyPoolEmpty);
    EXPECT_FALSE(result.offer.has_relay());
    EXPECT_EQ(result.relay_key_id, 0u);
}

//--------------------------------------------------------------------------------------------------
// The relay announces its keys without a cipher, and the offer always sets AES.
TEST_F(ConnectionRequestHandlerTest, CipherIsAes)
{
    addHost(kVersion_3_0_0);
    addRelayKey(42, 7);

    EXPECT_EQ(build().offer.relay().key().encryption(),
              proto::router::RelayKey::ENCRYPTION_AES256_GCM);
}

//--------------------------------------------------------------------------------------------------
// The STUN endpoint is attached only when this router actually runs one; the peers try a direct
// connection with it before falling back to the relay.
TEST_F(ConnectionRequestHandlerTest, StunInfoIsAttachedOnlyWhenTheServerRuns)
{
    addHost(kVersion_3_0_0);
    addRelayKey(42, 7);

    EXPECT_FALSE(build().offer.has_stun_info());

    addRelayKey(42, 8);
    client_.stun_port = 8065;

    const ConnectionRequestResult result = build();
    ASSERT_TRUE(result.offer.has_stun_info());
    EXPECT_EQ(result.offer.stun_info().port(), 8065);
    EXPECT_EQ(result.offer.stun_info().version(), 1u);

    // An empty host means "the address of this router" - the client already knows it.
    EXPECT_TRUE(result.offer.stun_info().host().empty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(ConnectionRequestHandlerTest, IssuedOfferIsRecordedInTheAccessLog)
{
    addHost(kVersion_3_0_0, "198.51.100.7");
    addRelayKey(42, 7);

    ASSERT_EQ(build().offer.error_code(), proto::router::kErrorOk);

    const proto::router::AccessLogList list = accessLog();
    ASSERT_EQ(list.entry_size(), 1);

    const proto::router::AccessLogEntry& entry = list.entry(0);
    EXPECT_EQ(entry.user_id(), admin_.entry_id);
    EXPECT_EQ(entry.user_name(), "operator");
    EXPECT_EQ(entry.session_type(), proto::peer::SESSION_TYPE_DESKTOP);
    EXPECT_EQ(entry.client_address(), "203.0.113.5");
    EXPECT_EQ(entry.client_computer_name(), "operator-pc");
    EXPECT_EQ(entry.client_os_name(), "Windows 11");
    EXPECT_EQ(entry.client_version(), "3.2.0");
    EXPECT_EQ(entry.host_id(), kHostId);
    EXPECT_EQ(entry.host_computer_name(), "host-pc");
    EXPECT_EQ(entry.host_address(), "198.51.100.7");
}

//--------------------------------------------------------------------------------------------------
// The log holds the offers that were issued. A request that got none leaves nothing in it.
TEST_F(ConnectionRequestHandlerTest, RefusedRequestsAreNotRecorded)
{
    addRelayKey(42, 7);
    ASSERT_EQ(build().offer.error_code(), proto::router::kErrorHostOffline);

    SharedKeyPool::instance().clear();
    addHost(kVersion_3_0_0);
    ASSERT_EQ(build().offer.error_code(), proto::router::kErrorKeyPoolEmpty);

    EXPECT_EQ(accessLog().total_count(), 0);
}

//--------------------------------------------------------------------------------------------------
// The router cannot check the type, so it is recorded as the client sent it. An old client sends none.
TEST_F(ConnectionRequestHandlerTest, SessionTypeIsRecordedAsTheClientSentIt)
{
    addHost(kVersion_3_0_0);

    const proto::peer::SessionType kTypes[] = {
        proto::peer::SESSION_TYPE_UNKNOWN,
        proto::peer::SESSION_TYPE_ALL,
        static_cast<proto::peer::SessionType>(1000)
    };

    for (proto::peer::SessionType session_type : kTypes)
    {
        addRelayKey(42, static_cast<quint32>(session_type));
        client_.session_type = session_type;
        ASSERT_EQ(build().offer.error_code(), proto::router::kErrorOk) << session_type;
    }

    const proto::router::AccessLogList list = accessLog();
    ASSERT_EQ(list.entry_size(), 3);
    EXPECT_EQ(list.entry(0).session_type(), kTypes[2]);
    EXPECT_EQ(list.entry(1).session_type(), kTypes[1]);
    EXPECT_EQ(list.entry(2).session_type(), kTypes[0]);
}

//--------------------------------------------------------------------------------------------------
// A broken access log does not stop the router from connecting clients.
TEST_F(ConnectionRequestHandlerTest, OfferIsIssuedWhenTheLogCannotBeWritten)
{
    addHost(kVersion_3_0_0);
    addRelayKey(42, 7);

    ASSERT_TRUE(execRaw(access_log_file_path_, "DROP TABLE access_log"));

    const ConnectionRequestResult result = build();
    EXPECT_EQ(result.offer.error_code(), proto::router::kErrorOk);
    EXPECT_TRUE(result.offer.has_relay());
    EXPECT_EQ(result.relay_key_id, 7u);
}
