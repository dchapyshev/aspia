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

#include <gtest/gtest.h>

#include <QList>
#include <QPair>

#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>

#include "base/shared_pointer.h"
#include "base/crypto/secure_string.h"
#include "base/net/tcp_channel_ng.h"
#include "base/net/tcp_server.h"
#include "base/peer/client_authenticator.h"
#include "base/peer/user.h"
#include "base/peer/user_list.h"
#include "base/threading/asio_event_dispatcher.h"
#include "base/threading/worker.h"
#include "proto/key_exchange.h"

namespace {

const char kUserName[] = "operator";
const char kPassword[] = "Password1234!";

// Session types are a bitmask whose meaning belongs to the role (router, host); the channel only
// checks that the one asked for is among those the user is allowed to open.
constexpr quint32 kAllowedSessionType = 1;
constexpr quint32 kDeniedSessionType = 2;

// The loopback port range the tests probe. A fixed range keeps the test independent of the ability
// to read back the port the acceptor bound to.
constexpr quint16 kFirstPort = 47921;
constexpr quint16 kPortCount = 50;

// The handshake does a full 8192-bit SRP exchange on both ends, so it is not instant. The waits are
// generous on purpose: they exist to fail a broken test instead of hanging forever, not to measure
// anything.
const Seconds kWaitTimeout{ 30 };

// A user list with a single user - the same thing the router hands to its server, minus the
// database behind it.
class TestUserList final : public UserList
{
public:
    // UserList implementation.
    User find(const QString& username) const final
    {
        if (username.compare(user_.name, Qt::CaseInsensitive) != 0)
            return User::kInvalidUser;
        return user_;
    }

    QByteArray seedKey() const final { return seed_key_; }
    void setSeedKey(const QByteArray& seed_key) final { seed_key_ = seed_key; }

    void setUser(const User& user) { user_ = user; }

private:
    User user_;
    QByteArray seed_key_ = QByteArrayLiteral("seed-key-for-tests");
};

// Counts events raised in the worker thread and lets the test thread wait for them.
class Latch
{
public:
    void signal()
    {
        std::lock_guard guard(lock_);
        ++count_;
        condition_.notify_all();
    }

    [[nodiscard]] bool wait(int expected, Seconds timeout)
    {
        std::unique_lock guard(lock_);
        return condition_.wait_for(guard, timeout, [&]() { return count_ >= expected; });
    }

    [[nodiscard]] int count()
    {
        std::lock_guard guard(lock_);
        return count_;
    }

private:
    std::mutex lock_;
    std::condition_variable condition_;
    int count_ = 0;
};

// The thread the channels live in. Every asio object of a channel belongs to the io_context of its
// own thread, and TcpServer takes the clock of its worker, so there is no way to drive this code
// from outside a worker - and no reason to: this is exactly how the router runs it.
class NetWorker final : public Worker
{
public:
    NetWorker()
        : Worker(Thread::AsioDispatcher, Seconds(1))
    {
        // Nothing
    }

    ~NetWorker() final = default;

    // Runs |work| in the worker thread and returns once it has finished.
    void invoke(const std::function<void()>& work)
    {
        std::mutex lock;
        std::condition_variable finished;
        bool done = false;

        post([&]()
        {
            work();

            std::lock_guard guard(lock);
            done = true;
            finished.notify_one();
        });

        std::unique_lock guard(lock);
        finished.wait(guard, [&]() { return done; });
    }

protected:
    // Worker implementation.
    void onStart() final { /* Nothing */ }
    void onStop() final { /* Nothing */ }
};

} // namespace

// Two real channels on the loopback interface: a real TcpServer with a real SRP handshake against a
// real socket. What is checked here is everything the sessions above take for granted - that the
// peer is who the handshake said it is, that a message arrives whole and in order, and that a peer
// which breaks the framing rules is dropped instead of being served.
class TcpChannelTest : public testing::Test
{
protected:
    void SetUp() override
    {
        std::unique_ptr<NetWorker> worker = std::make_unique<NetWorker>();
        worker_ = worker.get();

        workers_.add(std::move(worker));
        workers_.start();

        User user = User::create(QLatin1String(kUserName),
                                 SecureString(QLatin1String(kPassword)));
        user.sessions = kAllowedSessionType;
        user.flags = User::ENABLED;

        user_list_ = SharedPointer<TestUserList>(new TestUserList());
        user_list_->setUser(user);
    }

    void TearDown() override
    {
        // Everything was created in the worker thread and must go away there, before the thread
        // itself does.
        worker_->invoke([this]()
        {
            raw_socket_.reset();

            delete client_channel_;
            client_channel_ = nullptr;

            delete server_channel_;
            server_channel_ = nullptr;

            delete server_;
            server_ = nullptr;
        });
    }

    // Starts the server on the loopback interface. Returns the port it listens on, or zero.
    quint16 startServer(const QString& iface = "127.0.0.1")
    {
        quint16 port = 0;

        worker_->invoke([&]()
        {
            server_ = new TcpServer();
            server_->setUserList(user_list_);

            // The rate limiter is not what these tests are about, and every connection here comes
            // from the same address.
            server_->setMaxConnectionsPerMinute(1000);

            QObject::connect(server_, &TcpServer::sig_newConnection, server_, [this]()
            {
                while (server_->hasReadyConnections())
                {
                    server_channel_ = server_->nextReadyConnection();

                    QObject::connect(server_channel_, &TcpChannel::sig_messageReceived,
                                     server_channel_, [this](quint8 channel_id, const QByteArray& buffer)
                    {
                        server_messages_.append(qMakePair(channel_id, buffer));
                        server_message_.signal();
                    });

                    QObject::connect(server_channel_, &TcpChannel::sig_errorOccurred,
                                     server_channel_, [this](TcpChannel::ErrorCode error_code)
                    {
                        server_error_code_ = error_code;
                        server_error_.signal();
                    });

                    accepted_.signal();
                }
            });

            QObject::connect(server_, &TcpServer::sig_errorOccurred, server_,
                             [this](const QString&, const QString&)
            {
                rejected_.signal();
            });

            for (quint16 candidate = kFirstPort; candidate < kFirstPort + kPortCount; ++candidate)
            {
                if (server_->start(candidate, iface))
                {
                    port = candidate;
                    break;
                }
            }
        });

        return port;
    }

    // Opens a client channel and starts the handshake. Returns once the connect is under way, not
    // once it has completed.
    void connectClient(quint16 port, const QString& user_name, const QString& password,
                       quint32 session_type)
    {
        worker_->invoke([&]()
        {
            ClientAuthenticator* authenticator = new ClientAuthenticator();
            authenticator->setIdentify(proto::key_exchange::IDENTIFY_SRP);
            authenticator->setUserName(user_name);
            authenticator->setPassword(SecureString(password));
            authenticator->setSessionType(session_type);

            client_channel_ = new TcpChannelNG(authenticator);

            QObject::connect(client_channel_, &TcpChannel::sig_authenticated, client_channel_,
                             [this]() { client_authenticated_.signal(); });

            QObject::connect(client_channel_, &TcpChannel::sig_messageReceived, client_channel_,
                             [this](quint8 channel_id, const QByteArray& buffer)
            {
                client_messages_.append(qMakePair(channel_id, buffer));
                client_message_.signal();
            });

            QObject::connect(client_channel_, &TcpChannel::sig_errorOccurred, client_channel_,
                             [this](TcpChannel::ErrorCode error_code)
            {
                client_error_code_ = error_code;
                client_error_.signal();
            });

            client_channel_->connectTo("127.0.0.1", port, Seconds(10));
        });
    }

    // Brings both ends up and hands over a pair of authenticated channels.
    quint16 connectAuthenticatedPair()
    {
        const quint16 port = startServer();
        if (!port)
            return 0;

        connectClient(port, QLatin1String(kUserName), QLatin1String(kPassword), kAllowedSessionType);

        if (!client_authenticated_.wait(1, kWaitTimeout) || !accepted_.wait(1, kWaitTimeout))
            return 0;

        return port;
    }

    // A channel is paused after the handshake: its owner decides when it is ready to be served.
    void resumeBothEnds()
    {
        worker_->invoke([this]()
        {
            client_channel_->setPaused(false);
            server_channel_->setPaused(false);
        });
    }

    void sendFromClient(quint8 channel_id, const QByteArray& buffer)
    {
        worker_->invoke([&]() { client_channel_->send(channel_id, buffer); });
    }

    void sendFromServer(quint8 channel_id, const QByteArray& buffer)
    {
        worker_->invoke([&]() { server_channel_->send(channel_id, buffer); });
    }

    static QByteArray payload(int size, char filler)
    {
        return QByteArray(size, filler);
    }

    // A frame as it goes on the wire: an 8-byte header followed by the body.
    static QByteArray frame(quint8 type, quint32 length)
    {
        QByteArray bytes(8, '\0');
        bytes[0] = static_cast<char>(type);
        memcpy(bytes.data() + 4, &length, sizeof(length));
        return bytes;
    }

    // A plain socket to the server: a peer that sends whatever the test tells it to, including
    // things a channel would never send.
    void connectRawPeer(quint16 port)
    {
        worker_->invoke([&]()
        {
            raw_socket_ = std::make_unique<asio::ip::tcp::socket>(
                AsioEventDispatcher::ioContext());

            std::error_code error_code;
            raw_socket_->connect(
                asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port), error_code);
            ASSERT_FALSE(error_code);
        });
    }

    void sendRaw(const QByteArray& bytes)
    {
        worker_->invoke([&]()
        {
            std::error_code error_code;
            asio::write(*raw_socket_, asio::buffer(bytes.data(), bytes.size()), error_code);
            ASSERT_FALSE(error_code);
        });
    }

    // Signals |raw_closed_| once the server closes its end. The latch is shared with the handler:
    // the read is cancelled when the socket goes away in TearDown, and the cancellation handler
    // may still run after the fixture is gone.
    void watchForRawClose()
    {
        worker_->invoke([this]()
        {
            SharedPointer<Latch> latch = raw_closed_;
            raw_socket_->async_read_some(asio::buffer(raw_buffer_, sizeof(raw_buffer_)),
                                         [latch](const std::error_code& error_code, size_t)
            {
                if (error_code)
                    latch->signal();
            });
        });
    }

    // Declared before |workers_| so the worker threads are stopped before the objects they used.
    SharedPointer<TestUserList> user_list_;

    TcpServer* server_ = nullptr;
    TcpChannel* server_channel_ = nullptr;
    TcpChannel* client_channel_ = nullptr;

    QList<QPair<quint8, QByteArray>> server_messages_;
    QList<QPair<quint8, QByteArray>> client_messages_;

    TcpChannel::ErrorCode server_error_code_ = TcpChannel::ErrorCode::SUCCESS;
    TcpChannel::ErrorCode client_error_code_ = TcpChannel::ErrorCode::SUCCESS;

    std::unique_ptr<asio::ip::tcp::socket> raw_socket_;
    char raw_buffer_[64] = {};
    SharedPointer<Latch> raw_closed_ { new Latch() };

    Latch accepted_;
    Latch rejected_;
    Latch client_authenticated_;
    Latch server_message_;
    Latch client_message_;
    Latch server_error_;
    Latch client_error_;

    WorkerManager workers_;
    NetWorker* worker_ = nullptr;
};

//--------------------------------------------------------------------------------------------------
// The handshake completes and both ends learn who they are talking to. Every rule the sessions
// enforce rests on this: the identity comes from the authenticated channel, never from a message.
TEST_F(TcpChannelTest, HandshakeCompletesAndCarriesTheIdentity)
{
    ASSERT_NE(connectAuthenticatedPair(), 0);

    worker_->invoke([this]()
    {
        EXPECT_TRUE(client_channel_->isConnected());
        EXPECT_TRUE(client_channel_->isAuthenticated());

        ASSERT_NE(server_channel_, nullptr);
        EXPECT_TRUE(server_channel_->isAuthenticated());
        EXPECT_EQ(server_channel_->peerUserName(), kUserName);
        EXPECT_EQ(server_channel_->peerSessionType(), kAllowedSessionType);
        EXPECT_EQ(server_channel_->peerAddress(), "127.0.0.1");
        EXPECT_FALSE(server_channel_->peerVersion().isNull());
        EXPECT_FALSE(server_channel_->peerOsName().empty());
        EXPECT_FALSE(server_channel_->peerComputerName().empty());
    });
}

//--------------------------------------------------------------------------------------------------
// The default configuration has no listen interface, which binds the IPv6 wildcard. That socket
// must still serve IPv4 peers: the address a client was given is out of the router's hands, and
// most of them are IPv4.
TEST_F(TcpChannelTest, ServerWithoutAnInterfaceAcceptsIpv4Peers)
{
    const quint16 port = startServer(QString());
    ASSERT_NE(port, 0);

    connectClient(port, QLatin1String(kUserName), QLatin1String(kPassword), kAllowedSessionType);

    EXPECT_TRUE(client_authenticated_.wait(1, kWaitTimeout));
    EXPECT_TRUE(accepted_.wait(1, kWaitTimeout));
    EXPECT_EQ(client_error_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
// A message arrives whole, on the channel id it was sent on, in both directions.
TEST_F(TcpChannelTest, MessagesRoundTripInBothDirections)
{
    ASSERT_NE(connectAuthenticatedPair(), 0);
    resumeBothEnds();

    const QByteArray to_server = QByteArrayLiteral("request from the client");
    const QByteArray to_client = QByteArrayLiteral("reply from the server");

    sendFromClient(7, to_server);
    ASSERT_TRUE(server_message_.wait(1, kWaitTimeout));

    sendFromServer(9, to_client);
    ASSERT_TRUE(client_message_.wait(1, kWaitTimeout));

    worker_->invoke([&]()
    {
        ASSERT_EQ(server_messages_.size(), 1);
        EXPECT_EQ(server_messages_.at(0).first, 7);
        EXPECT_EQ(server_messages_.at(0).second, to_server);

        ASSERT_EQ(client_messages_.size(), 1);
        EXPECT_EQ(client_messages_.at(0).first, 9);
        EXPECT_EQ(client_messages_.at(0).second, to_client);
    });
}

//--------------------------------------------------------------------------------------------------
// The stream is a stream: what makes messages out of it is the framing. Sizes that straddle the
// socket buffer, sent back to back, must come out as the same messages in the same order.
TEST_F(TcpChannelTest, MessageBoundariesAndOrderArePreserved)
{
    ASSERT_NE(connectAuthenticatedPair(), 0);
    resumeBothEnds();

    QList<QByteArray> sent;
    for (int i = 0; i < 16; ++i)
        sent.append(payload(1 + i * 5000, static_cast<char>('a' + i)));

    worker_->invoke([&]()
    {
        for (int i = 0; i < sent.size(); ++i)
            client_channel_->send(static_cast<quint8>(i), sent.at(i));
    });

    ASSERT_TRUE(server_message_.wait(sent.size(), kWaitTimeout));

    worker_->invoke([&]()
    {
        ASSERT_EQ(server_messages_.size(), sent.size());

        for (int i = 0; i < sent.size(); ++i)
        {
            EXPECT_EQ(server_messages_.at(i).first, static_cast<quint8>(i));
            EXPECT_EQ(server_messages_.at(i).second, sent.at(i));
        }
    });
}

//--------------------------------------------------------------------------------------------------
// A message close to the limit still arrives intact - the host list of a large installation is
// megabytes of protobuf.
TEST_F(TcpChannelTest, LargeMessageSurvivesFraming)
{
    ASSERT_NE(connectAuthenticatedPair(), 0);
    resumeBothEnds();

    QByteArray large(4 * 1024 * 1024, '\0');
    for (qsizetype i = 0; i < large.size(); ++i)
        large[i] = static_cast<char>(i & 0xFF);

    sendFromClient(1, large);
    ASSERT_TRUE(server_message_.wait(1, kWaitTimeout));

    worker_->invoke([&]()
    {
        ASSERT_EQ(server_messages_.size(), 1);
        EXPECT_EQ(server_messages_.at(0).second, large);
    });
}

//--------------------------------------------------------------------------------------------------
// A wrong password never produces a channel: the key the client derives does not match, so nothing
// it sends can be read, and the connection never reaches the ready queue.
TEST_F(TcpChannelTest, WrongPasswordIsRefused)
{
    const quint16 port = startServer();
    ASSERT_NE(port, 0);

    connectClient(port, QLatin1String(kUserName), "WrongPassword!",
                  kAllowedSessionType);

    ASSERT_TRUE(client_error_.wait(1, kWaitTimeout));
    EXPECT_EQ(client_error_code_, TcpChannel::ErrorCode::ACCESS_DENIED);
    EXPECT_EQ(accepted_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
// An unknown user is refused the same way as a wrong password - the reply must not tell the two
// apart.
TEST_F(TcpChannelTest, UnknownUserIsRefusedLikeAWrongPassword)
{
    const quint16 port = startServer();
    ASSERT_NE(port, 0);

    connectClient(port, "nobody", QLatin1String(kPassword), kAllowedSessionType);

    ASSERT_TRUE(client_error_.wait(1, kWaitTimeout));
    EXPECT_EQ(client_error_code_, TcpChannel::ErrorCode::ACCESS_DENIED);
    EXPECT_EQ(accepted_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
// The credentials are right but the session type is not one this user may open. The connection is
// refused after the key exchange, with a code that tells the client it is not a password problem.
TEST_F(TcpChannelTest, DisallowedSessionTypeIsRefused)
{
    const quint16 port = startServer();
    ASSERT_NE(port, 0);

    connectClient(port, QLatin1String(kUserName), QLatin1String(kPassword), kDeniedSessionType);

    ASSERT_TRUE(client_error_.wait(1, kWaitTimeout));
    EXPECT_EQ(client_error_code_, TcpChannel::ErrorCode::SESSION_DENIED);
    EXPECT_EQ(accepted_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
// A paused channel stops delivering, it does not start losing: what arrived while it was paused is
// handed over when it resumes.
TEST_F(TcpChannelTest, PausedChannelHoldsMessagesUntilResumed)
{
    ASSERT_NE(connectAuthenticatedPair(), 0);

    // Only the sending end is resumed - the server channel stays paused, as it is right after the
    // handshake.
    worker_->invoke([this]() { client_channel_->setPaused(false); });

    const QByteArray buffer = QByteArrayLiteral("sent while the receiver was paused");
    sendFromClient(3, buffer);

    // Nothing is delivered while the channel is paused. There is no event to wait for here, so the
    // wait is expected to time out - keep it short.
    EXPECT_FALSE(server_message_.wait(1, Seconds(2)));

    worker_->invoke([this]() { server_channel_->setPaused(false); });

    ASSERT_TRUE(server_message_.wait(1, kWaitTimeout));

    worker_->invoke([&]()
    {
        ASSERT_EQ(server_messages_.size(), 1);
        EXPECT_EQ(server_messages_.at(0).second, buffer);
    });
}

//--------------------------------------------------------------------------------------------------
// A message the protocol cannot carry is refused by the sender: better a broken connection here
// than a peer that has to decide what to do with a frame it can never read.
TEST_F(TcpChannelTest, OversizedOutgoingMessageIsRefused)
{
    ASSERT_NE(connectAuthenticatedPair(), 0);
    resumeBothEnds();

    sendFromClient(1, payload(static_cast<int>(TcpChannel::kMaxMessageSize) + 1, 'x'));

    ASSERT_TRUE(client_error_.wait(1, kWaitTimeout));
    EXPECT_EQ(client_error_code_, TcpChannel::ErrorCode::INVALID_PROTOCOL);
    EXPECT_EQ(server_message_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
// An unauthenticated peer must not be able to make the server allocate: a header announcing more
// than a handshake frame can hold ends the connection before a single byte of it is read.
TEST_F(TcpChannelTest, OversizedHeaderBeforeAuthenticationIsRefused)
{
    const quint16 port = startServer();
    ASSERT_NE(port, 0);

    connectRawPeer(port);

    // type = AUTH_DATA, length = 1 MB - far above the pre-authentication cap.
    sendRaw(frame(2, 1024 * 1024));

    ASSERT_TRUE(rejected_.wait(1, kWaitTimeout));
    EXPECT_EQ(accepted_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
// The handshake is the only thing an unauthenticated peer may talk about. A payload frame before it
// completes ends the connection instead of reaching whatever is listening above.
TEST_F(TcpChannelTest, UserDataBeforeAuthenticationIsRefused)
{
    const quint16 port = startServer();
    ASSERT_NE(port, 0);

    connectRawPeer(port);

    // type = USER_DATA with a body the size of a small message.
    sendRaw(frame(3, 16) + payload(16, 'x'));

    ASSERT_TRUE(rejected_.wait(1, kWaitTimeout));
    EXPECT_EQ(accepted_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
// A peer that connects and then says nothing must not hold a slot forever: without the handshake
// deadline a handful of silent sockets would be enough to fill the pending queue of the router.
TEST_F(TcpChannelTest, SilentPeerIsDroppedAfterTheHandshakeTimeout)
{
    const quint16 port = startServer();
    ASSERT_NE(port, 0);

    connectRawPeer(port);
    watchForRawClose();

    EXPECT_TRUE(raw_closed_->wait(1, kWaitTimeout));
    EXPECT_EQ(accepted_.count(), 0);
}

//--------------------------------------------------------------------------------------------------
// A peer that goes away is reported, not waited for: the session on top of the channel has to be
// told, or it would keep a dead connection forever.
TEST_F(TcpChannelTest, PeerDisappearanceIsReported)
{
    ASSERT_NE(connectAuthenticatedPair(), 0);
    resumeBothEnds();

    worker_->invoke([this]()
    {
        delete client_channel_;
        client_channel_ = nullptr;
    });

    ASSERT_TRUE(server_error_.wait(1, kWaitTimeout));
    EXPECT_EQ(server_error_code_, TcpChannel::ErrorCode::REMOTE_HOST_CLOSED);
}
