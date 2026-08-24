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

#include "client/router_2fa.h"

#include <gtest/gtest.h>

#include <QEventLoop>
#include <QObject>
#include <QTemporaryDir>
#include <QTimer>

#include <limits>
#include <optional>

#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/os_crypt.h"
#include "base/crypto/secure_byte_array.h"
#include "client/database.h"
#include "client/router_test_fixture.h"
#include "proto/router_constants.h"

namespace {

constexpr qint64 kRouterId = 1;
constexpr qint64 kUserId = 7;

} // namespace

// Test-only delivery of what the worker would hand over.
class Router2FATestPeer
{
public:
    static void start(Router2FA& login, const QVersionNumber& peer_version)
    {
        login.onStart(peer_version);
    }

    static void receive(Router2FA& login, const google::protobuf::MessageLite& message)
    {
        login.onMessageReceived(proto::router::CHANNEL_ID_CLIENT,
                                QByteArray::fromStdString(message.SerializeAsString()));
    }

    static void dropConnection(Router2FA& login)
    {
        login.onConnectionLost(TcpChannel::ErrorCode::UNKNOWN);
    }
};

// The login without a worker. The router messages are handed to it as the worker would, and the
// observable contract is under test, the life of the prompt and the signals of the login. What
// goes out to the worker is internal plumbing and is not watched here.
class Router2FATest : public testing::Test
{
protected:
    Router2FATest()
        : login_(config())
    {
        QObject::connect(&login_, &Router2FA::sig_twoFactorRequired,
                         [this](qint64) { ++prompts_required_; });
        QObject::connect(&login_, &Router2FA::sig_twoFactorFinished,
                         [this](qint64, qint64 user_id, const QVersionNumber& peer_version)
        {
            logged_in_user_ = user_id;
            logged_in_version_ = peer_version;
        });
    }

    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());
        DatabaseTestPeer::setFilePath(temp_dir_.path() + "/client.db3");
        ASSERT_TRUE(Database::instance().isValid());

        // The records of the book are sealed with the master key; the tests run against an
        // unlocked one.
        DataCryptor::instance().setKey(SecureByteArray(QByteArray(32, 'k')));
    }

    static SharedPointer<RouterConfig> config(const QByteArray& device_token = QByteArray())
    {
        RouterConfig* config = new RouterConfig();
        config->setRouterId(kRouterId);
        config->setAddress("router.example.com");
        config->setUsername("user");
        config->setPassword(SecureString(QString("secret")));

        // The record holds the token the way it is stored: wrapped by the OS keystore.
        if (!device_token.isEmpty())
        {
            QByteArray wrapped;
            CHECK(OSCrypt::encryptBytes(device_token, &wrapped));
            config->setDeviceToken(wrapped);
        }

        return SharedPointer<RouterConfig>(config);
    }

    // Puts the record the login works with into the isolated book.
    void seedRecord(const QByteArray& device_token = QByteArray())
    {
        RouterConfig record = *config(device_token);
        ASSERT_TRUE(Database::instance().addRouter(record));
        ASSERT_EQ(record.routerId(), kRouterId);
    }

    static proto::router::RouterToClient activeChallenge()
    {
        proto::router::RouterToClient message;
        message.mutable_two_factor_challenge()->set_mode(proto::router::TWO_FACTOR_MODE_ACTIVE);
        return message;
    }

    QTemporaryDir temp_dir_;
    Router2FA login_;
    int prompts_required_ = 0;
    qint64 logged_in_user_ = 0;
    QVersionNumber logged_in_version_;
};

//--------------------------------------------------------------------------------------------------
// An enrolled account with no stored token has nothing to answer by itself, so the challenge
// births the prompt and announces it.
TEST_F(Router2FATest, ActiveChallengeOpensThePrompt)
{
    EXPECT_EQ(login_.twoFactorPrompt(), nullptr);

    Router2FATestPeer::receive(login_, activeChallenge());

    TwoFactorPrompt* prompt = login_.twoFactorPrompt();
    ASSERT_NE(prompt, nullptr);
    EXPECT_TRUE(prompt->otpauthUri().isEmpty());
    EXPECT_FALSE(prompt->codeRefused());
    EXPECT_EQ(prompt->blockedSeconds(), 0);
    EXPECT_EQ(prompts_required_, 1);
}

//--------------------------------------------------------------------------------------------------
// A stored device token answers the challenge without disturbing the operator.
TEST_F(Router2FATest, StoredTokenAnswersWithoutAPrompt)
{
    Router2FA login(config("device-token-32-bytes-long-value"));

    Router2FATestPeer::receive(login, activeChallenge());

    EXPECT_EQ(login.twoFactorPrompt(), nullptr);
}

//--------------------------------------------------------------------------------------------------
// The stored wrap opens with the local OS keystore. One that does not open right now (a record
// brought from another user or machine, a locked keychain) answers nothing: the operator is
// asked for a code, and the wrap stays in the record untouched for the day it opens again.
TEST_F(Router2FATest, UnopenableTokenFallsBackToThePrompt)
{
    SharedPointer<RouterConfig> unopenable = config();
    unopenable->setDeviceToken("not-a-wrap-of-this-machine");
    Router2FA login(unopenable);

    Router2FATestPeer::receive(login, activeChallenge());

    EXPECT_NE(login.twoFactorPrompt(), nullptr);
    EXPECT_EQ(unopenable->deviceToken(), QByteArray("not-a-wrap-of-this-machine"));
}

//--------------------------------------------------------------------------------------------------
// A rejected token is dead on the router, so the stored copy leaves the record at once and the
// operator is asked for a code.
TEST_F(Router2FATest, RejectedTokenIsDroppedFromTheRecord)
{
    seedRecord("stored-device-token");
    Router2FA login(config("stored-device-token"));

    proto::router::RouterToClient message = activeChallenge();
    message.mutable_two_factor_challenge()->set_token_rejected(true);
    Router2FATestPeer::receive(login, message);

    EXPECT_NE(login.twoFactorPrompt(), nullptr);

    const std::optional<RouterConfig> stored = Database::instance().findRouter(kRouterId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->deviceToken().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Enrollment means a brand new secret, so a token of the previous life of the account leaves
// the record together with it.
TEST_F(Router2FATest, EnrollmentDropsTheStoredToken)
{
    seedRecord("stored-device-token");
    Router2FA login(config("stored-device-token"));

    proto::router::RouterToClient message;
    message.mutable_two_factor_challenge()->set_mode(proto::router::TWO_FACTOR_MODE_ENROLL);
    message.mutable_two_factor_challenge()->set_otpauth_uri(
        "otpauth://totp/Aspia:user?secret=ABCDEFGH");
    Router2FATestPeer::receive(login, message);

    EXPECT_NE(login.twoFactorPrompt(), nullptr);

    const std::optional<RouterConfig> stored = Database::instance().findRouter(kRouterId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->deviceToken().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The enrollment challenge repeats on every reconnect of the stage, and most enrollments hold no
// token at all. A challenge with nothing to drop must not rewrite the record.
TEST_F(Router2FATest, EnrollmentWithoutAStoredTokenLeavesTheRecordAlone)
{
    seedRecord();

    // A rename made behind the login's back: a needless rewrite would revert it.
    RouterConfig renamed = *config();
    renamed.setDisplayName("renamed");
    ASSERT_TRUE(Database::instance().modifyRouter(renamed));

    proto::router::RouterToClient message;
    message.mutable_two_factor_challenge()->set_mode(proto::router::TWO_FACTOR_MODE_ENROLL);
    message.mutable_two_factor_challenge()->set_otpauth_uri(
        "otpauth://totp/Aspia:user?secret=ABCDEFGH");
    Router2FATestPeer::receive(login_, message);

    EXPECT_NE(login_.twoFactorPrompt(), nullptr);

    const std::optional<RouterConfig> stored = Database::instance().findRouter(kRouterId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->displayName(), QString("renamed"));
}

//--------------------------------------------------------------------------------------------------
// The router repeats the enrollment challenge on every reconnect of the stage. The QR code on
// screen must not change under the operator's hands, so the same question keeps the same prompt,
// and a challenge with a different URI is a new question that replaces it.
TEST_F(Router2FATest, RepeatedEnrollmentChallengeKeepsThePrompt)
{
    proto::router::RouterToClient message;
    message.mutable_two_factor_challenge()->set_mode(proto::router::TWO_FACTOR_MODE_ENROLL);
    message.mutable_two_factor_challenge()->set_otpauth_uri(
        "otpauth://totp/Aspia:user?secret=ABCDEFGH");

    Router2FATestPeer::receive(login_, message);

    TwoFactorPrompt* first = login_.twoFactorPrompt();
    ASSERT_NE(first, nullptr);
    ASSERT_EQ(prompts_required_, 1);

    Router2FATestPeer::receive(login_, message);

    EXPECT_EQ(login_.twoFactorPrompt(), first);
    EXPECT_EQ(prompts_required_, 1);

    // A reset on the router hands out a new secret: the different URI is a new question.
    proto::router::RouterToClient other;
    other.mutable_two_factor_challenge()->set_mode(proto::router::TWO_FACTOR_MODE_ENROLL);
    other.mutable_two_factor_challenge()->set_otpauth_uri(
        "otpauth://totp/Aspia:user?secret=IJKLMNOP");
    Router2FATestPeer::receive(login_, other);

    EXPECT_NE(login_.twoFactorPrompt(), first);
    EXPECT_EQ(prompts_required_, 2);
}

//--------------------------------------------------------------------------------------------------
// The token issued by LoginResult is what skips the prompt next time, so it must reach the
// stored record.
TEST_F(Router2FATest, IssuedTokenIsStoredInTheRecord)
{
    seedRecord();

    Router2FATestPeer::start(login_, QVersionNumber(3, 0, 0));

    const std::string token(proto::router::kDeviceTokenSize, 't');

    proto::router::RouterToClient message;
    message.mutable_login_result()->set_user_id(kUserId);
    message.mutable_login_result()->set_new_token(token);
    Router2FATestPeer::receive(login_, message);

    EXPECT_EQ(logged_in_user_, kUserId);

    // The record stores the wrap, and the wrap made here opens back into the token.
    const std::optional<RouterConfig> stored = Database::instance().findRouter(kRouterId);
    ASSERT_TRUE(stored.has_value());
    QByteArray raw;
    ASSERT_TRUE(OSCrypt::decryptBytes(stored->deviceToken(), &raw));
    EXPECT_EQ(raw, QByteArray::fromStdString(token));
}

//--------------------------------------------------------------------------------------------------
// A token of any other size is not a token: the router refuses everything but the one size it
// issues, so what is stored would only buy a doomed round of presenting it back. The login is
// still complete.
TEST_F(Router2FATest, TokenOfAnUnexpectedSizeIsNotStored)
{
    seedRecord();

    Router2FATestPeer::start(login_, QVersionNumber(3, 0, 0));

    proto::router::RouterToClient message;
    message.mutable_login_result()->set_user_id(kUserId);
    message.mutable_login_result()->set_new_token(
        std::string(proto::router::kDeviceTokenSize + 1, 't'));
    Router2FATestPeer::receive(login_, message);

    EXPECT_EQ(logged_in_user_, kUserId);

    const std::optional<RouterConfig> stored = Database::instance().findRouter(kRouterId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->deviceToken().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The refusal of the last code and a running block arrive with the challenge and are what the
// reopened prompt describes.
TEST_F(Router2FATest, RefusalAndBlockArriveWithTheChallenge)
{
    proto::router::RouterToClient message = activeChallenge();
    message.mutable_two_factor_challenge()->set_code_rejected(true);
    message.mutable_two_factor_challenge()->set_blocked_seconds(600);
    Router2FATestPeer::receive(login_, message);

    TwoFactorPrompt* prompt = login_.twoFactorPrompt();
    ASSERT_NE(prompt, nullptr);
    EXPECT_TRUE(prompt->codeRefused());
    EXPECT_EQ(prompt->blockedSeconds(), 600);
}

//--------------------------------------------------------------------------------------------------
// The seconds of the block come from the peer, and only their bounds are believed. A negative
// block would split the gates that read it (shown by one, out of the rotation by another), and an
// enormous one would overflow the arithmetic of whoever formats it.
TEST_F(Router2FATest, BlockOfTheChallengeIsBounded)
{
    proto::router::RouterToClient message = activeChallenge();
    message.mutable_two_factor_challenge()->set_blocked_seconds(-600);
    Router2FATestPeer::receive(login_, message);

    TwoFactorPrompt* prompt = login_.twoFactorPrompt();
    ASSERT_NE(prompt, nullptr);
    EXPECT_EQ(prompt->blockedSeconds(), 0);

    message.mutable_two_factor_challenge()->set_blocked_seconds(
        std::numeric_limits<qint64>::max());
    Router2FATestPeer::receive(login_, message);

    prompt = login_.twoFactorPrompt();
    ASSERT_NE(prompt, nullptr);
    EXPECT_EQ(prompt->blockedSeconds(), proto::router::kMaxTwoFactorBlockSeconds);
}

//--------------------------------------------------------------------------------------------------
// The block is sent as a remaining duration, and this side counts the same wait down by itself:
// when the announced seconds run out, the question is announced again and the dialog opens the
// regular way, without waiting for the connection to die.
TEST_F(Router2FATest, ExpiredBlockReasksTheQuestion)
{
    proto::router::RouterToClient message = activeChallenge();
    message.mutable_two_factor_challenge()->set_blocked_seconds(1);
    Router2FATestPeer::receive(login_, message);

    ASSERT_NE(login_.twoFactorPrompt(), nullptr);
    EXPECT_EQ(prompts_required_, 1);

    QEventLoop loop;
    QTimer::singleShot(1200, &loop, &QEventLoop::quit);
    loop.exec();

    EXPECT_EQ(prompts_required_, 2);
    TwoFactorPrompt* prompt = login_.twoFactorPrompt();
    ASSERT_NE(prompt, nullptr);
    EXPECT_EQ(prompt->blockedSeconds(), 0);
}

//--------------------------------------------------------------------------------------------------
// While the block runs, every reconnect brings the same question with a smaller remainder. The
// remainder carries no news (both sides count the same block down), so the prompt stays whole
// and the journal is not flooded. A challenge without the block is a different question.
TEST_F(Router2FATest, RepeatedBlockedChallengeChangesNothing)
{
    proto::router::RouterToClient message = activeChallenge();
    message.mutable_two_factor_challenge()->set_blocked_seconds(600);
    Router2FATestPeer::receive(login_, message);
    TwoFactorPrompt* prompt = login_.twoFactorPrompt();
    ASSERT_NE(prompt, nullptr);
    EXPECT_EQ(prompts_required_, 1);

    message.mutable_two_factor_challenge()->set_blocked_seconds(597);
    Router2FATestPeer::receive(login_, message);

    EXPECT_EQ(login_.twoFactorPrompt(), prompt);
    EXPECT_EQ(prompts_required_, 1);

    message.mutable_two_factor_challenge()->set_blocked_seconds(0);
    Router2FATestPeer::receive(login_, message);

    EXPECT_NE(login_.twoFactorPrompt(), prompt);
    EXPECT_EQ(prompts_required_, 2);
}

//--------------------------------------------------------------------------------------------------
// The submitted code closes the question at once, the reply is either LoginResult or the
// connection going down.
TEST_F(Router2FATest, SubmittedCodeClosesThePrompt)
{
    Router2FATestPeer::start(login_, QVersionNumber(3, 0, 0));
    Router2FATestPeer::receive(login_, activeChallenge());

    login_.twoFactorPrompt()->submitCode("123456");

    EXPECT_EQ(login_.twoFactorPrompt(), nullptr);
}

//--------------------------------------------------------------------------------------------------
// A prompt answers once. A late repeated submission of an already answered prompt must not send
// anything and must not close the question that replaced it.
TEST_F(Router2FATest, AnsweredPromptCannotTouchTheNextOne)
{
    Router2FATestPeer::start(login_, QVersionNumber(3, 0, 0));
    Router2FATestPeer::receive(login_, activeChallenge());
    TwoFactorPrompt* first = login_.twoFactorPrompt();
    ASSERT_NE(first, nullptr);
    first->submitCode("123456");

    Router2FATestPeer::receive(login_, activeChallenge());
    TwoFactorPrompt* second = login_.twoFactorPrompt();
    ASSERT_NE(second, nullptr);
    EXPECT_NE(second, first);

    first->submitCode("654321");

    EXPECT_EQ(login_.twoFactorPrompt(), second);
    EXPECT_EQ(prompts_required_, 2);
}

//--------------------------------------------------------------------------------------------------
// A new challenge replaces the question of an open prompt with a fresh object and announces it,
// so whoever shows the question learns that what is on screen is no longer asked.
TEST_F(Router2FATest, ReplacedQuestionIsAnnounced)
{
    Router2FATestPeer::receive(login_, activeChallenge());
    TwoFactorPrompt* first = login_.twoFactorPrompt();
    EXPECT_EQ(prompts_required_, 1);

    proto::router::RouterToClient message = activeChallenge();
    message.mutable_two_factor_challenge()->set_code_rejected(true);
    Router2FATestPeer::receive(login_, message);

    TwoFactorPrompt* second = login_.twoFactorPrompt();
    ASSERT_NE(second, nullptr);
    EXPECT_NE(second, first);
    EXPECT_TRUE(second->codeRefused());
    EXPECT_EQ(prompts_required_, 2);
}

//--------------------------------------------------------------------------------------------------
// The enrollment URI is what the operator scans into their authenticator, and it arrives from
// the peer. One that is not an otpauth:// URI, or is too long for the QR encoder to carry, is
// refused whole and no prompt is born.
TEST_F(Router2FATest, MalformedEnrollmentUriIsRefused)
{
    proto::router::RouterToClient message;
    message.mutable_two_factor_challenge()->set_mode(proto::router::TWO_FACTOR_MODE_ENROLL);
    message.mutable_two_factor_challenge()->set_otpauth_uri(
        "otpauth://totp/Aspia:user?secret=" + std::string(4096, 'A'));
    Router2FATestPeer::receive(login_, message);

    EXPECT_EQ(login_.twoFactorPrompt(), nullptr);
    EXPECT_EQ(prompts_required_, 0);

    message.mutable_two_factor_challenge()->set_otpauth_uri("https://example.com/");
    Router2FATestPeer::receive(login_, message);

    EXPECT_EQ(login_.twoFactorPrompt(), nullptr);
    EXPECT_EQ(prompts_required_, 0);
}

//--------------------------------------------------------------------------------------------------
// The dialogs show the secret of the URI's query as the setup key. A URI without one passes the
// shape checks but puts an empty key on screen, so it is refused whole and the stored token of
// the record stays untouched.
TEST_F(Router2FATest, EnrollmentUriWithoutASecretIsRefused)
{
    seedRecord("stored-device-token");
    Router2FA login(config("stored-device-token"));

    proto::router::RouterToClient message;
    message.mutable_two_factor_challenge()->set_mode(proto::router::TWO_FACTOR_MODE_ENROLL);
    message.mutable_two_factor_challenge()->set_otpauth_uri("otpauth://totp/Aspia:user");
    Router2FATestPeer::receive(login, message);

    EXPECT_EQ(login.twoFactorPrompt(), nullptr);

    const std::optional<RouterConfig> stored = Database::instance().findRouter(kRouterId);
    ASSERT_TRUE(stored.has_value());
    QByteArray raw;
    ASSERT_TRUE(OSCrypt::decryptBytes(stored->deviceToken(), &raw));
    EXPECT_EQ(raw, QByteArray("stored-device-token"));
}

//--------------------------------------------------------------------------------------------------
// No check on this side can know whether a submitted code reaches the router, so the answer is
// sent into whatever the connection is and closes the question at once. When the router never
// received it, the challenge after the reconnect asks again and the prompt is reborn.
TEST_F(Router2FATest, LostAnswerIsReaskedByTheRouter)
{
    Router2FATestPeer::start(login_, QVersionNumber(3, 0, 0));
    Router2FATestPeer::receive(login_, activeChallenge());
    TwoFactorPrompt* prompt = login_.twoFactorPrompt();
    ASSERT_NE(prompt, nullptr);

    Router2FATestPeer::dropConnection(login_);
    prompt->submitCode("123456");

    EXPECT_EQ(login_.twoFactorPrompt(), nullptr);

    Router2FATestPeer::start(login_, QVersionNumber(3, 0, 0));
    Router2FATestPeer::receive(login_, activeChallenge());

    EXPECT_NE(login_.twoFactorPrompt(), nullptr);
    EXPECT_EQ(prompts_required_, 2);
}

//--------------------------------------------------------------------------------------------------
// A dialog can outlive its question: a fresh challenge replaces the prompt while the old dialog
// still holds the old one. Its late answer must not send anything and must not spend the
// question that replaced it.
TEST_F(Router2FATest, LateSubmissionOfAReplacedQuestionIsIgnored)
{
    Router2FATestPeer::start(login_, QVersionNumber(3, 0, 0));
    Router2FATestPeer::receive(login_, activeChallenge());
    TwoFactorPrompt* first = login_.twoFactorPrompt();
    ASSERT_NE(first, nullptr);

    proto::router::RouterToClient message = activeChallenge();
    message.mutable_two_factor_challenge()->set_code_rejected(true);
    Router2FATestPeer::receive(login_, message);
    TwoFactorPrompt* second = login_.twoFactorPrompt();
    ASSERT_NE(second, nullptr);
    ASSERT_NE(second, first);

    first->submitCode("123456");

    EXPECT_EQ(login_.twoFactorPrompt(), second);
    EXPECT_EQ(prompts_required_, 2);
}

//--------------------------------------------------------------------------------------------------
// The question survives the connection: the router asks the same thing after the reconnect, so
// the prompt stays whole through the drop and the repeated challenge, and the operator goes on
// typing into the same dialog.
TEST_F(Router2FATest, ConnectionLossKeepsTheQuestion)
{
    Router2FATestPeer::receive(login_, activeChallenge());
    TwoFactorPrompt* prompt = login_.twoFactorPrompt();
    ASSERT_NE(prompt, nullptr);

    Router2FATestPeer::dropConnection(login_);

    EXPECT_EQ(login_.twoFactorPrompt(), prompt);

    Router2FATestPeer::receive(login_, activeChallenge());

    EXPECT_EQ(login_.twoFactorPrompt(), prompt);
    EXPECT_EQ(prompts_required_, 1);
}

//--------------------------------------------------------------------------------------------------
// LoginResult ends the login. The report carries the user and the version the connection was
// authenticated with, and the object goes passive until the owner destroys it.
TEST_F(Router2FATest, LoginResultReportsTheCompletedLogin)
{
    Router2FATestPeer::start(login_, QVersionNumber(3, 0, 0));
    Router2FATestPeer::receive(login_, activeChallenge());
    login_.twoFactorPrompt()->submitCode("123456");

    proto::router::RouterToClient message;
    message.mutable_login_result()->set_user_id(kUserId);
    Router2FATestPeer::receive(login_, message);

    EXPECT_EQ(logged_in_user_, kUserId);
    EXPECT_EQ(logged_in_version_, QVersionNumber(3, 0, 0));
}
