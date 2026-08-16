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

#include "host/router_manager.h"

#include <QPointer>
#include <QSettings>
#include <QTemporaryDir>
#include <QtEndian>

#include <gtest/gtest.h>

#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "base/serialization.h"
#include "base/xml_settings.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/random.h"
#include "base/net/tcp_server.h"
#include "base/peer/client_authenticator.h"
#include "base/peer/relay_peer.h"
#include "base/threading/asio_event_dispatcher.h"
#include "base/threading/worker.h"
#include "build/build_config.h"
#include "host/database.h"
#include "host/host_storage.h"
#include "proto/key_exchange.h"
#include "proto/peer.h"
#include "proto/router.h"
#include "proto/router_constants.h"
#include "proto/router_host.h"

namespace {

// The waits exist to fail a broken test instead of hanging forever, not to measure anything.
const Seconds kWaitTimeout{ 30 };

// RouterManager, the pause between connect attempts.
constexpr Seconds kReconnectTimeout{ 10 };

// What the fixture writes into the database for the one-time password.
constexpr MilliSeconds kPasswordExpire{ 60 * 1000 };

const HostId kHostId = 1234;
const char kHostKey[] = "the-key-the-router-issued";

// The loopback port range the stand probes for its listener.
constexpr quint16 kFirstPort = 48111;
constexpr quint16 kPortCount = 50;

} // namespace

// A real worker thread of the host: it carries the database connection of its own thread and the
// clock that drives the channels created in it. What is put under test is created, driven and
// destroyed exactly where the host does it.
class HostTestWorker final : public Worker
{
public:
    // An empty |file_path| runs the worker without a database, for the peers of a stand.
    explicit HostTestWorker(const QString& file_path = QString())
        : Worker(Thread::AsioDispatcher, Seconds(1)),
          file_path_(file_path)
    {
        // Nothing
    }

    ~HostTestWorker() final = default;

    // Runs |work| in the worker thread and returns once it has finished. The caller is blocked
    // meanwhile, which keeps the test itself sequential.
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

    // The connection of the worker thread. Only valid inside invoke().
    Database& database() { return *database_; }

protected:
    // Worker implementation.
    void onStart() final
    {
        if (file_path_.isEmpty())
            return;

        database_ = Database::openForTesting(file_path_);
        CHECK(database_);
    }

    void onStop() final
    {
        database_.reset();
    }

private:
    const QString file_path_;
    std::unique_ptr<Database> database_;

    Q_DISABLE_COPY_MOVE(HostTestWorker)
};

// Reaches into the manager from its own thread: drives the timer with a synthetic clock (the
// tests cannot wait the real timeouts out).
class RouterManagerTestPeer
{
public:
    explicit RouterManagerTestPeer(HostTestWorker* worker, RouterManager* manager)
        : worker_(worker),
          manager_(manager)
    {
        // Nothing
    }

    void fireTimer(TimePoint now)
    {
        worker_->invoke([&]() { manager_->onTimer(now); });
    }

private:
    HostTestWorker* worker_;
    RouterManager* manager_;
};

// The router channel of the host end to end. A real TcpServer plays the router on the loopback
// interface, with the same anonymous access the router grants hosts, and the manager under test is
// the real RouterManager against a temporary database and storage.
class RouterManagerTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());

        // The host storage is opened anew on every access and lives in the scope of the machine.
        // Moving the scope into the temporary directory keeps the test off the real storage without
        // any test-only entry in the production code. Both scopes are redirected because the host
        // picks one by platform.
        QSettings::setPath(XmlSettings::format(), QSettings::SystemScope, temp_dir_.path());
        QSettings::setPath(XmlSettings::format(), QSettings::UserScope, temp_dir_.path());

        router_keys_ = KeyPair::create(KeyPair::Type::X25519);
        ASSERT_TRUE(router_keys_.isValid());

        std::unique_ptr<HostTestWorker> stand_worker = std::make_unique<HostTestWorker>();
        stand_worker_ = stand_worker.get();
        workers_.add(std::move(stand_worker));

        std::unique_ptr<HostTestWorker> host_worker =
            std::make_unique<HostTestWorker>(temp_dir_.path() + "/host.db3");
        host_worker_ = host_worker.get();
        workers_.add(std::move(host_worker));

        workers_.start();

        startRouterStand();
        ASSERT_NE(router_port_, 0);

        startFakeRelay();
        ASSERT_NE(relay_port_, 0);
    }

    void TearDown() override
    {
        host_worker_->invoke([this]() { delete manager_.data(); });

        stopRouterStand();

        stand_worker_->invoke([this]()
        {
            relay_acceptor_.reset();
            relay_peers_.clear();
        });
    }

    // Starts the stand and leaves the port it listens on in |router_port_|.
    void startRouterStand()
    {
        stand_worker_->invoke([this]()
        {
            server_ = new TcpServer();
            server_->setPrivateKey(router_keys_.privateKey());
            server_->setAnonymousAccess(
                ServerAuthenticator::AnonymousAccess::ENABLE, proto::router::SESSION_TYPE_HOST);
            server_->setMaxConnectionsPerMinute(1000);

            QObject::connect(server_, &TcpServer::sig_newConnection, server_, [this]()
            {
                while (server_->hasReadyConnections())
                {
                    host_channel_ = server_->nextReadyConnection();

                    QObject::connect(host_channel_, &TcpChannel::sig_messageReceived,
                                     host_channel_,
                                     [this](quint8 /* channel_id */, const QByteArray& buffer)
                    {
                        proto::router::HostToRouter message;
                        if (!parse(buffer, &message))
                            return;

                        if (!message.has_host_id_request())
                            return;

                        last_request_ = message.host_id_request();
                        ++requests_received_;
                    });

                    host_channel_->setPaused(false);
                    ++accepted_;
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
            delete host_channel_.data();
            delete server_.data();
        });
    }

    // One peer the fake relay accepted: the authentication frame it must send first, then the
    // socket the bridge pumps. Shared pointers keep a peer alive for its pending handlers.
    struct RelayPeerSocket
    {
        explicit RelayPeerSocket(asio::ip::tcp::socket socket)
            : socket(std::move(socket))
        {
            // Nothing
        }

        asio::ip::tcp::socket socket;
        quint8 size_buffer[4];
        std::vector<char> auth_message;
        std::array<char, 8192> data;
        bool ready = false;
        bool paired = false;
    };
    using RelayPeerSocketPtr = std::shared_ptr<RelayPeerSocket>;

    // The relay end of the offers, honest enough for a complete handshake: every peer sends the
    // authentication frame of the relay protocol, and two ready peers are bridged byte for byte.
    void startFakeRelay()
    {
        relay_key_pair_ = KeyPair::create(KeyPair::Type::X25519);
        ASSERT_TRUE(relay_key_pair_.isValid());
        relay_iv_ = Random::byteArray(12).toStdString();
        relay_secret_ = Random::byteArray(16).toStdString();

        stand_worker_->invoke([this]()
        {
            relay_acceptor_ =
                std::make_unique<asio::ip::tcp::acceptor>(AsioEventDispatcher::ioContext());

            const asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 0);
            std::error_code error_code;

            relay_acceptor_->open(endpoint.protocol(), error_code);
            ASSERT_FALSE(error_code) << error_code.message();
            relay_acceptor_->bind(endpoint, error_code);
            ASSERT_FALSE(error_code) << error_code.message();
            relay_acceptor_->listen(asio::socket_base::max_listen_connections, error_code);
            ASSERT_FALSE(error_code) << error_code.message();

            relay_port_ = relay_acceptor_->local_endpoint().port();
            acceptNextRelayConnection();
        });
    }

    // Keeps one accept in flight. Runs in the stand thread.
    void acceptNextRelayConnection()
    {
        relay_acceptor_->async_accept(
            [this](const std::error_code& error_code, asio::ip::tcp::socket socket)
        {
            if (error_code)
                return;

            ++relay_accepted_;

            relay_peers_.push_back(std::make_shared<RelayPeerSocket>(std::move(socket)));
            readPeerAuthentication(relay_peers_.back());

            acceptNextRelayConnection();
        });
    }

    // The size-prefixed frame every peer opens with. The real relay checks the secret inside; the
    // bridge only needs the frame out of the way of the session bytes.
    void readPeerAuthentication(const RelayPeerSocketPtr& peer)
    {
        asio::async_read(peer->socket, asio::buffer(peer->size_buffer),
            [this, peer](const std::error_code& error_code, size_t /* bytes */)
        {
            if (error_code)
                return;

            const quint32 size = qFromBigEndian<quint32>(peer->size_buffer);
            if (!size || size > 64 * 1024)
                return;

            peer->auth_message.resize(size);
            asio::async_read(peer->socket, asio::buffer(peer->auth_message),
                [this, peer](const std::error_code& error_code, size_t /* bytes */)
            {
                if (error_code)
                    return;

                peer->ready = true;
                pairRelayPeers();
            });
        });
    }

    void pairRelayPeers()
    {
        RelayPeerSocketPtr first;
        for (const RelayPeerSocketPtr& peer : relay_peers_)
        {
            if (!peer->ready || peer->paired)
                continue;

            if (!first)
            {
                first = peer;
                continue;
            }

            first->paired = peer->paired = true;
            forwardRelayData(first, peer);
            forwardRelayData(peer, first);
            return;
        }
    }

    void forwardRelayData(const RelayPeerSocketPtr& from, const RelayPeerSocketPtr& to)
    {
        from->socket.async_read_some(asio::buffer(from->data),
            [this, from, to](const std::error_code& error_code, size_t bytes)
        {
            if (error_code)
                return;

            asio::async_write(to->socket, asio::buffer(from->data.data(), bytes),
                [this, from, to](const std::error_code& error_code, size_t /* bytes */)
            {
                if (error_code)
                    return;

                forwardRelayData(from, to);
            });
        });
    }

    // The credentials of the fake relay, the same for both peers of a brokered connection.
    void fillRelayCredentials(proto::router::RelayCredentials* relay)
    {
        relay->set_host("127.0.0.1");
        relay->set_port(relay_port_);
        relay->set_secret(relay_secret_);

        proto::router::RelayKey* key = relay->mutable_key();
        key->set_key_id(1);
        key->set_type(proto::router::RelayKey::TYPE_X25519);
        key->set_encryption(proto::router::RelayKey::ENCRYPTION_CHACHA20_POLY1305);
        key->set_public_key(relay_key_pair_.publicKey().toStdString());
        key->set_iv(relay_iv_);
    }

    // Sends the connection offer the way the router does, pointing the manager at the fake relay.
    void sendConnectionOffer()
    {
        stand_worker_->invoke([&]()
        {
            proto::router::RouterToHost message;
            proto::router::ConnectionOffer* offer = message.mutable_connection_offer();
            offer->set_error_code(proto::router::kErrorOk);

            fillRelayCredentials(offer->mutable_relay());

            host_channel_->send(0, serialize(message));
        });
    }

    // The client end of a brokered connection: RelayPeer and the anonymous authenticator, built
    // from the offer the way the network worker of the client builds them.
    struct ClientPeer
    {
        QPointer<RelayPeer> peer;
        QPointer<TcpChannel> channel;
        std::atomic<bool> ready { false };
        std::atomic<bool> failed { false };
        std::atomic<int> messages_received { 0 };
        std::mutex lock;
        QByteArray last_message;
    };

    // Starts the connection of |client| to the fake relay, authenticating against the host by
    // |user_name| and |password| over SRP. |session_type| is what the client came for.
    void connectClientPeer(ClientPeer* client, const QString& user_name, const SecureString& password,
                           quint32 session_type)
    {
        stand_worker_->invoke([&]()
        {
            ClientAuthenticator* authenticator = new ClientAuthenticator();
            authenticator->setIdentify(proto::key_exchange::IDENTIFY_SRP);
            authenticator->setUserName(user_name);
            authenticator->setPassword(password);
            authenticator->setSessionType(session_type);

            client->peer = new RelayPeer(authenticator, nullptr);

            QObject::connect(client->peer, &RelayPeer::sig_connectionError, client->peer,
                             [client](std::optional<TcpChannel::ErrorCode> /* error_code */)
            {
                client->failed = true;
            });

            QObject::connect(client->peer, &RelayPeer::sig_connectionReady, client->peer,
                             [client]()
            {
                client->channel = client->peer->takeChannel();

                QObject::connect(client->channel, &TcpChannel::sig_messageReceived,
                                 client->channel,
                                 [client](quint8 /* channel_id */, const QByteArray& buffer)
                {
                    std::lock_guard guard(client->lock);
                    client->last_message = buffer;
                    ++client->messages_received;
                });

                client->channel->setPaused(false);
                client->ready = true;
            });

            proto::router::ConnectionOffer offer;
            offer.set_error_code(proto::router::kErrorOk);
            fillRelayCredentials(offer.mutable_relay());

            client->peer->start(offer);
        });
    }

    // The teardown of the client end, in the thread it lives in.
    void closeClientPeer(ClientPeer* client)
    {
        stand_worker_->invoke([client]()
        {
            delete client->channel.data();
            delete client->peer.data();
        });
    }

    // Answers the pending id request the way the router does.
    void sendIdResponse(std::string_view error_code, HostId host_id, std::string_view key)
    {
        stand_worker_->invoke([&]()
        {
            proto::router::RouterToHost message;
            proto::router::HostIdResponse* response = message.mutable_host_id_response();
            response->set_error_code(std::string(error_code));

            if (host_id != kInvalidHostId)
                response->set_host_id(host_id);
            if (!key.empty())
                response->set_key(std::string(key));

            host_channel_->send(0, serialize(message));
        });
    }

    // Creates the manager in its worker thread, pointed at the stand, with the one-time password
    // switched by |one_time_password|.
    void startManager(bool one_time_password = false)
    {
        host_worker_->invoke([this, one_time_password]()
        {
            Database& database = host_worker_->database();

            // The default port must differ from the stand port, or toString() would omit the
            // port and the read would put the standard one back.
            Address address(DEFAULT_ROUTER_HOST_TCP_PORT);
            address.setHost("127.0.0.1");
            address.setPort(router_port_);

            ASSERT_TRUE(database.setRouterEnabled(true));
            ASSERT_TRUE(database.setRouterAddress(address));
            ASSERT_TRUE(database.setRouterPublicKey(router_keys_.publicKey()));
            ASSERT_TRUE(database.setOneTimePassword(one_time_password));
            ASSERT_TRUE(database.setOneTimePasswordExpire(kPasswordExpire));

            manager_ = new RouterManager(database);

            QObject::connect(manager_, &RouterManager::sig_credentialsChanged, manager_,
                             [this](HostId host_id, const SecureString& password)
            {
                credentials_host_id_ = host_id;

                std::lock_guard guard(password_lock_);
                last_password_ = password;
                ++credentials_received_;
            });

            manager_->start();
        });
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

    // The password the manager reported last. Guarded: the signal fires in the host worker thread.
    SecureString lastPassword()
    {
        std::lock_guard guard(password_lock_);
        return last_password_;
    }

    QTemporaryDir temp_dir_;

    KeyPair router_keys_;
    quint16 router_port_ = 0;

    WorkerManager workers_;
    HostTestWorker* stand_worker_ = nullptr;
    HostTestWorker* host_worker_ = nullptr;

    std::unique_ptr<asio::ip::tcp::acceptor> relay_acceptor_;
    std::vector<RelayPeerSocketPtr> relay_peers_;
    KeyPair relay_key_pair_;
    std::string relay_iv_;
    std::string relay_secret_;
    quint16 relay_port_ = 0;
    std::atomic<int> relay_accepted_ { 0 };

    QPointer<TcpServer> server_;
    QPointer<TcpChannel> host_channel_;
    proto::router::HostIdRequest last_request_;
    std::atomic<int> accepted_ { 0 };
    std::atomic<int> requests_received_ { 0 };

    QPointer<RouterManager> manager_;
    std::atomic<HostId> credentials_host_id_ { kInvalidHostId };
    std::atomic<int> credentials_received_ { 0 };

    std::mutex password_lock_;
    SecureString last_password_;
};

//--------------------------------------------------------------------------------------------------
// A host that has no key yet asks for a new id, reports its hardware id, and keeps what the router
// issues: the key, the id, and the credentials it announces to the user.
TEST_F(RouterManagerTest, NewHostAsksForANewIdAndKeepsTheIssuedKey)
{
    startManager();

    ASSERT_TRUE(waitFor([this]() { return requests_received_.load() >= 1; }));
    EXPECT_EQ(last_request_.type(), proto::router::HostIdRequest::NEW_ID);
    EXPECT_FALSE(last_request_.hw_id().empty());

    sendIdResponse(proto::router::kErrorOk, kHostId, kHostKey);

    ASSERT_TRUE(waitFor([this]() { return credentials_host_id_.load() == kHostId; }));

    QByteArray host_key;
    host_worker_->invoke([&]() { host_key = host_worker_->database().hostKey(); });
    EXPECT_EQ(host_key, QByteArray(kHostKey));

    HostStorage storage;
    EXPECT_EQ(storage.lastHostId(), kHostId);
}

//--------------------------------------------------------------------------------------------------
// A host with a stored key presents it. A router that does not know the key sends the host down
// the new-id path, so a wiped router database heals by itself.
TEST_F(RouterManagerTest, UnknownKeyIsResetAndANewIdIsRequested)
{
    host_worker_->invoke([this]()
    {
        ASSERT_TRUE(host_worker_->database().setHostKey(QByteArrayLiteral("the-key-the-router-forgot")));
    });

    startManager();

    ASSERT_TRUE(waitFor([this]() { return requests_received_.load() >= 1; }));
    EXPECT_EQ(last_request_.type(), proto::router::HostIdRequest::EXISTING_ID);
    EXPECT_FALSE(last_request_.key().empty());

    sendIdResponse(proto::router::kErrorNotFound, kInvalidHostId, std::string_view());

    ASSERT_TRUE(waitFor([this]() { return requests_received_.load() >= 2; }));
    EXPECT_EQ(last_request_.type(), proto::router::HostIdRequest::NEW_ID);
}

//--------------------------------------------------------------------------------------------------
// The router going away drops the credentials, and the host comes back on its own once the
// reconnect pause passes.
TEST_F(RouterManagerTest, ReconnectsAfterTheRouterIsLost)
{
    startManager();

    ASSERT_TRUE(waitFor([this]() { return requests_received_.load() >= 1; }));
    sendIdResponse(proto::router::kErrorOk, kHostId, kHostKey);
    ASSERT_TRUE(waitFor([this]() { return credentials_host_id_.load() == kHostId; }));

    stopRouterStand();

    // The loss is reported as soon as the manager notices it.
    ASSERT_TRUE(waitFor([this]() { return credentials_host_id_.load() == kInvalidHostId; }));

    startRouterStand();
    ASSERT_NE(router_port_, 0);

    // The real pause is ten seconds; the synthetic clock crosses it at once.
    RouterManagerTestPeer timer(host_worker_, manager_);
    const TimePoint after_pause = Clock::now() + kReconnectTimeout + Seconds(1);

    ASSERT_TRUE(waitFor([&]()
    {
        timer.fireTimer(after_pause);
        return requests_received_.load() >= 2;
    }));

    // The host knows its key now, so it comes back with it.
    EXPECT_EQ(last_request_.type(), proto::router::HostIdRequest::EXISTING_ID);
}

//--------------------------------------------------------------------------------------------------
// The one-time password lives until its expiration and is replaced then. What the user sees stays
// in step with what the authenticator accepts.
TEST_F(RouterManagerTest, OneTimePasswordRotatesWhenItExpires)
{
    startManager(true);

    ASSERT_TRUE(waitFor([this]() { return requests_received_.load() >= 1; }));
    sendIdResponse(proto::router::kErrorOk, kHostId, kHostKey);
    ASSERT_TRUE(waitFor([this]() { return credentials_host_id_.load() == kHostId; }));

    const SecureString first_password = lastPassword();
    ASSERT_FALSE(first_password.isEmpty());

    const int seen = credentials_received_.load();

    RouterManagerTestPeer timer(host_worker_, manager_);
    timer.fireTimer(Clock::now() + DurationCast<Seconds>(kPasswordExpire) + Seconds(1));

    ASSERT_TRUE(waitFor([&]() { return credentials_received_.load() > seen; }));
    EXPECT_FALSE(lastPassword().isEmpty());
    EXPECT_NE(lastPassword(), first_password);
}

//--------------------------------------------------------------------------------------------------
// The password the user has already read out keeps working until it expires. Neither a write to
// the settings the password does not depend on, nor a change of the allowed session types, takes
// it away from the person it was given to.
TEST_F(RouterManagerTest, OneTimePasswordSurvivesUnrelatedSettingsChanges)
{
    startManager(true);

    ASSERT_TRUE(waitFor([this]() { return requests_received_.load() >= 1; }));
    sendIdResponse(proto::router::kErrorOk, kHostId, kHostKey);
    ASSERT_TRUE(waitFor([this]() { return credentials_host_id_.load() == kHostId; }));

    const SecureString first_password = lastPassword();
    ASSERT_FALSE(first_password.isEmpty());

    // What the service does when it sees the settings file change.
    host_worker_->invoke([this]() { manager_->onSettingsChanged(); });
    EXPECT_EQ(lastPassword(), first_password);

    // What the user interface does when the allowed session types are ticked off.
    host_worker_->invoke([this]()
    {
        manager_->onOneTimeSessionsChanged(proto::peer::SESSION_TYPE_DESKTOP);
    });
    EXPECT_EQ(lastPassword(), first_password);
}

//--------------------------------------------------------------------------------------------------
// Asking for a new password is the one thing that does take the current one away, and what the user
// is shown afterwards is the password that was actually put in place.
TEST_F(RouterManagerTest, ExplicitRequestReplacesTheOneTimePassword)
{
    startManager(true);

    ASSERT_TRUE(waitFor([this]() { return requests_received_.load() >= 1; }));
    sendIdResponse(proto::router::kErrorOk, kHostId, kHostKey);
    ASSERT_TRUE(waitFor([this]() { return credentials_host_id_.load() == kHostId; }));

    const SecureString first_password = lastPassword();
    ASSERT_FALSE(first_password.isEmpty());

    const int seen = credentials_received_.load();

    host_worker_->invoke([this]() { manager_->onNewOneTimePassword(); });

    EXPECT_GT(credentials_received_.load(), seen);
    EXPECT_FALSE(lastPassword().isEmpty());
    EXPECT_NE(lastPassword(), first_password);
}

//--------------------------------------------------------------------------------------------------
// The whole path of a brokered connection: the offer brings the relay credentials to both ends and
// the SRP handshake against the one-time user runs through the relay. The channel that comes out
// carries the session bytes both ways.
TEST_F(RouterManagerTest, PasswordOpensTheChannel)
{
    startManager(true);
    ASSERT_TRUE(waitFor([this]() { return requests_received_.load() >= 1; }));

    host_worker_->invoke([this]()
    {
        manager_->onOneTimeSessionsChanged(proto::peer::SESSION_TYPE_DESKTOP);
    });

    sendIdResponse(proto::router::kErrorOk, kHostId, kHostKey);
    ASSERT_TRUE(waitFor([this]() { return credentials_host_id_.load() == kHostId; }));

    const SecureString password = lastPassword();
    ASSERT_FALSE(password.isEmpty());

    sendConnectionOffer();

    ClientPeer client;
    connectClientPeer(&client, '#' + hostIdToString(kHostId), password,
                      proto::peer::SESSION_TYPE_DESKTOP);
    ASSERT_TRUE(waitFor([&]() { return client.ready.load(); }));

    // The host end comes out of the manager the way the host takes it into a session.
    TcpChannel* host_channel = nullptr;
    std::atomic<int> host_messages { 0 };
    std::mutex host_lock;
    QByteArray host_last_message;

    ASSERT_TRUE(waitFor([&]()
    {
        host_worker_->invoke([&]()
        {
            if (!manager_->hasReadyConnections())
                return;

            std::optional<RouterManager::ReadyConnection> ready =
                manager_->nextReadyConnection();
            ASSERT_TRUE(ready.has_value());

            host_channel = ready->tcp_channel;
            QObject::connect(host_channel, &TcpChannel::sig_messageReceived, host_channel,
                             [&](quint8 /* channel_id */, const QByteArray& buffer)
            {
                std::lock_guard guard(host_lock);
                host_last_message = buffer;
                ++host_messages;
            });

            host_channel->setPaused(false);
        });

        return host_channel != nullptr;
    }));

    // The session bytes make it through the relay in both directions.
    stand_worker_->invoke([&]() { client.channel->send(1, QByteArray("hello-from-client")); });
    ASSERT_TRUE(waitFor([&]() { return host_messages.load() >= 1; }));
    {
        std::lock_guard guard(host_lock);
        EXPECT_EQ(host_last_message, QByteArray("hello-from-client"));
    }

    host_worker_->invoke([&]() { host_channel->send(1, QByteArray("hello-from-host")); });
    ASSERT_TRUE(waitFor([&]() { return client.messages_received.load() >= 1; }));
    {
        std::lock_guard guard(client.lock);
        EXPECT_EQ(client.last_message, QByteArray("hello-from-host"));
    }

    host_worker_->invoke([&]() { delete host_channel; });
    closeClientPeer(&client);
}

//--------------------------------------------------------------------------------------------------
// The password is the one anchor the client has. A client that presents a different one must not
// end up on the channel of the host: the handshake dies instead.
TEST_F(RouterManagerTest, WrongPasswordFailsTheHandshake)
{
    startManager(true);
    ASSERT_TRUE(waitFor([this]() { return requests_received_.load() >= 1; }));

    host_worker_->invoke([this]()
    {
        manager_->onOneTimeSessionsChanged(proto::peer::SESSION_TYPE_DESKTOP);
    });

    sendIdResponse(proto::router::kErrorOk, kHostId, kHostKey);
    ASSERT_TRUE(waitFor([this]() { return credentials_host_id_.load() == kHostId; }));

    sendConnectionOffer();

    ClientPeer client;
    connectClientPeer(&client, '#' + hostIdToString(kHostId), SecureString(QString("wrong")),
                      proto::peer::SESSION_TYPE_DESKTOP);

    ASSERT_TRUE(waitFor([&]() { return client.failed.load(); }));
    EXPECT_FALSE(client.ready.load());

    host_worker_->invoke([this]() { EXPECT_FALSE(manager_->hasReadyConnections()); });
    closeClientPeer(&client);
}

//--------------------------------------------------------------------------------------------------
// The one-time user opens the session types ticked in the settings and nothing else. A client
// with the right password but another type is refused by the host end of the handshake.
TEST_F(RouterManagerTest, PasswordOpensTheAllowedSessionTypesOnly)
{
    startManager(true);
    ASSERT_TRUE(waitFor([this]() { return requests_received_.load() >= 1; }));

    host_worker_->invoke([this]()
    {
        manager_->onOneTimeSessionsChanged(proto::peer::SESSION_TYPE_DESKTOP);
    });

    sendIdResponse(proto::router::kErrorOk, kHostId, kHostKey);
    ASSERT_TRUE(waitFor([this]() { return credentials_host_id_.load() == kHostId; }));

    const SecureString password = lastPassword();
    ASSERT_FALSE(password.isEmpty());

    sendConnectionOffer();

    ClientPeer client;
    connectClientPeer(&client, '#' + hostIdToString(kHostId), password,
                      proto::peer::SESSION_TYPE_FILE_TRANSFER);

    ASSERT_TRUE(waitFor([&]() { return client.failed.load(); }));
    EXPECT_FALSE(client.ready.load());

    host_worker_->invoke([this]() { EXPECT_FALSE(manager_->hasReadyConnections()); });
    closeClientPeer(&client);
}
