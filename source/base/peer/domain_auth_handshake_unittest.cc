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

#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "base/shared_pointer.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_string.h"
#include "base/peer/client_authenticator.h"
#include "base/peer/domain_auth_limiter.h"
#include "base/peer/domain_authenticator.h"
#include "base/peer/server_authenticator.h"
#include "base/peer/user_list.h"
#include "proto/key_exchange.h"
#include "proto/peer.h"

namespace {

const char kDomain[] = "CORP";
const char kUserName[] = "alice";
const char kPassword[] = "Correct horse battery staple";

class FakeDomainAuthenticator final : public DomainAuthenticator
{
public:
    explicit FakeDomainAuthenticator(Status status, quint32 session_types, int delay_ms = 0)
        : status_(status),
          session_types_(session_types),
          delay_ms_(delay_ms)
    {
        // Nothing
    }

    Result authenticate(const QString& domain,
                        const QString& username,
                        const SecureString& password,
                        const QString& peer_address) const final
    {
        ++calls;
        {
            std::scoped_lock lock(peer_address_lock);
            last_peer_address = peer_address;
        }

        // Simulates the time the operating system takes (pam_unix waits after a wrong password).
        // authenticate() runs on a pool thread, so sleeping here does not block the authenticators.
        if (delay_ms_ > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_));

        Result result;
        result.account = domain + QLatin1Char('\\') + username;

        if (domain != QLatin1String(kDomain) || username != QLatin1String(kUserName) ||
            password.toString() != QLatin1String(kPassword))
        {
            result.status = Status::INVALID_CREDENTIALS;
            return result;
        }

        result.status = status_;
        result.session_types = session_types_;
        return result;
    }

    mutable std::atomic_int calls = 0;
    mutable std::mutex peer_address_lock;
    mutable QString last_peer_address;

private:
    const Status status_;
    const quint32 session_types_;
    const int delay_ms_;
};

class TestUserList final : public UserList
{
public:
    User find(const QString& /* username */) const final { return User::kInvalidUser; }
    QByteArray seedKey() const final { return QByteArray(64, 'x'); }
    void setSeedKey(const QByteArray& /* seed_key */) final { /* Nothing */ }

    std::shared_ptr<const DomainAuthenticator> domainAuthenticator() const final
    {
        return domain_authenticator;
    }

    std::shared_ptr<DomainAuthLimiter> domainAuthLimiter() const final
    {
        return limiter;
    }

    std::shared_ptr<FakeDomainAuthenticator> domain_authenticator;
    std::shared_ptr<DomainAuthLimiter> limiter;
};

// Runs a client and a server authenticator against each other in memory. Messages are passed as
// they are: the encryption of the channel is not part of the authenticators.
class DomainAuthHandshakeTest : public testing::Test
{
protected:
    void SetUp() override
    {
        host_key_ = KeyPair::create(KeyPair::Type::X25519);
        ASSERT_TRUE(host_key_.isValid());
        user_list_ = SharedPointer<TestUserList>(new TestUserList());
    }

    struct Outcome
    {
        std::optional<Authenticator::ErrorCode> client;
        std::optional<Authenticator::ErrorCode> server;
        bool untrusted_signal = false;
        bool refused_on_close = false;
        bool key_changed = false;
        QByteArray reported_key;
        QByteArray client_c2s;
        QByteArray server_c2s;
        QByteArray client_s2c;
        QByteArray server_s2c;
        std::string server_user_name;
        quint32 server_session_type = 0;
        qint64 server_elapsed_ms = 0;
    };

    Outcome run(const QByteArray& pinned_key, const QString& username, const QString& password,
                quint32 session_type = proto::peer::SESSION_TYPE_DESKTOP)
    {
        Outcome outcome;

        ServerAuthenticator server;
        server.setUserList(user_list_);
        server.setPeerAddress(QStringLiteral("2001:db8:1:2::10"));
        EXPECT_TRUE(server.setIdentityKey(host_key_.privateKey()));

        ClientAuthenticator client;
        client.setIdentify(proto::key_exchange::IDENTIFY_PASSWORD);
        client.setUserName(username);
        client.setPassword(SecureString(password));
        client.setSessionType(session_type);
        client.setPinnedHostKey(pinned_key);

        QObject::connect(&client, &Authenticator::sig_outgoingMessage, &server,
                         &Authenticator::onIncomingMessage, Qt::QueuedConnection);
        QObject::connect(&server, &Authenticator::sig_outgoingMessage, &client,
                         &Authenticator::onIncomingMessage, Qt::QueuedConnection);

        QObject::connect(&client, &ClientAuthenticator::sig_hostKeyUntrusted, &client,
                         [&](const QByteArray& key, bool key_changed)
        {
            outcome.untrusted_signal = true;
            outcome.key_changed = key_changed;
            outcome.reported_key = key;
        });

        QEventLoop loop;
        QElapsedTimer elapsed;

        auto check_done = [&]()
        {
            // A client that refuses the host sends nothing more, so the server only times out. A
            // server that refuses the client closes the connection, which is not simulated here.
            if (outcome.client && (outcome.server || *outcome.client != Authenticator::ErrorCode::SUCCESS))
                loop.quit();
            if (outcome.server && *outcome.server != Authenticator::ErrorCode::SUCCESS)
                loop.quit();
        };

        QObject::connect(&client, &Authenticator::sig_finished, &loop,
                         [&](Authenticator::ErrorCode error_code)
        {
            outcome.client = error_code;
            if (error_code == Authenticator::ErrorCode::SUCCESS)
            {
                outcome.client_c2s = client.sessionKey(Authenticator::Direction::ENCRYPT).toByteArray();
                outcome.client_s2c = client.sessionKey(Authenticator::Direction::DECRYPT).toByteArray();
            }
            check_done();
        });

        QObject::connect(&server, &Authenticator::sig_finished, &loop,
                         [&](Authenticator::ErrorCode error_code)
        {
            outcome.server = error_code;
            outcome.server_elapsed_ms = elapsed.elapsed();
            if (error_code == Authenticator::ErrorCode::SUCCESS)
            {
                outcome.server_c2s = server.sessionKey(Authenticator::Direction::DECRYPT).toByteArray();
                outcome.server_s2c = server.sessionKey(Authenticator::Direction::ENCRYPT).toByteArray();
                outcome.server_user_name = server.userName();
                outcome.server_session_type = server.sessionType();
            }
            check_done();
        });

        QTimer::singleShot(15000, &loop, &QEventLoop::quit);

        elapsed.start();
        server.start();
        client.start();
        loop.exec();

        outcome.refused_on_close = client.isRefusedOnClose();
        return outcome;
    }

    KeyPair host_key_;
    SharedPointer<TestUserList> user_list_;
};

} // namespace

TEST_F(DomainAuthHandshakeTest, PinnedHostAcceptsValidDomainCredentials)
{
    user_list_->domain_authenticator = std::make_shared<FakeDomainAuthenticator>(
        DomainAuthenticator::Status::SUCCESS,
        proto::peer::SESSION_TYPE_DESKTOP | proto::peer::SESSION_TYPE_FILE_TRANSFER);

    Outcome outcome = run(host_key_.publicKey(), QStringLiteral("CORP\\alice"),
                          QLatin1String(kPassword));

    ASSERT_TRUE(outcome.client);
    ASSERT_TRUE(outcome.server);
    EXPECT_EQ(*outcome.client, Authenticator::ErrorCode::SUCCESS);
    EXPECT_EQ(*outcome.server, Authenticator::ErrorCode::SUCCESS);
    EXPECT_FALSE(outcome.untrusted_signal);
    EXPECT_EQ(outcome.server_user_name, "CORP\\alice");
    EXPECT_EQ(outcome.server_session_type, quint32(proto::peer::SESSION_TYPE_DESKTOP));
    EXPECT_FALSE(outcome.client_c2s.isEmpty());
    EXPECT_EQ(outcome.client_c2s, outcome.server_c2s);
    EXPECT_EQ(outcome.client_s2c, outcome.server_s2c);
    EXPECT_NE(outcome.client_c2s, outcome.client_s2c);
    EXPECT_EQ(user_list_->domain_authenticator->calls, 1);

    // The address of the client reaches the operating system (PAM_RHOST).
    std::scoped_lock lock(user_list_->domain_authenticator->peer_address_lock);
    EXPECT_EQ(user_list_->domain_authenticator->last_peer_address, "2001:db8:1:2::10");
}

TEST_F(DomainAuthHandshakeTest, UnknownHostKeyStopsBeforeCredentialsAreSent)
{
    user_list_->domain_authenticator = std::make_shared<FakeDomainAuthenticator>(
        DomainAuthenticator::Status::SUCCESS, proto::peer::SESSION_TYPE_ALL);

    Outcome outcome = run(QByteArray(), QStringLiteral("CORP\\alice"), QLatin1String(kPassword));

    ASSERT_TRUE(outcome.client);
    EXPECT_EQ(*outcome.client, Authenticator::ErrorCode::HOST_KEY_UNTRUSTED);
    EXPECT_TRUE(outcome.untrusted_signal);
    EXPECT_FALSE(outcome.key_changed);
    EXPECT_EQ(outcome.reported_key, host_key_.publicKey());
    EXPECT_EQ(user_list_->domain_authenticator->calls, 0);
}

TEST_F(DomainAuthHandshakeTest, ChangedHostKeyIsReportedAndNoCredentialsAreSent)
{
    user_list_->domain_authenticator = std::make_shared<FakeDomainAuthenticator>(
        DomainAuthenticator::Status::SUCCESS, proto::peer::SESSION_TYPE_ALL);

    KeyPair other = KeyPair::create(KeyPair::Type::X25519);
    Outcome outcome = run(other.publicKey(), QStringLiteral("CORP\\alice"),
                          QLatin1String(kPassword));

    ASSERT_TRUE(outcome.client);
    EXPECT_EQ(*outcome.client, Authenticator::ErrorCode::HOST_KEY_UNTRUSTED);
    EXPECT_TRUE(outcome.untrusted_signal);
    EXPECT_TRUE(outcome.key_changed);
    EXPECT_EQ(outcome.reported_key, host_key_.publicKey());
    EXPECT_EQ(user_list_->domain_authenticator->calls, 0);
}

TEST_F(DomainAuthHandshakeTest, WrongPasswordIsRefused)
{
    user_list_->domain_authenticator = std::make_shared<FakeDomainAuthenticator>(
        DomainAuthenticator::Status::SUCCESS, proto::peer::SESSION_TYPE_ALL);

    Outcome outcome = run(host_key_.publicKey(), QStringLiteral("CORP\\alice"),
                          QStringLiteral("wrong password"));

    ASSERT_TRUE(outcome.server);
    EXPECT_EQ(*outcome.server, Authenticator::ErrorCode::ACCESS_DENIED);
    EXPECT_FALSE(outcome.client);
    // The channel turns the following disconnect into ACCESS_DENIED for the client.
    EXPECT_TRUE(outcome.refused_on_close);
    EXPECT_EQ(user_list_->domain_authenticator->calls, 1);
}

TEST_F(DomainAuthHandshakeTest, DisabledDomainLogonIsRefused)
{
    // No domain authenticator: the feature is turned off on the host.
    Outcome outcome = run(host_key_.publicKey(), QStringLiteral("CORP\\alice"),
                          QLatin1String(kPassword));

    ASSERT_TRUE(outcome.server);
    EXPECT_EQ(*outcome.server, Authenticator::ErrorCode::ACCESS_DENIED);
    EXPECT_FALSE(outcome.client);
    // Refused before any credentials were sent.
    EXPECT_FALSE(outcome.refused_on_close);
}

TEST_F(DomainAuthHandshakeTest, AccountWithoutMappedGroupIsRefusedLikeAWrongPassword)
{
    user_list_->domain_authenticator = std::make_shared<FakeDomainAuthenticator>(
        DomainAuthenticator::Status::NO_PERMISSIONS, 0);

    Outcome outcome = run(host_key_.publicKey(), QStringLiteral("CORP\\alice"),
                          QLatin1String(kPassword));

    // The client must not learn that the password was correct.
    ASSERT_TRUE(outcome.server);
    EXPECT_EQ(*outcome.server, Authenticator::ErrorCode::ACCESS_DENIED);
    EXPECT_FALSE(outcome.client);
    EXPECT_TRUE(outcome.refused_on_close);
}

TEST_F(DomainAuthHandshakeTest, SessionTypeOutsideTheMappedGroupsIsRefused)
{
    user_list_->domain_authenticator = std::make_shared<FakeDomainAuthenticator>(
        DomainAuthenticator::Status::SUCCESS, proto::peer::SESSION_TYPE_FILE_TRANSFER);

    Outcome outcome = run(host_key_.publicKey(), QStringLiteral("CORP\\alice"),
                          QLatin1String(kPassword), proto::peer::SESSION_TYPE_DESKTOP);

    ASSERT_TRUE(outcome.client);
    EXPECT_EQ(*outcome.client, Authenticator::ErrorCode::SESSION_DENIED);
}

TEST_F(DomainAuthHandshakeTest, FailuresAreAnsweredAtTheSameTime)
{
    // pam_unix answers a wrong password after a delay, but a correct password of an account without
    // a mapped group at once. The server must answer both at the same time, otherwise the time of
    // the answer tells that the password is correct.
    user_list_->domain_authenticator = std::make_shared<FakeDomainAuthenticator>(
        DomainAuthenticator::Status::NO_PERMISSIONS, 0, 1500);

    Outcome wrong_password = run(host_key_.publicKey(), QStringLiteral("CORP\\alice"),
                                 QStringLiteral("wrong password"));

    user_list_->domain_authenticator = std::make_shared<FakeDomainAuthenticator>(
        DomainAuthenticator::Status::NO_PERMISSIONS, 0, 0);

    Outcome no_permissions = run(host_key_.publicKey(), QStringLiteral("CORP\\alice"),
                                 QLatin1String(kPassword));

    ASSERT_TRUE(wrong_password.server);
    ASSERT_TRUE(no_permissions.server);
    EXPECT_EQ(*wrong_password.server, Authenticator::ErrorCode::ACCESS_DENIED);
    EXPECT_EQ(*no_permissions.server, Authenticator::ErrorCode::ACCESS_DENIED);

    // Both are answered about 4 seconds after the credentials were received.
    EXPECT_GE(wrong_password.server_elapsed_ms, 3900);
    EXPECT_GE(no_permissions.server_elapsed_ms, 3900);
    EXPECT_LT(wrong_password.server_elapsed_ms, 5500);
    EXPECT_LT(no_permissions.server_elapsed_ms, 5500);
    EXPECT_LT(std::abs(wrong_password.server_elapsed_ms - no_permissions.server_elapsed_ms), 500);
}

TEST_F(DomainAuthHandshakeTest, SlowLogonFailureIsAnsweredAtTheNextMultipleOfTheAnswerTime)
{
    // A logon that takes longer than the answer time (e.g. an unreachable domain controller) is
    // answered at the next multiple of the answer time (8 seconds here), not when it returns.
    user_list_->domain_authenticator = std::make_shared<FakeDomainAuthenticator>(
        DomainAuthenticator::Status::SUCCESS, proto::peer::SESSION_TYPE_ALL, 5000);

    Outcome outcome = run(host_key_.publicKey(), QStringLiteral("CORP\\alice"),
                          QStringLiteral("wrong password"));

    ASSERT_TRUE(outcome.server);
    EXPECT_EQ(*outcome.server, Authenticator::ErrorCode::ACCESS_DENIED);
    EXPECT_GE(outcome.server_elapsed_ms, 7900);
    EXPECT_LT(outcome.server_elapsed_ms, 8500);
}

TEST_F(DomainAuthHandshakeTest, SuccessIsNotDelayed)
{
    user_list_->domain_authenticator = std::make_shared<FakeDomainAuthenticator>(
        DomainAuthenticator::Status::SUCCESS, proto::peer::SESSION_TYPE_ALL);

    Outcome outcome = run(host_key_.publicKey(), QStringLiteral("CORP\\alice"),
                          QLatin1String(kPassword));

    ASSERT_TRUE(outcome.server);
    EXPECT_EQ(*outcome.server, Authenticator::ErrorCode::SUCCESS);
    EXPECT_LT(outcome.server_elapsed_ms, 2000);
}

TEST_F(DomainAuthHandshakeTest, AccountInBackoffIsRefusedWithoutAskingTheSystem)
{
    const DomainAuthLimiter::Policy account_policy =
        { 1, MilliSeconds(Minutes(1)), MilliSeconds(Minutes(30)), MilliSeconds(Hours(1)) };
    const DomainAuthLimiter::Policy address_policy =
        { 100, MilliSeconds(Minutes(1)), MilliSeconds(Minutes(30)), MilliSeconds(Hours(1)) };

    user_list_->limiter = std::make_shared<DomainAuthLimiter>(account_policy, address_policy, 100);
    user_list_->domain_authenticator = std::make_shared<FakeDomainAuthenticator>(
        DomainAuthenticator::Status::SUCCESS, proto::peer::SESSION_TYPE_ALL);

    Outcome wrong = run(host_key_.publicKey(), QStringLiteral("CORP\\alice"),
                        QStringLiteral("wrong password"));
    ASSERT_TRUE(wrong.server);
    EXPECT_EQ(*wrong.server, Authenticator::ErrorCode::ACCESS_DENIED);
    EXPECT_EQ(user_list_->domain_authenticator->calls, 1);

    // Now even the correct password is refused, the operating system is not asked (no attempt is
    // added to the lockout counter of the domain), and the answer comes at the usual time.
    Outcome blocked = run(host_key_.publicKey(), QStringLiteral("alice@corp.example.com"),
                          QLatin1String(kPassword));
    ASSERT_TRUE(blocked.server);
    EXPECT_EQ(*blocked.server, Authenticator::ErrorCode::ACCESS_DENIED);
    EXPECT_FALSE(blocked.client);
    EXPECT_EQ(user_list_->domain_authenticator->calls, 1);
    EXPECT_GE(blocked.server_elapsed_ms, 3900);
    EXPECT_LT(blocked.server_elapsed_ms, 5500);
}

TEST_F(DomainAuthHandshakeTest, PasswordWithNulIsNotSent)
{
    user_list_->domain_authenticator = std::make_shared<FakeDomainAuthenticator>(
        DomainAuthenticator::Status::SUCCESS, proto::peer::SESSION_TYPE_ALL);

    Outcome outcome = run(host_key_.publicKey(), QStringLiteral("CORP\\alice"),
                          QString::fromUtf16(u"pass\u0000word", 9));

    ASSERT_TRUE(outcome.client);
    EXPECT_EQ(*outcome.client, Authenticator::ErrorCode::ACCESS_DENIED);
    EXPECT_EQ(user_list_->domain_authenticator->calls, 0);
}

TEST_F(DomainAuthHandshakeTest, ComputerAccountIsNotSent)
{
    user_list_->domain_authenticator = std::make_shared<FakeDomainAuthenticator>(
        DomainAuthenticator::Status::SUCCESS, proto::peer::SESSION_TYPE_ALL);

    Outcome outcome = run(host_key_.publicKey(), QStringLiteral("CORP\\PC01$"),
                          QLatin1String(kPassword));

    ASSERT_TRUE(outcome.client);
    EXPECT_EQ(*outcome.client, Authenticator::ErrorCode::ACCESS_DENIED);
    EXPECT_EQ(user_list_->domain_authenticator->calls, 0);
}

TEST_F(DomainAuthHandshakeTest, SuccessfulLogonResetsTheLimiter)
{
    user_list_->limiter = std::make_shared<DomainAuthLimiter>();
    user_list_->domain_authenticator = std::make_shared<FakeDomainAuthenticator>(
        DomainAuthenticator::Status::SUCCESS, proto::peer::SESSION_TYPE_ALL);

    Outcome wrong = run(host_key_.publicKey(), QStringLiteral("CORP\\alice"),
                        QStringLiteral("wrong password"));
    ASSERT_TRUE(wrong.server);
    EXPECT_EQ(user_list_->limiter->accountCount(), 1);
    EXPECT_EQ(user_list_->limiter->addressCount(), 1);

    Outcome good = run(host_key_.publicKey(), QStringLiteral("CORP\\alice"), QLatin1String(kPassword));
    ASSERT_TRUE(good.server);
    EXPECT_EQ(*good.server, Authenticator::ErrorCode::SUCCESS);
    EXPECT_EQ(user_list_->limiter->accountCount(), 0);
    EXPECT_EQ(user_list_->limiter->addressCount(), 0);
}

TEST(DomainAuthIdentityKeyTest, IdentityKeyDoesNotEnableAnonymousAccess)
{
    KeyPair key = KeyPair::create(KeyPair::Type::X25519);
    ASSERT_TRUE(key.isValid());

    ServerAuthenticator server;
    EXPECT_TRUE(server.setIdentityKey(key.privateKey()));
    // Anonymous access needs its own key (setPrivateKey()); the identity key is not reused.
    EXPECT_FALSE(server.setAnonymousAccess(ServerAuthenticator::AnonymousAccess::ENABLE,
                                           proto::peer::SESSION_TYPE_DESKTOP));
}

TEST(DomainAuthIdentityKeyTest, ModeLabelIsVersioned)
{
    // Changing the label is a protocol break of IDENTIFY_PASSWORD; keep it explicit.
    EXPECT_STREQ(kPasswordModeLabel, "aspia/identify-password/v2");
}
