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

#include "relay/workers/router_worker.h"

#include <QPointer>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <thread>

#include "base/crypto/key_pair.h"
#include "base/net/tcp_server.h"
#include "proto/router.h"
#include "relay/relay_test_worker.h"
#include "relay/settings.h"
#include "relay/shared_key_pool.h"
#include "relay/workers/relay_worker.h"

namespace {

// The waits exist to fail a broken test instead of hanging forever, not to measure anything.
const Seconds kWaitTimeout{ 30 };

constexpr Seconds kKeyUseTimeout{ 30 };   // RouterWorker, the budget a handed out key has.
constexpr Seconds kReconnectTimeout{ 15 };  // RouterWorker, the pause between connect attempts.

constexpr quint32 kMaxPeerCount = 3;

// The loopback port range the stand probes for its listener.
constexpr quint16 kFirstPort = 48021;
constexpr quint16 kPortCount = 50;

//--------------------------------------------------------------------------------------------------
// A free loopback port for the peer listener of the relay. The port is released before the worker
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
class RouterWorkerTestPeer
{
public:
    explicit RouterWorkerTestPeer(RouterWorker* worker)
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
    RouterWorker* worker_;
};

// The router side of the relay, end to end. A real TcpServer plays the router on the loopback
// interface, with the same anonymous access the router grants relays, and the workers under test
// are the real RouterWorker and RelayWorker wired the way the relay binary wires them.
class RouterWorkerTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());
        qputenv("ASPIA_RELAY_CONFIG_FILE", (temp_dir_.path() + "/relay.conf").toLocal8Bit());

        SharedKeyPool::instance().clear();

        router_keys_ = KeyPair::create(KeyPair::Type::X25519);
        ASSERT_TRUE(router_keys_.isValid());

        std::unique_ptr<RelayTestWorker> stand_worker = std::make_unique<RelayTestWorker>();
        stand_worker_ = stand_worker.get();
        stand_workers_.add(std::move(stand_worker));
        stand_workers_.start();

        startRouterStand();
        ASSERT_NE(router_port_, 0);

        Settings settings;
        settings.setRouterAddress("127.0.0.1");
        settings.setRouterPort(router_port_);
        settings.setRouterPublicKey(router_keys_.publicKey());
        settings.setListenInterface("127.0.0.1");
        settings.setPeerAddress("127.0.0.1");
        settings.setPeerPort(pickFreePort());
        settings.setPeerIdleTimeout(Minutes(1));
        settings.setMaxPeerCount(kMaxPeerCount);
        settings.setStatisticsEnabled(false);
        ASSERT_TRUE(settings.sync());

        std::unique_ptr<RouterWorker> router_worker = std::make_unique<RouterWorker>();
        router_worker_ = router_worker.get();
        relay_workers_.add(std::move(router_worker));
        relay_workers_.add(std::make_unique<RelayWorker>());
        relay_workers_.start();
    }

    void TearDown() override
    {
        stopRouterStand();
        SharedKeyPool::instance().clear();
        qunsetenv("ASPIA_RELAY_CONFIG_FILE");
    }

    // Starts the stand and leaves the port it listens on in |router_port_|.
    void startRouterStand()
    {
        stand_worker_->invoke([this]()
        {
            server_ = new TcpServer();
            server_->setPrivateKey(router_keys_.privateKey());
            server_->setAnonymousAccess(
                ServerAuthenticator::AnonymousAccess::ENABLE, proto::router::SESSION_TYPE_RELAY);
            server_->setMaxConnectionsPerMinute(1000);

            QObject::connect(server_, &TcpServer::sig_newConnection, server_, [this]()
            {
                while (server_->hasReadyConnections())
                {
                    relay_channel_ = server_->nextReadyConnection();

                    QObject::connect(relay_channel_, &TcpChannel::sig_messageReceived,
                                     relay_channel_,
                                     [this](quint8 /* channel_id */, const QByteArray& buffer)
                    {
                        proto::router::RelayToRouter message;
                        if (!parse(buffer, &message))
                            return;

                        if (message.has_key_pool())
                        {
                            last_key_pool_ = message.key_pool();
                            key_pool_received_.signal();
                        }
                    });

                    relay_channel_->setPaused(false);
                    accepted_.signal();
                }
            });

            if (router_port_ && server_->start(router_port_))
                return;

            for (quint16 candidate = kFirstPort; candidate < kFirstPort + kPortCount; ++candidate)
            {
                if (server_->start(candidate))
                {
                    router_port_ = candidate;
                    break;
                }
            }
        });
    }

    void stopRouterStand()
    {
        stand_worker_->invoke([this]()
        {
            delete relay_channel_.data();
            delete server_.data();
        });
    }

    // Tells the relay that a key was handed to a pair of peers, the way the router does.
    void sendKeyUsed(quint32 key_id)
    {
        stand_worker_->invoke([&]()
        {
            proto::router::RouterToRelay message;
            message.mutable_key_used()->set_key_id(key_id);
            relay_channel_->send(0, serialize(message));
        });
    }

    // Fires the timer of the worker with a synthetic clock until |condition| holds. The
    // wait bounds a broken test, the condition is what the test is actually about.
    [[nodiscard]] bool fireTimerUntil(TimePoint now, const std::function<bool()>& condition)
    {
        RouterWorkerTestPeer peer(router_worker_);

        const TimePoint give_up = Clock::now() + kWaitTimeout;
        while (Clock::now() < give_up)
        {
            peer.fireTimer(now);
            if (condition())
                return true;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return false;
    }

    QTemporaryDir temp_dir_;

    KeyPair router_keys_;
    quint16 router_port_ = 0;

    WorkerManager stand_workers_;
    RelayTestWorker* stand_worker_ = nullptr;

    WorkerManager relay_workers_;
    RouterWorker* router_worker_ = nullptr;

    QPointer<TcpServer> server_;
    QPointer<TcpChannel> relay_channel_;

    proto::router::RelayKeyPool last_key_pool_;
    TestLatch accepted_;
    TestLatch key_pool_received_;
};

//--------------------------------------------------------------------------------------------------
// The relay comes up, authenticates anonymously and announces one key per free slot of its
// capacity, with the endpoint the peers are to be sent to.
TEST_F(RouterWorkerTest, RelayConnectsAndAnnouncesItsCapacity)
{
    ASSERT_TRUE(accepted_.wait(1, kWaitTimeout));
    ASSERT_TRUE(key_pool_received_.wait(1, kWaitTimeout));

    EXPECT_EQ(last_key_pool_.key_size(), int(kMaxPeerCount));
    EXPECT_EQ(last_key_pool_.peer_host(), "127.0.0.1");
    EXPECT_EQ(SharedKeyPool::instance().count(), size_t(kMaxPeerCount));

    for (int i = 0; i < last_key_pool_.key_size(); ++i)
    {
        EXPECT_EQ(last_key_pool_.key(i).type(), proto::router::RelayKey::TYPE_X25519);
        EXPECT_FALSE(last_key_pool_.key(i).public_key().empty());
        EXPECT_FALSE(last_key_pool_.key(i).iv().empty());
    }
}

//--------------------------------------------------------------------------------------------------
// A key the router reported as handed out is only good for its use budget. Once it expires the
// relay drops it and announces a replacement, so the capacity never leaks away.
TEST_F(RouterWorkerTest, UnusedKeyExpiresAndIsReplaced)
{
    ASSERT_TRUE(key_pool_received_.wait(1, kWaitTimeout));
    const quint32 key_id = last_key_pool_.key(0).key_id();

    sendKeyUsed(key_id);

    ASSERT_TRUE(fireTimerUntil(Clock::now() + kKeyUseTimeout + Seconds(1), [&]()
    {
        return key_pool_received_.count() >= 2;
    }));

    // The expired key is gone and exactly one replacement was announced.
    EXPECT_EQ(last_key_pool_.key_size(), 1);
    EXPECT_EQ(SharedKeyPool::instance().count(), size_t(kMaxPeerCount));
}

//--------------------------------------------------------------------------------------------------
// The reported key stays in the pool until its budget runs out. An early check changes nothing.
TEST_F(RouterWorkerTest, ReportedKeyLivesUntilItsBudget)
{
    ASSERT_TRUE(key_pool_received_.wait(1, kWaitTimeout));
    sendKeyUsed(last_key_pool_.key(0).key_id());

    RouterWorkerTestPeer peer(router_worker_);
    peer.fireTimer(Clock::now() + kKeyUseTimeout - Seconds(5));

    EXPECT_EQ(key_pool_received_.count(), 1);
    EXPECT_EQ(SharedKeyPool::instance().count(), size_t(kMaxPeerCount));
}

//--------------------------------------------------------------------------------------------------
// The router going away invalidates every key the relay announced to it. The relay clears its
// pool and comes back on its own once the reconnect pause passes.
TEST_F(RouterWorkerTest, RouterLossClearsThePoolAndTheRelayReturns)
{
    ASSERT_TRUE(key_pool_received_.wait(1, kWaitTimeout));

    stopRouterStand();

    // The pool empties as soon as the relay notices the loss.
    const TimePoint give_up = Clock::now() + kWaitTimeout;
    while (SharedKeyPool::instance().count() != 0 && Clock::now() < give_up)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(SharedKeyPool::instance().count(), 0u);

    startRouterStand();
    ASSERT_NE(router_port_, 0);

    ASSERT_TRUE(fireTimerUntil(Clock::now() + kReconnectTimeout + Seconds(1), [&]()
    {
        return key_pool_received_.count() >= 2;
    }));

    EXPECT_EQ(last_key_pool_.key_size(), int(kMaxPeerCount));
    EXPECT_EQ(SharedKeyPool::instance().count(), size_t(kMaxPeerCount));
}
