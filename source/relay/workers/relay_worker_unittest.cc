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

#include "relay/workers/relay_worker.h"

#include <QTemporaryDir>
#include <QtEndian>

#include <gtest/gtest.h>

#include <asio/read.hpp>
#include <asio/write.hpp>

#include <thread>

#include "base/serialization.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/stream_encryptor.h"
#include "proto/relay_peer.h"
#include "relay/relay_test_worker.h"
#include "relay/session_key.h"
#include "relay/settings.h"
#include "relay/shared_key_pool.h"

namespace {

// The waits exist to fail a broken test instead of hanging forever, not to measure anything.
const Seconds kWaitTimeout{ 30 };

constexpr Seconds kPendingHandshakeTimeout{ 5 };  // PendingSession, the budget before the handshake.
constexpr Seconds kPendingTotalTimeout{ 30 };     // PendingSession, the budget after it.
constexpr Minutes kIdleTimeout{ 1 };              // What the fixture writes into the settings.

//--------------------------------------------------------------------------------------------------
// A free loopback port for the acceptor of the worker. The port is released before the worker
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
class RelayWorkerTestPeer
{
public:
    explicit RelayWorkerTestPeer(RelayWorker* worker)
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
    RelayWorker* worker_;
};

// The whole peer side of the relay, end to end. A real RelayWorker listens on the loopback
// interface, and the peers of the test do the real key derivation and encryption, so what is
// checked is exactly what a client and a host go through after they receive a connection offer.
class RelayWorkerTest : public testing::Test
{
protected:
    // The public part of a one-time key, as the connection offer carries it to the peers.
    struct OfferedKey
    {
        quint32 key_id = 0;
        QByteArray relay_public_key;
        QByteArray iv;
    };

    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());
        qputenv("ASPIA_RELAY_CONFIG_FILE", (temp_dir_.path() + "/relay.conf").toLocal8Bit());

        SharedKeyPool::instance().clear();
        peer_port_ = pickFreePort();

        Settings settings;
        settings.setListenInterface("127.0.0.1");
        settings.setPeerPort(peer_port_);
        settings.setPeerIdleTimeout(kIdleTimeout);
        settings.setStatisticsEnabled(statistics_enabled_);
        settings.setStatisticsInterval(Seconds(5));
        ASSERT_TRUE(settings.sync());

        std::unique_ptr<RelayWorker> worker = std::make_unique<RelayWorker>();
        worker_ = worker.get();

        QObject::connect(worker_, &RelayWorker::sig_ready, worker_,
                         [this]() { ready_.signal(); });
        QObject::connect(worker_, &RelayWorker::sig_sessionStarted, worker_,
                         [this]() { session_started_.signal(); });
        QObject::connect(worker_, &RelayWorker::sig_sessionFinished, worker_,
                         [this]() { session_finished_.signal(); });
        QObject::connect(worker_, &RelayWorker::sig_statistics, worker_,
                         [this](const proto::router::RelayStatistics& statistics)
        {
            last_statistics_ = statistics;
            statistics_received_.signal();
        });

        workers_.add(std::move(worker));
        workers_.start();

        ASSERT_TRUE(ready_.wait(1, kWaitTimeout));
    }

    void TearDown() override
    {
        SharedKeyPool::instance().clear();
        qunsetenv("ASPIA_RELAY_CONFIG_FILE");
    }

    // Puts a fresh one-time key into the pool, the way the router worker of the relay announces
    // it, and keeps the public part the offer would carry to the peers.
    OfferedKey announceKey()
    {
        SessionKey session_key = SessionKey::create();
        CHECK(session_key.isValid());

        OfferedKey key;
        key.relay_public_key = session_key.publicKey();
        key.iv = session_key.iv();
        key.key_id = SharedKeyPool::instance().add(std::move(session_key));
        return key;
    }

    asio::ip::tcp::socket connectPeer()
    {
        asio::ip::tcp::socket socket(io_context_);

        std::error_code error_code;
        socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), peer_port_),
                       error_code);
        CHECK(!error_code);
        return socket;
    }

    // The handshake a genuine peer sends. The session key is derived exactly the way the relay
    // derives it from its half, so the relay can open the secret.
    void sendHandshake(asio::ip::tcp::socket& socket, const OfferedKey& key,
                       const QByteArray& secret, bool legacy = false)
    {
        KeyPair peer_pair = KeyPair::create(KeyPair::Type::X25519);
        CHECK(peer_pair.isValid());

        SecureByteArray shared_secret = peer_pair.sessionKey(key.relay_public_key);
        CHECK(!shared_secret.isEmpty());

        SecureByteArray session_key;
        std::unique_ptr<StreamEncryptor> encryptor;
        if (legacy)
        {
            session_key = SecureByteArray(
                GenericHash::hash(GenericHash::Type::BLAKE2s256, shared_secret.toByteArray()));
            encryptor = StreamEncryptor::createForChaCha20Poly1305(session_key, key.iv);
        }
        else
        {
            GenericHash hash(GenericHash::Type::SHA256);
            hash.addData(shared_secret);
            hash.addData(key.relay_public_key);
            hash.addData(peer_pair.publicKey());
            session_key = SecureByteArray(hash.result());
            encryptor = StreamEncryptor::createForAes256Gcm(session_key, key.iv);
        }
        CHECK(encryptor);

        QByteArray ciphertext(encryptor->encryptedDataSize(secret.size()), 0);
        CHECK(encryptor->encrypt(secret.constData(), secret.size(), ciphertext.data()));

        proto::relay::PeerToRelay message;
        message.set_key_id(key.key_id);
        if (!legacy)
            message.set_encryption(proto::relay::PeerToRelay::ENCRYPTION_AES256_GCM);
        message.set_public_key(peer_pair.publicKey().toStdString());
        message.set_data(ciphertext.toStdString());

        sendFrame(socket, serialize(message));
    }

    void sendFrame(asio::ip::tcp::socket& socket, const QByteArray& payload)
    {
        QByteArray frame(sizeof(quint32), 0);
        qToBigEndian(static_cast<quint32>(payload.size()), frame.data());
        frame.append(payload);

        std::error_code error_code;
        asio::write(socket, asio::const_buffer(frame.constData(), frame.size()), error_code);
        CHECK(!error_code);
    }

    static QByteArray secret(const std::string& random_data = "random")
    {
        proto::relay::PeerToRelay::Secret message;
        message.set_random_data(random_data);
        message.set_client_address("203.0.113.5");
        message.set_client_user_name("operator");
        message.set_host_address("198.51.100.7");
        message.set_host_id(100);
        return serialize(message);
    }

    // Fires the timer with the synthetic |now| until the relay closes |socket|. The polling covers
    // the gap between the connect of the peer and the moment the worker picks the connection up.
    [[nodiscard]] bool firedUntilClosed(asio::ip::tcp::socket& socket, TimePoint now)
    {
        RelayWorkerTestPeer timer(worker_);

        std::error_code error_code;
        socket.non_blocking(true, error_code);
        CHECK(!error_code);

        const TimePoint give_up = Clock::now() + kWaitTimeout;
        while (Clock::now() < give_up)
        {
            timer.fireTimer(now);

            char byte = 0;
            std::error_code read_code;
            socket.read_some(asio::buffer(&byte, 1), read_code);
            if (read_code && read_code != asio::error::would_block)
                return true;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return false;
    }

    // Reads |size| bytes. Returns what arrived before the relay closed the connection, so an
    // expected refusal reads back as an empty array.
    static QByteArray receive(asio::ip::tcp::socket& socket, qsizetype size)
    {
        QByteArray buffer(size, 0);

        std::error_code error_code;
        const size_t read = asio::read(socket, asio::buffer(buffer.data(), buffer.size()),
                                       error_code);
        buffer.resize(static_cast<qsizetype>(read));
        return buffer;
    }

    static void send(asio::ip::tcp::socket& socket, const QByteArray& data)
    {
        std::error_code error_code;
        asio::write(socket, asio::const_buffer(data.constData(), data.size()), error_code);
        CHECK(!error_code);
    }

    QTemporaryDir temp_dir_;
    quint16 peer_port_ = 0;
    bool statistics_enabled_ = false;

    WorkerManager workers_;
    RelayWorker* worker_ = nullptr;

    asio::io_context io_context_;

    proto::router::RelayStatistics last_statistics_;
    TestLatch ready_;
    TestLatch session_started_;
    TestLatch session_finished_;
    TestLatch statistics_received_;
};

// The same stand with the statistics reporting turned on.
class RelayWorkerStatisticsTest : public RelayWorkerTest
{
protected:
    void SetUp() override
    {
        statistics_enabled_ = true;
        RelayWorkerTest::SetUp();
    }
};

//--------------------------------------------------------------------------------------------------
// The happy path of the whole relay. Two peers present the same key and the same secret, the relay
// pairs them, spends the key and pumps their bytes.
TEST_F(RelayWorkerTest, PeersWithTheSameSecretArePaired)
{
    const OfferedKey key = announceKey();
    const QByteArray shared_secret = secret();

    asio::ip::tcp::socket client = connectPeer();
    sendHandshake(client, key, shared_secret);

    asio::ip::tcp::socket host = connectPeer();
    sendHandshake(host, key, shared_secret);

    ASSERT_TRUE(session_started_.wait(1, kWaitTimeout));
    EXPECT_EQ(SharedKeyPool::instance().count(), 0u);

    const QByteArray from_client = QByteArrayLiteral("client-to-host");
    send(client, from_client);
    EXPECT_EQ(receive(host, from_client.size()), from_client);

    const QByteArray from_host = QByteArrayLiteral("host-to-client!!");
    send(host, from_host);
    EXPECT_EQ(receive(client, from_host.size()), from_host);
}

//--------------------------------------------------------------------------------------------------
// The legacy derivation, as peers that predate AES support use it.
TEST_F(RelayWorkerTest, LegacyPeersArePaired)
{
    const OfferedKey key = announceKey();
    const QByteArray shared_secret = secret();

    asio::ip::tcp::socket client = connectPeer();
    sendHandshake(client, key, shared_secret, true);

    asio::ip::tcp::socket host = connectPeer();
    sendHandshake(host, key, shared_secret, true);

    ASSERT_TRUE(session_started_.wait(1, kWaitTimeout));

    const QByteArray data = QByteArrayLiteral("legacy-bytes");
    send(client, data);
    EXPECT_EQ(receive(host, data.size()), data);
}

//--------------------------------------------------------------------------------------------------
// A key the relay never announced buys nothing. The connection is dropped and no session appears.
TEST_F(RelayWorkerTest, UnknownKeyIsRefused)
{
    SessionKey foreign_key = SessionKey::create();
    ASSERT_TRUE(foreign_key.isValid());

    OfferedKey key;
    key.key_id = 777;
    key.relay_public_key = foreign_key.publicKey();
    key.iv = foreign_key.iv();

    asio::ip::tcp::socket peer = connectPeer();
    sendHandshake(peer, key, secret());

    EXPECT_TRUE(receive(peer, 1).isEmpty());
    EXPECT_EQ(session_started_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
// A secret the key of the relay cannot open is refused. This is what a stolen key id without the
// matching key material comes down to.
TEST_F(RelayWorkerTest, UndecryptableSecretIsRefused)
{
    const OfferedKey announced = announceKey();

    // The peer derives from its own key pair instead of the announced half, so the ciphertext
    // does not open.
    SessionKey foreign_key = SessionKey::create();
    ASSERT_TRUE(foreign_key.isValid());

    OfferedKey forged;
    forged.key_id = announced.key_id;
    forged.relay_public_key = foreign_key.publicKey();
    forged.iv = foreign_key.iv();

    asio::ip::tcp::socket peer = connectPeer();
    sendHandshake(peer, forged, secret());

    EXPECT_TRUE(receive(peer, 1).isEmpty());
    EXPECT_EQ(session_started_.count(), 0);

    // The key of a refused peer stays in the pool for its legitimate owner.
    EXPECT_EQ(SharedKeyPool::instance().count(), 1u);
}

//--------------------------------------------------------------------------------------------------
// Peers with different secrets are never paired, whatever key they share.
TEST_F(RelayWorkerTest, DifferentSecretsAreNotPaired)
{
    const OfferedKey key = announceKey();

    asio::ip::tcp::socket client = connectPeer();
    sendHandshake(client, key, secret("one"));

    asio::ip::tcp::socket host = connectPeer();
    sendHandshake(host, key, secret("two"));

    EXPECT_FALSE(session_started_.wait(1, Seconds(2)));
}

//--------------------------------------------------------------------------------------------------
// The key dies with the pairing it served. A second pair presenting it is refused.
TEST_F(RelayWorkerTest, KeyIsOneTime)
{
    const OfferedKey key = announceKey();
    const QByteArray shared_secret = secret();

    asio::ip::tcp::socket client = connectPeer();
    sendHandshake(client, key, shared_secret);
    asio::ip::tcp::socket host = connectPeer();
    sendHandshake(host, key, shared_secret);
    ASSERT_TRUE(session_started_.wait(1, kWaitTimeout));

    asio::ip::tcp::socket second_client = connectPeer();
    sendHandshake(second_client, key, shared_secret);

    EXPECT_TRUE(receive(second_client, 1).isEmpty());
    EXPECT_EQ(session_started_.count(), 1);
}

//--------------------------------------------------------------------------------------------------
// Either peer going away finishes the session, and the worker announces that.
TEST_F(RelayWorkerTest, PeerDisconnectFinishesTheSession)
{
    const OfferedKey key = announceKey();
    const QByteArray shared_secret = secret();

    asio::ip::tcp::socket client = connectPeer();
    sendHandshake(client, key, shared_secret);
    asio::ip::tcp::socket host = connectPeer();
    sendHandshake(host, key, shared_secret);
    ASSERT_TRUE(session_started_.wait(1, kWaitTimeout));

    std::error_code ignored_code;
    client.close(ignored_code);

    ASSERT_TRUE(session_finished_.wait(1, kWaitTimeout));
}

//--------------------------------------------------------------------------------------------------
// A peer that connected and never presented its credentials is dropped once the handshake budget
// passes. The slot it occupies is what the flood guard protects.
TEST_F(RelayWorkerTest, SilentPeerIsDroppedAfterTheHandshakeBudget)
{
    asio::ip::tcp::socket peer = connectPeer();

    EXPECT_TRUE(firedUntilClosed(peer, Clock::now() + kPendingHandshakeTimeout + Seconds(1)));
    EXPECT_EQ(session_started_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
// A peer that presented its credentials may legitimately wait for its partner well past the
// handshake budget, and the partner still finds it.
TEST_F(RelayWorkerTest, HandshakedPeerWaitsBeyondTheHandshakeBudget)
{
    const OfferedKey key = announceKey();
    const QByteArray shared_secret = secret();

    asio::ip::tcp::socket client = connectPeer();
    sendHandshake(client, key, shared_secret);

    // Ticks past the handshake budget. The handshake is processed well within this loop, so the
    // later ticks genuinely test a handshaked session against the passed budget.
    RelayWorkerTestPeer timer(worker_);
    const TimePoint past_handshake_budget = Clock::now() + kPendingHandshakeTimeout + Seconds(1);
    for (int i = 0; i < 20; ++i)
    {
        timer.fireTimer(past_handshake_budget);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    asio::ip::tcp::socket host = connectPeer();
    sendHandshake(host, key, shared_secret);

    ASSERT_TRUE(session_started_.wait(1, kWaitTimeout));
}

//--------------------------------------------------------------------------------------------------
// The wait for a partner is not open-ended. A handshaked peer nobody came for is dropped once the
// total budget, measured from connect, runs out.
TEST_F(RelayWorkerTest, LonePeerIsDroppedAfterTheTotalBudget)
{
    const OfferedKey key = announceKey();

    asio::ip::tcp::socket peer = connectPeer();
    sendHandshake(peer, key, secret());

    EXPECT_TRUE(firedUntilClosed(peer, Clock::now() + kPendingTotalTimeout + Seconds(1)));
    EXPECT_EQ(session_started_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
// A session whose peers stopped talking is closed once the idle budget from the settings passes,
// and the worker announces the finish.
TEST_F(RelayWorkerTest, IdleSessionIsClosed)
{
    const OfferedKey key = announceKey();
    const QByteArray shared_secret = secret();

    asio::ip::tcp::socket client = connectPeer();
    sendHandshake(client, key, shared_secret);
    asio::ip::tcp::socket host = connectPeer();
    sendHandshake(host, key, shared_secret);
    ASSERT_TRUE(session_started_.wait(1, kWaitTimeout));

    RelayWorkerTestPeer timer(worker_);
    timer.fireTimer(Clock::now() + kIdleTimeout + Seconds(2));

    ASSERT_TRUE(session_finished_.wait(1, kWaitTimeout));
    EXPECT_TRUE(receive(client, 1).isEmpty());
    EXPECT_TRUE(receive(host, 1).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The report the relay sends to the router carries every active session with the identity from
// its secret. This is what the administrator sees in the console.
TEST_F(RelayWorkerStatisticsTest, StatisticsReportTheActiveSessions)
{
    const OfferedKey key = announceKey();
    const QByteArray shared_secret = secret();

    asio::ip::tcp::socket client = connectPeer();
    sendHandshake(client, key, shared_secret);
    asio::ip::tcp::socket host = connectPeer();
    sendHandshake(host, key, shared_secret);
    ASSERT_TRUE(session_started_.wait(1, kWaitTimeout));

    RelayWorkerTestPeer timer(worker_);
    timer.fireTimer(Clock::now() + Seconds(6));

    ASSERT_TRUE(statistics_received_.wait(1, kWaitTimeout));
    ASSERT_EQ(last_statistics_.peer_size(), 1);

    const proto::router::Peer& peer = last_statistics_.peer(0);
    EXPECT_EQ(peer.status(), proto::router::Peer::STATUS_ACTIVE);
    EXPECT_EQ(peer.client_address(), "203.0.113.5");
    EXPECT_EQ(peer.client_user_name(), "operator");
    EXPECT_EQ(peer.host_address(), "198.51.100.7");
    EXPECT_EQ(peer.host_id(), 100u);
}
