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

#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "base/serialization.h"
#include "base/xml_settings.h"
#include "base/crypto/key_pair.h"
#include "base/net/tcp_server.h"
#include "base/threading/worker.h"
#include "build/build_config.h"
#include "host/database.h"
#include "host/host_storage.h"
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

// Drives the timer of the manager with a synthetic clock, in the thread of the manager. In
// production the timer runs with the real time, and the tests cannot wait the real timeouts out.
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
    }

    void TearDown() override
    {
        host_worker_->invoke([this]() { delete manager_.data(); });

        stopRouterStand();
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

    HostStorage storage;
    EXPECT_EQ(storage.hostKey(), QByteArray(kHostKey));
    EXPECT_EQ(storage.lastHostId(), kHostId);
}

//--------------------------------------------------------------------------------------------------
// A host with a stored key presents it. A router that does not know the key sends the host down
// the new-id path, so a wiped router database heals by itself.
TEST_F(RouterManagerTest, UnknownKeyIsResetAndANewIdIsRequested)
{
    HostStorage storage;
    storage.setHostKey(QByteArrayLiteral("the-key-the-router-forgot"));

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
