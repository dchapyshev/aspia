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

#include <QObject>

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

    static RouterConfig config(const QByteArray& device_token = QByteArray())
    {
        RouterConfig config;
        config.setRouterId(kRouterId);
        config.setDeviceToken(device_token);
        return config;
    }

    static proto::router::RouterToClient activeChallenge()
    {
        proto::router::RouterToClient message;
        message.mutable_two_factor_challenge()->set_mode(proto::router::TWO_FACTOR_MODE_ACTIVE);
        return message;
    }

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
// The submitted code closes the question at once, the reply is either LoginResult or the
// connection going down.
TEST_F(Router2FATest, SubmittedCodeClosesThePrompt)
{
    Router2FATestPeer::receive(login_, activeChallenge());

    login_.twoFactorPrompt()->submitCode("123456");

    EXPECT_EQ(login_.twoFactorPrompt(), nullptr);
}

//--------------------------------------------------------------------------------------------------
// A prompt answers once. A late repeated submission of an already answered prompt must not send
// anything and must not close the question that replaced it.
TEST_F(Router2FATest, AnsweredPromptCannotTouchTheNextOne)
{
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
