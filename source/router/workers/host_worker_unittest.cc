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

// How long a session that has not asked for an id is kept.
constexpr Seconds kIdentifyTimeout{ 30 };

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
        // The channel belongs to the thread of the peer worker, and so does its teardown.
        peer_worker_->invoke([this]()
        {
            delete channel_;
            channel_ = nullptr;
        });

        SharedHosts::instance().clear();
        qunsetenv("ASPIA_ROUTER_CONFIG_FILE");
        qunsetenv("ASPIA_ROUTER_DB_FILE");
    }

    // What the router stores for a host is the hash of the key the host presents.
    static QByteArray keyHash(std::string_view key)
    {
        return GenericHash::hash(GenericHash::Type::BLAKE2b512, key);
    }

    // Connects a peer that behaves like a host: anonymous authentication as a host session, then
    // one request for its existing id. Returns once the router has answered. A peer that does not
    // ask stays silent after the authentication, and the call returns as soon as it is in.
    [[nodiscard]] bool connectHost(bool ask_for_id = true)
    {
        peer_worker_->invoke([this, ask_for_id]()
        {
            ClientAuthenticator* authenticator = new ClientAuthenticator();
            authenticator->setIdentify(proto::key_exchange::IDENTIFY_ANONYMOUS);
            authenticator->setPeerPublicKey(router_keys_.publicKey());
            authenticator->setSessionType(proto::router::SESSION_TYPE_HOST);

            channel_ = new TcpChannelNG(authenticator, nullptr);

            QObject::connect(peer_worker_, &Worker::sig_tick, channel_, &TcpChannel::tick);

            QObject::connect(channel_, &TcpChannel::sig_errorOccurred, channel_,
                             [this](TcpChannel::ErrorCode /* error_code */)
            {
                disconnected_ = true;
            });

            QObject::connect(channel_, &TcpChannel::sig_authenticated, channel_, [this, ask_for_id]()
            {
                channel_->setPaused(false);
                authenticated_ = true;

                if (!ask_for_id)
                    return;

                proto::router::HostToRouter message;
                proto::router::HostIdRequest* request = message.mutable_host_id_request();
                request->set_type(proto::router::HostIdRequest::EXISTING_ID);
                request->set_key(kHostKey);
                request->set_hw_id(kHardwareId);

                channel_->send(0, serialize(message));
            });

            QObject::connect(channel_, &TcpChannel::sig_messageReceived, channel_,
                             [this](quint8 /* channel_id */, const QByteArray& buffer)
            {
                proto::router::RouterToHost message;
                if (!parse(buffer, &message) || !message.has_host_id_response())
                    return;

                assigned_host_id_ = message.host_id_response().host_id();
            });

            channel_->connectTo("127.0.0.1", host_port_);
        });

        if (!ask_for_id)
            return waitFor([this]() { return authenticated_.load(); });

        return waitFor([this]() { return assigned_host_id_.load() == host_id_; });
    }

    // Fires the timer with the synthetic |now| until the router drops the peer. The polling covers
    // the gap between the authentication of the peer and the moment the worker picks the session up.
    [[nodiscard]] bool firedUntilDisconnected(TimePoint now)
    {
        HostWorkerTestPeer timer(host_worker_);

        const TimePoint give_up = Clock::now() + kWaitTimeout;
        while (Clock::now() < give_up)
        {
            timer.fireTimer(now);
            if (disconnected_.load())
                return true;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return false;
    }

    // Polls |condition| until it holds. The wait bounds a broken test, the condition is what the
    // test is actually about.
    [[nodiscard]] static bool waitFor(const std::function<bool()>& condition)
    {
        const TimePoint give_up = Clock::now() + kWaitTimeout;
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
    std::atomic<HostId> assigned_host_id_ { kInvalidHostId };
    std::atomic<bool> authenticated_ { false };
    std::atomic<bool> disconnected_ { false };

    WorkerManager workers_;
    RouterTestWorker* peer_worker_ = nullptr;
    HostWorker* host_worker_ = nullptr;
    TcpChannel* channel_ = nullptr;
};

//--------------------------------------------------------------------------------------------------
// A connected host is announced as reachable, which is what makes a connection offer to it possible.
TEST_F(HostWorkerTest, ConnectedHostIsAnnounced)
{
    ASSERT_TRUE(connectHost());
    EXPECT_TRUE(waitFor([this]() { return SharedHosts::instance().contains(host_id_); }));
}

//--------------------------------------------------------------------------------------------------
// A host asks for its id as soon as it is authenticated. A session that never asks serves nobody,
// and anonymous access means anybody can open one, so it is not kept around.
TEST_F(HostWorkerTest, SilentSessionIsDropped)
{
    ASSERT_TRUE(connectHost(false));

    EXPECT_TRUE(firedUntilDisconnected(Clock::now() + kIdentifyTimeout + Seconds(1)));
}

//--------------------------------------------------------------------------------------------------
// A removed host stops being reachable at once. Its session lives on until it carries the remove
// command out, and while it is announced a client that knows the id would be given an offer to a
// host the administrator has already removed.
TEST_F(HostWorkerTest, RemovedHostIsNoLongerAnnounced)
{
    ASSERT_TRUE(connectHost());
    ASSERT_TRUE(waitFor([this]() { return SharedHosts::instance().contains(host_id_); }));

    // The command comes from the administrator console, which lives in another worker thread.
    peer_worker_->invoke([this]()
    {
        host_worker_->removeHost(host_id_, peer_worker_,
                                 [](HostWorker::RemoveHostResult&& /* result */) {});
    });

    EXPECT_TRUE(waitFor([this]() { return !SharedHosts::instance().contains(host_id_); }));
}
