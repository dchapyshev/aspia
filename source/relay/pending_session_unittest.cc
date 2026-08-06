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

#include "relay/pending_session.h"

#include <QtEndian>

#include <gtest/gtest.h>

#include <asio/write.hpp>

#include <memory>

#include "base/serialization.h"
#include "proto/relay_peer.h"
#include "relay/relay_test_worker.h"

namespace {

// The waits exist to fail a broken test instead of hanging forever, not to measure anything.
const Seconds kWaitTimeout{ 30 };

constexpr Seconds kHandshakeTimeout{ 5 };  // PendingSession, the budget before the handshake.
constexpr Seconds kTotalTimeout{ 30 };     // PendingSession, the budget after it.
constexpr quint32 kMaxMessageSize = 16 * 1024;

} // namespace

// A freshly accepted peer connection of the relay, on a real socket in a real worker thread. What
// is checked here is the handshake framing, the deadlines the owner drives from its clock and the
// pairing predicate.
class PendingSessionTest : public testing::Test
{
protected:
    void SetUp() override
    {
        std::unique_ptr<RelayTestWorker> worker = std::make_unique<RelayTestWorker>();
        worker_ = worker.get();

        workers_.add(std::move(worker));
        workers_.start();
    }

    void TearDown() override
    {
        worker_->invoke([this]()
        {
            session_.reset();
            peer_socket_.reset();
        });
    }

    // Creates the session over a fresh connection and starts it. The other end of the connection
    // stays with the test as |peer_socket_|.
    void createSession()
    {
        worker_->invoke([this]()
        {
            auto [outer, inner] = RelayTestWorker::createSocketPair();
            peer_socket_ = std::make_unique<asio::ip::tcp::socket>(std::move(outer));
            session_ = std::make_unique<PendingSession>(std::move(inner));

            QObject::connect(session_.get(), &PendingSession::sig_ready, session_.get(),
                             [this](const proto::relay::PeerToRelay& message)
            {
                ready_message_ = message;
                ready_.signal();
            });

            QObject::connect(session_.get(), &PendingSession::sig_failed, session_.get(),
                             [this]() { failed_.signal(); });

            session_->start();
        });
    }

    // Sends |payload| framed the way a peer frames its handshake.
    void sendFrame(const QByteArray& payload, quint32 declared_size)
    {
        worker_->invoke([&]()
        {
            QByteArray frame(sizeof(quint32), 0);
            qToBigEndian(declared_size, frame.data());
            frame.append(payload);

            std::error_code error_code;
            asio::write(*peer_socket_, asio::const_buffer(frame.constData(), frame.size()),
                        error_code);
            CHECK(!error_code);
        });
    }

    void sendHandshake(const QByteArray& payload)
    {
        sendFrame(payload, static_cast<quint32>(payload.size()));
    }

    static QByteArray handshake(quint32 key_id)
    {
        proto::relay::PeerToRelay message;
        message.set_key_id(key_id);
        message.set_public_key(std::string(32, 'p'));
        message.set_data("encrypted-secret");
        return serialize(message);
    }

    bool isExpired(TimePoint now)
    {
        bool expired = false;
        worker_->invoke([&]() { expired = session_->isExpired(now); });
        return expired;
    }

    WorkerManager workers_;
    RelayTestWorker* worker_ = nullptr;

    std::unique_ptr<asio::ip::tcp::socket> peer_socket_;
    std::unique_ptr<PendingSession> session_;

    proto::relay::PeerToRelay ready_message_;
    TestLatch ready_;
    TestLatch failed_;
};

//--------------------------------------------------------------------------------------------------
// The handshake arrives whole and parsed, and the session hands it to the owner.
TEST_F(PendingSessionTest, HandshakeIsDelivered)
{
    createSession();
    sendHandshake(handshake(42));

    ASSERT_TRUE(ready_.wait(1, kWaitTimeout));
    EXPECT_EQ(ready_message_.key_id(), 42u);
    EXPECT_EQ(ready_message_.data(), "encrypted-secret");
    EXPECT_EQ(failed_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
// The session has no timer of its own. The owner sweeps it from its clock, and before the
// handshake the budget is short.
TEST_F(PendingSessionTest, HandshakeDeadlineIsSeenByTheOwnerClock)
{
    const TimePoint before = Clock::now();
    createSession();
    const TimePoint after = Clock::now();

    EXPECT_FALSE(isExpired(before + kHandshakeTimeout - Seconds(1)));
    EXPECT_TRUE(isExpired(after + kHandshakeTimeout));
}

//--------------------------------------------------------------------------------------------------
// A peer that presented its credentials may legitimately wait for its partner, so the deadline
// moves to the total budget. It is measured from connect and not from the handshake, so the slot
// is never occupied longer than the total budget.
TEST_F(PendingSessionTest, HandshakeMovesTheDeadlineToTheTotalBudget)
{
    const TimePoint before = Clock::now();
    createSession();
    const TimePoint after = Clock::now();

    sendHandshake(handshake(42));
    ASSERT_TRUE(ready_.wait(1, kWaitTimeout));

    EXPECT_FALSE(isExpired(before + kTotalTimeout - Seconds(1)));
    EXPECT_TRUE(isExpired(after + kTotalTimeout));
}

//--------------------------------------------------------------------------------------------------
// The frame length is the first thing an anonymous peer controls.
TEST_F(PendingSessionTest, EmptyFrameFails)
{
    createSession();
    sendFrame(QByteArray(), 0);

    ASSERT_TRUE(failed_.wait(1, kWaitTimeout));
    EXPECT_EQ(ready_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
// A declared length over the cap is refused before a single byte of the body is read, so an
// anonymous peer cannot pin that much memory.
TEST_F(PendingSessionTest, OversizedFrameFails)
{
    createSession();
    sendFrame(QByteArray(), kMaxMessageSize + 1);

    ASSERT_TRUE(failed_.wait(1, kWaitTimeout));
    EXPECT_EQ(ready_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(PendingSessionTest, GarbageHandshakeFails)
{
    createSession();
    sendHandshake(QByteArray::fromHex("ffffffffffffffff"));

    ASSERT_TRUE(failed_.wait(1, kWaitTimeout));
    EXPECT_EQ(ready_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(PendingSessionTest, PeerDisconnectFails)
{
    createSession();

    worker_->invoke([this]()
    {
        std::error_code ignored_code;
        peer_socket_->close(ignored_code);
    });

    ASSERT_TRUE(failed_.wait(1, kWaitTimeout));
    EXPECT_EQ(ready_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
// Two pending sessions are a pair when they present the same key and the same secret. That match
// is the whole pairing decision of the relay.
TEST_F(PendingSessionTest, PeersMatchByKeyAndSecret)
{
    worker_->invoke([]()
    {
        auto [outer_one, inner_one] = RelayTestWorker::createSocketPair();
        auto [outer_two, inner_two] = RelayTestWorker::createSocketPair();

        PendingSession first(std::move(inner_one));
        PendingSession second(std::move(inner_two));

        // Neither side has presented credentials yet.
        EXPECT_FALSE(first.isPeerFor(second));

        first.setIdentify(42, QByteArrayLiteral("secret"));
        EXPECT_FALSE(first.isPeerFor(second));

        second.setIdentify(42, QByteArrayLiteral("secret"));
        EXPECT_TRUE(first.isPeerFor(second));
        EXPECT_TRUE(second.isPeerFor(first));

        // A session is never a pair for itself.
        EXPECT_FALSE(first.isPeerFor(first));

        second.setIdentify(43, QByteArrayLiteral("secret"));
        EXPECT_FALSE(first.isPeerFor(second));

        second.setIdentify(42, QByteArrayLiteral("other"));
        EXPECT_FALSE(first.isPeerFor(second));
    });
}
