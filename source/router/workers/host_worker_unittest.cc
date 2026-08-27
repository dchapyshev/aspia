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

#include "router/workers/host_worker.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <asio/ip/tcp.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "base/serialization.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/key_pair.h"
#include "base/net/tcp_channel_ng.h"
#include "base/peer/client_authenticator.h"
#include "proto/key_exchange.h"
#include "proto/router.h"
#include "proto/router_constants.h"
#include "proto/router_host.h"
#include "router/database.h"
#include "router/router_test_worker.h"
#include "router/settings.h"
#include "router/shared_hosts.h"

namespace {

// The waits exist to fail a broken test instead of hanging forever, not to measure anything.
const Seconds kWaitTimeout{ 30 };

const char kHostKey[] = "the-key-the-host-presents";
const char kHardwareId[] = "hw-id-of-the-host";

// How long a connection that has not asked for an id is kept.
constexpr Seconds kIdentifyTimeout{ 30 };

// How many id requests one connection may make.
constexpr int kMaxIdRequests = 5;

//--------------------------------------------------------------------------------------------------
// A free loopback port for the listener of the worker. The port is released before the worker
// starts, and the worker binds with reuse_address, so the window between the two is harmless.
quint16 pickFreePort()
{
    asio::io_context io_context;
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 0);

    std::error_code error_code;
    asio::ip::tcp::acceptor acceptor(io_context);
    acceptor.open(endpoint.protocol(), error_code);
    CHECK(!error_code);
    acceptor.bind(endpoint, error_code);
    CHECK(!error_code);

    return acceptor.local_endpoint().port();
}

} // namespace

// Fires the timer of the worker with a synthetic clock, in the thread of the worker. In production
// the timer runs with the real time, and the tests cannot wait the real timeouts out.
class HostWorkerTestPeer
{
public:
    explicit HostWorkerTestPeer(HostWorker* worker)
        : worker_(worker)
    {
        // Nothing
    }

    void fireTimer(TimePoint now)
    {
        std::mutex lock;
        std::condition_variable finished;
        bool done = false;

        worker_->post([&]()
        {
            worker_->onTimer(now);

            std::lock_guard guard(lock);
            done = true;
            finished.notify_one();
        });

        std::unique_lock guard(lock);
        finished.wait(guard, [&]() { return done; });
    }

private:
    HostWorker* worker_;
};

// The host channel of the router end to end. A real HostWorker listens on the loopback interface
// with the anonymous access it grants hosts, and the peer of the test connects and asks for its id
// exactly the way a host does.
class HostWorkerTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());

        db_path_ = temp_dir_.path() + "/router.db3";
        qputenv("ASPIA_ROUTER_CONFIG_FILE", (temp_dir_.path() + "/router.conf").toLocal8Bit());
        qputenv("ASPIA_ROUTER_DB_FILE", db_path_.toLocal8Bit());

        SharedHosts::instance().clear();

        router_keys_ = KeyPair::create(KeyPair::Type::X25519);
        ASSERT_TRUE(router_keys_.isValid());

        host_port_ = pickFreePort();

        Settings settings;
        settings.setHostPrivateKey(SecureByteArray(router_keys_.privateKey()));
        settings.setListenInterface("127.0.0.1");
        settings.setHostPort(host_port_);
        settings.setLegacyHostPort(pickFreePort());
        settings.sync();

        // The host is already approved, so it presents its key and gets its permanent id.
        ASSERT_TRUE(database_.open(db_path_));
        ASSERT_TRUE(database_.addHost(keyHash(kHostKey), kHardwareId));
        ASSERT_EQ(database_.hostId(keyHash(kHostKey), &host_id_), proto::router::kErrorOk);

        std::unique_ptr<RouterTestWorker> peer_worker = std::make_unique<RouterTestWorker>(db_path_);
        peer_worker_ = peer_worker.get();
        workers_.add(std::move(peer_worker));

        std::unique_ptr<HostWorker> host_worker = std::make_unique<HostWorker>();
        host_worker_ = host_worker.get();
        workers_.add(std::move(host_worker));

        workers_.start();
    }

    void TearDown() override
    {
        for (const std::unique_ptr<Peer>& peer : peers_)
            closePeer(peer.get());
        peers_.clear();

        SharedHosts::instance().clear();
        qunsetenv("ASPIA_ROUTER_CONFIG_FILE");
        qunsetenv("ASPIA_ROUTER_DB_FILE");
    }

    // One connected peer of the test and what the router did to it.
    struct Peer
    {
        TcpChannel* channel = nullptr;
        std::atomic<HostId> assigned_host_id { kInvalidHostId };
        std::atomic<bool> authenticated { false };
        std::atomic<bool> disconnected { false };
    };

    // What the router stores for a host is the hash of the key the host presents.
    static QByteArray keyHash(std::string_view key)
    {
        return GenericHash::hash(GenericHash::Type::BLAKE2b512, key);
    }

    // Connects a peer that behaves like a host: anonymous authentication as a host, then
    // one request for its existing id. Returns once the router has answered. A peer that does not
    // ask stays silent after the authentication, and the call returns as soon as it is in.
    [[nodiscard]] Peer* connectHost(bool ask_for_id = true)
    {
        peers_.push_back(std::make_unique<Peer>());
        Peer* peer = peers_.back().get();

        peer_worker_->invoke([this, peer, ask_for_id]()
        {
            ClientAuthenticator* authenticator = new ClientAuthenticator();
            authenticator->setIdentify(proto::key_exchange::IDENTIFY_ANONYMOUS);
            authenticator->setPeerPublicKey(router_keys_.publicKey());
            authenticator->setSessionType(proto::router::SESSION_TYPE_HOST);

            peer->channel = new TcpChannelNG(authenticator, nullptr);

            QObject::connect(peer_worker_, &Worker::sig_tick, peer->channel, &TcpChannel::tick);

            QObject::connect(peer->channel, &TcpChannel::sig_errorOccurred, peer->channel,
                             [peer](TcpChannel::ErrorCode /* error_code */)
            {
                peer->disconnected = true;
            });

            QObject::connect(peer->channel, &TcpChannel::sig_authenticated, peer->channel,
                             [peer, ask_for_id]()
            {
                peer->channel->setPaused(false);
                peer->authenticated = true;

                if (!ask_for_id)
                    return;

                proto::router::HostToRouter message;
                proto::router::HostIdRequest* request = message.mutable_host_id_request();
                request->set_type(proto::router::HostIdRequest::EXISTING_ID);
                request->set_key(kHostKey);
                request->set_hw_id(kHardwareId);

                peer->channel->send(0, serialize(message));
            });

            QObject::connect(peer->channel, &TcpChannel::sig_messageReceived, peer->channel,
                             [peer](quint8 /* channel_id */, const QByteArray& buffer)
            {
                proto::router::RouterToHost message;
                if (!parse(buffer, &message))
                    return;

                if (message.has_host_id_response())
                    peer->assigned_host_id = message.host_id_response().host_id();
            });

            peer->channel->connectTo("127.0.0.1", host_port_);
        });

        const bool ready = ask_for_id
            ? waitFor([&]() { return peer->assigned_host_id.load() == host_id_; })
            : waitFor([&]() { return peer->authenticated.load(); });

        return ready ? peer : nullptr;
    }

    // Connects a peer that behaves like a brand new host: anonymous authentication, then a request
    // for a new id. The router answers with a temporary id and keeps the connection among the
    // temporary hosts until an administrator approves it.
    [[nodiscard]] Peer* connectTempHost()
    {
        peers_.push_back(std::make_unique<Peer>());
        Peer* peer = peers_.back().get();

        peer_worker_->invoke([this, peer]()
        {
            ClientAuthenticator* authenticator = new ClientAuthenticator();
            authenticator->setIdentify(proto::key_exchange::IDENTIFY_ANONYMOUS);
            authenticator->setPeerPublicKey(router_keys_.publicKey());
            authenticator->setSessionType(proto::router::SESSION_TYPE_HOST);

            peer->channel = new TcpChannelNG(authenticator, nullptr);

            QObject::connect(peer_worker_, &Worker::sig_tick, peer->channel, &TcpChannel::tick);

            QObject::connect(peer->channel, &TcpChannel::sig_errorOccurred, peer->channel,
                             [peer](TcpChannel::ErrorCode /* error_code */)
            {
                peer->disconnected = true;
            });

            QObject::connect(peer->channel, &TcpChannel::sig_authenticated, peer->channel, [peer]()
            {
                peer->channel->setPaused(false);
                peer->authenticated = true;

                proto::router::HostToRouter message;
                proto::router::HostIdRequest* request = message.mutable_host_id_request();
                request->set_type(proto::router::HostIdRequest::NEW_ID);
                request->set_hw_id(kHardwareId);

                peer->channel->send(0, serialize(message));
            });

            QObject::connect(peer->channel, &TcpChannel::sig_messageReceived, peer->channel,
                             [peer](quint8 /* channel_id */, const QByteArray& buffer)
            {
                proto::router::RouterToHost message;
                if (!parse(buffer, &message))
                    return;

                if (message.has_host_id_response())
                    peer->assigned_host_id = message.host_id_response().host_id();
            });

            peer->channel->connectTo("127.0.0.1", host_port_);
        });

        const bool ready =
            waitFor([peer]() { return isTempHostId(peer->assigned_host_id.load()); });

        return ready ? peer : nullptr;
    }

    // Asks the worker for a page of the temporary host list from another worker thread, the way
    // an operator does.
    proto::router::TempHostList tempHostList(qint64 offset, qint64 count)
    {
        proto::router::TempHostList list;
        std::atomic<bool> done { false };

        peer_worker_->invoke([&]()
        {
            host_worker_->requestTempHostList(true, offset, count, peer_worker_,
                                              [&](proto::router::TempHostList&& result)
            {
                list = std::move(result);
                done = true;
            });
        });

        EXPECT_TRUE(waitFor([&]() { return done.load(); }));
        return list;
    }

    // The peer goes away the way a host whose connection was lost does. The channel belongs to the
    // thread of the peer worker, and so does its teardown.
    void closePeer(Peer* peer)
    {
        peer_worker_->invoke([peer]()
        {
            delete peer->channel;
            peer->channel = nullptr;
        });
    }

    // One more request for an existing id, the way a host that was told "not found" would repeat.
    void sendIdRequest(Peer* peer, const char* key)
    {
        peer_worker_->invoke([peer, key]()
        {
            proto::router::HostToRouter message;
            proto::router::HostIdRequest* request = message.mutable_host_id_request();
            request->set_type(proto::router::HostIdRequest::EXISTING_ID);
            request->set_key(key);
            request->set_hw_id(kHardwareId);

            peer->channel->send(0, serialize(message));
        });
    }

    // Removes the host the way the administrator console does, from another worker thread.
    void removeHost()
    {
        peer_worker_->invoke([this]()
        {
            host_worker_->removeHost(host_id_, peer_worker_,
                                     [](HostWorker::RemoveHostResult&& /* result */) {});
        });
    }

    // Fires the timer with the synthetic |now| until the router drops the peer. The polling covers
    // the gap between the authentication of the peer and the moment the worker picks it up.
    [[nodiscard]] bool firedUntilDisconnected(Peer* peer, TimePoint now)
    {
        HostWorkerTestPeer timer(host_worker_);

        const TimePoint give_up = Clock::now() + kWaitTimeout;
        while (Clock::now() < give_up)
        {
            timer.fireTimer(now);
            if (peer->disconnected.load())
                return true;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return false;
    }

    // Polls |condition| until it holds. The wait bounds a broken test, the condition is what the
    // test is actually about.
    [[nodiscard]] static bool waitFor(const std::function<bool()>& condition,
                                      Seconds timeout = kWaitTimeout)
    {
        const TimePoint give_up = Clock::now() + timeout;
        while (Clock::now() < give_up)
        {
            if (condition())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return false;
    }

    QTemporaryDir temp_dir_;
    QString db_path_;
    Database database_;

    KeyPair router_keys_;
    quint16 host_port_ = 0;
    HostId host_id_ = kInvalidHostId;

    WorkerManager workers_;
    RouterTestWorker* peer_worker_ = nullptr;
    HostWorker* host_worker_ = nullptr;
    std::vector<std::unique_ptr<Peer>> peers_;
};

//--------------------------------------------------------------------------------------------------
// A connected host is announced as reachable, which is what makes a connection offer to it possible.
TEST_F(HostWorkerTest, ConnectedHostIsAnnounced)
{
    ASSERT_TRUE(connectHost());
    EXPECT_TRUE(waitFor([this]() { return SharedHosts::instance().contains(host_id_); }));
}

//--------------------------------------------------------------------------------------------------
// A host that reconnects while its old connection is still around displaces it. The old connection
// goes away and the host stays announced through the new one.
TEST_F(HostWorkerTest, ReconnectedHostDisplacesItsStalePredecessor)
{
    Peer* first = connectHost();
    ASSERT_TRUE(first);
    ASSERT_TRUE(waitFor([this]() { return SharedHosts::instance().contains(host_id_); }));

    ASSERT_TRUE(connectHost());

    EXPECT_TRUE(waitFor([first]() { return first->disconnected.load(); }));
    EXPECT_TRUE(SharedHosts::instance().contains(host_id_));
}

//--------------------------------------------------------------------------------------------------
// A host can have more than one connection for a while: the router has not noticed that the old
// one is dead yet, and the host has already reconnected. Removing the host must not leave a
// connection behind that puts it back among the reachable ones when the other one goes away.
TEST_F(HostWorkerTest, RemovedHostIsNotAnnouncedByAnotherConnection)
{
    Peer* first = connectHost();
    ASSERT_TRUE(first);
    ASSERT_TRUE(waitFor([this]() { return SharedHosts::instance().contains(host_id_); }));

    removeHost();
    ASSERT_TRUE(waitFor([this]() { return !SharedHosts::instance().contains(host_id_); }));

    // The host reconnects, is recognised by the record on its way out and told to remove itself.
    // The stale predecessor is dropped in favour of the new one, the same as for any other host.
    ASSERT_TRUE(connectHost());
    EXPECT_TRUE(waitFor([first]() { return first->disconnected.load(); }));

    EXPECT_FALSE(waitFor([this]() { return SharedHosts::instance().contains(host_id_); },
                         Seconds(2)));
}

//--------------------------------------------------------------------------------------------------
// A host asks for its id as soon as it is authenticated. A connection that never asks serves
// nobody, and anonymous access means anybody can open one, so it is not kept around.
TEST_F(HostWorkerTest, SilentConnectionIsDropped)
{
    Peer* peer = connectHost(false);
    ASSERT_TRUE(peer);

    EXPECT_TRUE(firedUntilDisconnected(peer, Clock::now() + kIdentifyTimeout + Seconds(1)));
}

//--------------------------------------------------------------------------------------------------
// A request that finds nothing does not spend the single attempt a connection has, or a host told
// "not found" could not ask for a new id. What it must not buy is an endless stream of lookups on
// a connection that costs the peer nothing.
TEST_F(HostWorkerTest, TooManyIdRequestsCloseTheConnection)
{
    Peer* peer = connectHost(false);
    ASSERT_TRUE(peer);

    for (int i = 0; i < kMaxIdRequests + 1; ++i)
        sendIdRequest(peer, "the-key-nobody-knows");

    EXPECT_TRUE(waitFor([peer]() { return peer->disconnected.load(); }));
}

//--------------------------------------------------------------------------------------------------
// A removed host stops being reachable at once. Its connection lives on until it carries the remove
// command out, and while it is announced a client that knows the id would be given an offer to a
// host the administrator has already removed.
TEST_F(HostWorkerTest, RemovedHostIsNoLongerAnnounced)
{
    ASSERT_TRUE(connectHost());
    ASSERT_TRUE(waitFor([this]() { return SharedHosts::instance().contains(host_id_); }));

    removeHost();

    EXPECT_TRUE(waitFor([this]() { return !SharedHosts::instance().contains(host_id_); }));
}

//--------------------------------------------------------------------------------------------------
// The temporary host list is served page by page in one stable order, with the count of the whole
// list on every page.
TEST_F(HostWorkerTest, TempHostListIsPagedInAStableOrder)
{
    ASSERT_TRUE(connectTempHost());
    ASSERT_TRUE(connectTempHost());
    ASSERT_TRUE(connectTempHost());

    const proto::router::TempHostList first = tempHostList(0, 2);
    ASSERT_EQ(first.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(first.total_count(), 3);
    ASSERT_EQ(first.host_size(), 2);
    EXPECT_LT(first.host(0).temp_id(), first.host(1).temp_id());

    const proto::router::TempHostList second = tempHostList(2, 2);
    ASSERT_EQ(second.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(second.total_count(), 3);
    ASSERT_EQ(second.host_size(), 1);
    EXPECT_LT(first.host(1).temp_id(), second.host(0).temp_id());
}

//--------------------------------------------------------------------------------------------------
// A request that names no page has a count of zero and is refused, as is one that asks for more
// than the cap. Whatever comes out of the list goes into a single reply, so its size must never
// follow the number of connected hosts.
TEST_F(HostWorkerTest, TempHostListWithoutAPageIsRefused)
{
    ASSERT_TRUE(connectTempHost());

    const proto::router::TempHostList refused = tempHostList(0, 0);
    EXPECT_EQ(refused.error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(refused.total_count(), 0);
    EXPECT_EQ(refused.host_size(), 0);

    const proto::router::TempHostList oversized =
        tempHostList(0, proto::router::kMaxTempHostPageSize + 1);
    EXPECT_EQ(oversized.error_code(), proto::router::kErrorInvalidRequest);
}

