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

#include "router/handlers/two_factor_handler.h"

#include <QUrl>
#include <QUrlQuery>

#include <vector>

#include "base/crypto/base32.h"
#include "base/crypto/totp.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "router/router_test_base.h"
#include "router/handlers/user_request_handler.h"

// The per-user state outlives the sessions on purpose, so it also outlives a test case.
class TwoFactorHandlerTestPeer
{
public:
    static void forgetUsers() { TwoFactorHandler::user_states_.clear(); }
    static bool hasUserState(qint64 user_id) { return TwoFactorHandler::user_states_.contains(user_id); }
};

// The two-factor stage of a client session against a real database and the real TOTP code, with
// the clock supplied by the test.
class TwoFactorHandlerTest : public RouterTestBase
{
protected:
    void SetUp() override
    {
        RouterTestBase::SetUp();

        // The stage runs for the session of the built-in administrator; the caller identity comes
        // from the authenticated channel, never from the request.
        caller_.session_type = proto::router::SESSION_TYPE_ADMIN;

        TwoFactorHandlerTestPeer::forgetUsers();
    }

    TwoFactorHandler::Result start(TwoFactorHandler& handler, qint64 now = kNow)
    {
        return handler.start(db_, caller_, now);
    }

    TwoFactorHandler::Result submitCode(TwoFactorHandler& handler, const QString& code, qint64 now)
    {
        proto::router::TwoFactorResponse response;
        response.set_totp_code(code.toStdString());
        return handler.handleResponse(db_, caller_, response, "127.0.0.1", now);
    }

    TwoFactorHandler::Result submitToken(TwoFactorHandler& handler, std::string_view token)
    {
        proto::router::TwoFactorResponse response;
        response.set_token(std::string(token));
        return handler.handleResponse(db_, caller_, response, "127.0.0.1", kNow);
    }

    // A code the secret does not produce anywhere inside the drift window around |at|.
    QString wrongCode(const QByteArray& secret, qint64 at) const
    {
        quint64 matched = 0;
        for (int value = 0; ; ++value)
        {
            const QString candidate = QString("%1").arg(value, Totp::kDefaultDigits, 10,
                                                        QLatin1Char('0'));
            if (!Totp::verify(secret, candidate, at, Totp::kDefaultStepSec, Totp::kDefaultDigits,
                              Totp::kDefaultWindowSteps, &matched))
            {
                return candidate;
            }
        }
    }

    // The secret the client scans out of the enrollment URI.
    static QByteArray secretFromUri(const std::string& otpauth_uri)
    {
        const QUrlQuery query(QUrl(QString::fromStdString(otpauth_uri)).query());
        return Base32::decode(query.queryItemValue("secret").toUtf8());
    }

    size_t tokenCount(qint64 user_id)
    {
        std::vector<DeviceToken> tokens;
        if (db_.listClientDeviceTokens(user_id, &tokens) != proto::router::kErrorOk)
            return static_cast<size_t>(-1);
        return tokens.size();
    }

    // Rows in the table, whether they are still usable or not.
    qint64 storedTokenCount()
    {
        return countRaw("SELECT COUNT(*) FROM client_device_tokens");
    }

    // Moves the last use of a token |days| into the past.
    bool ageToken(qint64 token_id, int days)
    {
        return execRaw(QString("UPDATE client_device_tokens SET last_used_at=last_used_at-%1 "
                               "WHERE token_id=%2").arg(days * 24 * 3600).arg(token_id));
    }

    // A fixed point in time: every step of the tests is placed relative to it, so nothing depends
    // on when the suite runs.
    static constexpr qint64 kNow = 1'700'000'000;
};

//--------------------------------------------------------------------------------------------------
// A user with no secret is walked through the enrollment: the secret reaches the database only
// once the user proves they scanned it.
TEST_F(TwoFactorHandlerTest, EnrollmentStoresSecretOnlyAfterConfirmation)
{
    TwoFactorHandler handler;

    const TwoFactorHandler::Result challenge = start(handler);
    ASSERT_EQ(challenge.action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_EQ(challenge.challenge.mode, proto::router::TWO_FACTOR_MODE_ENROLL);

    const QByteArray secret = secretFromUri(challenge.challenge.otpauth_uri);
    ASSERT_FALSE(secret.isEmpty());

    // Nothing is stored until the code arrives.
    EXPECT_TRUE(findUser(admin_.entry_id).otp_secret.isEmpty());

    const TwoFactorHandler::Result accepted =
        submitCode(handler, Totp::code(secret, kNow), kNow);

    ASSERT_EQ(accepted.action, TwoFactorHandler::Action::ACCEPT);
    EXPECT_FALSE(accepted.new_token.empty());
    EXPECT_GT(accepted.token_id, 0);
    EXPECT_EQ(findUser(admin_.entry_id).otp_secret, secret);
}

//--------------------------------------------------------------------------------------------------
// The enrollment is confirmed by the code of the secret we handed out - anything else means the
// user scanned something they do not have.
TEST_F(TwoFactorHandlerTest, EnrollmentRejectsWrongCode)
{
    TwoFactorHandler handler;
    const QByteArray secret = secretFromUri(start(handler).challenge.otpauth_uri);
    ASSERT_FALSE(secret.isEmpty());

    EXPECT_EQ(submitCode(handler, wrongCode(secret, kNow), kNow).action,
              TwoFactorHandler::Action::CLOSE);
    EXPECT_TRUE(findUser(admin_.entry_id).otp_secret.isEmpty());
    EXPECT_EQ(tokenCount(admin_.entry_id), 0u);
}

//--------------------------------------------------------------------------------------------------
// The stage dies with its session every couple of minutes, but the operator may need longer to
// set an authenticator up. The secret is kept per user, so the next session asks with the same
// QR code and the one already scanned stays good.
TEST_F(TwoFactorHandlerTest, EnrollmentSecretSurvivesReconnect)
{
    TwoFactorHandler first;
    const TwoFactorHandler::Result challenge = start(first);
    const QByteArray secret = secretFromUri(challenge.challenge.otpauth_uri);
    ASSERT_FALSE(secret.isEmpty());

    TwoFactorHandler second;
    const TwoFactorHandler::Result again = start(second);
    EXPECT_EQ(secretFromUri(again.challenge.otpauth_uri), secret);

    EXPECT_EQ(submitCode(second, Totp::code(secret, kNow), kNow).action,
              TwoFactorHandler::Action::ACCEPT);
}

//--------------------------------------------------------------------------------------------------
// A mistyped code ends the session, not the enrollment: the reconnected session asks with the
// same secret, so the user retypes the code instead of scanning anew.
TEST_F(TwoFactorHandlerTest, WrongCodeDuringEnrollmentKeepsTheSecret)
{
    TwoFactorHandler first;
    const QByteArray secret = secretFromUri(start(first).challenge.otpauth_uri);
    ASSERT_FALSE(secret.isEmpty());
    ASSERT_EQ(submitCode(first, wrongCode(secret, kNow), kNow).action,
              TwoFactorHandler::Action::CLOSE);

    TwoFactorHandler second;
    const TwoFactorHandler::Result reopened = start(second);
    EXPECT_EQ(secretFromUri(reopened.challenge.otpauth_uri), secret);
    EXPECT_EQ(submitCode(second, Totp::code(secret, kNow), kNow).action,
              TwoFactorHandler::Action::ACCEPT);
}

//--------------------------------------------------------------------------------------------------
// The confirmation retires the shared secret: an enrollment that starts over after a reset must
// hand out a fresh one, not the one a previous life confirmed.
TEST_F(TwoFactorHandlerTest, CompletedEnrollmentRetiresTheSharedSecret)
{
    TwoFactorHandler first;
    const QByteArray secret = secretFromUri(start(first).challenge.otpauth_uri);
    ASSERT_EQ(submitCode(first, Totp::code(secret, kNow), kNow).action,
              TwoFactorHandler::Action::ACCEPT);

    ASSERT_EQ(db_.resetUserOtp(admin_.entry_id), proto::router::kErrorOk);

    TwoFactorHandler second;
    const TwoFactorHandler::Result again = start(second);
    ASSERT_EQ(again.challenge.mode, proto::router::TWO_FACTOR_MODE_ENROLL);
    EXPECT_NE(secretFromUri(again.challenge.otpauth_uri), secret);
}

//--------------------------------------------------------------------------------------------------
// An administrator reset starts the two-factor life of the account anew: the refusal of a code
// from the old life is not shown, and the QR code of a half-done enrollment is not reused.
TEST_F(TwoFactorHandlerTest, AdministratorResetForgetsTheUser)
{
    TwoFactorHandler first;
    const QByteArray secret = secretFromUri(start(first).challenge.otpauth_uri);
    ASSERT_FALSE(secret.isEmpty());
    ASSERT_EQ(submitCode(first, wrongCode(secret, kNow), kNow).action,
              TwoFactorHandler::Action::CLOSE);

    // The refusal is carried into the next challenge until the reset wipes it.
    TwoFactorHandler second;
    ASSERT_TRUE(start(second).challenge.code_rejected);

    proto::router::UserRequest request;
    request.set_command_name(proto::router::kCommandUserResetOtp);
    request.mutable_user()->set_entry_id(admin_.entry_id);
    ASSERT_EQ(handleUserRequest(db_, caller_, request).error_code, proto::router::kErrorOk);

    TwoFactorHandler third;
    const TwoFactorHandler::Result reopened = start(third);
    ASSERT_EQ(reopened.challenge.mode, proto::router::TWO_FACTOR_MODE_ENROLL);
    EXPECT_FALSE(reopened.challenge.code_rejected);
    EXPECT_NE(secretFromUri(reopened.challenge.otpauth_uri), secret);
}

//--------------------------------------------------------------------------------------------------
// A password rotation answers a leaked password, and a half-done enrollment secret is part of
// what the old password could have shown. The rotation retires it together with the refusal
// flag, so the next login enrolls from a fresh QR code.
TEST_F(TwoFactorHandlerTest, PasswordRotationForgetsTheUser)
{
    TwoFactorHandler first;
    const QByteArray secret = secretFromUri(start(first).challenge.otpauth_uri);
    ASSERT_FALSE(secret.isEmpty());
    ASSERT_EQ(submitCode(first, wrongCode(secret, kNow), kNow).action,
              TwoFactorHandler::Action::CLOSE);

    RouterUser rotated = RouterUser::create("admin", SecureString("Rotated1234!"));
    rotated.entry_id = admin_.entry_id;
    rotated.sessions = kAllSessions;
    rotated.flags = User::ENABLED;

    proto::router::UserRequest request;
    request.set_command_name(proto::router::kCommandUserModify);
    request.mutable_user()->CopyFrom(rotated.serialize());
    ASSERT_EQ(handleUserRequest(db_, caller_, request).error_code, proto::router::kErrorOk);

    TwoFactorHandler second;
    const TwoFactorHandler::Result reopened = start(second);
    ASSERT_EQ(reopened.challenge.mode, proto::router::TWO_FACTOR_MODE_ENROLL);
    EXPECT_FALSE(reopened.challenge.code_rejected);
    EXPECT_NE(secretFromUri(reopened.challenge.otpauth_uri), secret);
}

//--------------------------------------------------------------------------------------------------
// The self-service rotation over the client channel is the same event as the administrative one
// and must retire the same stored state.
TEST_F(TwoFactorHandlerTest, SelfServiceRotationForgetsTheUser)
{
    TwoFactorHandler first;
    const QByteArray secret = secretFromUri(start(first).challenge.otpauth_uri);
    ASSERT_FALSE(secret.isEmpty());
    ASSERT_EQ(submitCode(first, wrongCode(secret, kNow), kNow).action,
              TwoFactorHandler::Action::CLOSE);

    const RouterUser rotated = RouterUser::create("admin", SecureString("Rotated1234!"));
    proto::router::ChangePasswordRequest request;
    request.set_salt(toStdString(rotated.salt));
    request.set_verifier(toStdString(rotated.verifier));
    ASSERT_EQ(handleChangePassword(db_, caller_, request).error_code, proto::router::kErrorOk);

    EXPECT_FALSE(TwoFactorHandlerTestPeer::hasUserState(admin_.entry_id));

    TwoFactorHandler second;
    const TwoFactorHandler::Result reopened = start(second);
    ASSERT_EQ(reopened.challenge.mode, proto::router::TWO_FACTOR_MODE_ENROLL);
    EXPECT_FALSE(reopened.challenge.code_rejected);
    EXPECT_NE(secretFromUri(reopened.challenge.otpauth_uri), secret);
}

//--------------------------------------------------------------------------------------------------
// The id of a deleted user is never reused, so a state entry left behind would sit in memory
// forever.
TEST_F(TwoFactorHandlerTest, DeletedUserLeavesNoStateBehind)
{
    const RouterUser victim = addUser("operator", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_TRUE(victim.isValid());

    RequestCaller victim_caller;
    victim_caller.user_id = victim.entry_id;
    victim_caller.name = "operator";
    victim_caller.session_type = proto::router::SESSION_TYPE_OPERATOR;

    TwoFactorHandler handler;
    ASSERT_EQ(handler.start(db_, victim_caller, kNow).action,
              TwoFactorHandler::Action::SEND_CHALLENGE);
    ASSERT_TRUE(TwoFactorHandlerTestPeer::hasUserState(victim.entry_id));

    proto::router::UserRequest request;
    request.set_command_name(proto::router::kCommandUserDelete);
    request.mutable_user()->set_entry_id(victim.entry_id);
    ASSERT_EQ(handleUserRequest(db_, caller_, request).error_code, proto::router::kErrorOk);

    EXPECT_FALSE(TwoFactorHandlerTestPeer::hasUserState(victim.entry_id));
}

//--------------------------------------------------------------------------------------------------
// Another session of the same user finished the enrollment while this one was still at the prompt:
// the session goes away instead of overwriting what is on file. The code it sent was right, so
// the next challenge does not brand it refused.
TEST_F(TwoFactorHandlerTest, EnrollmentLosesRaceToAnotherSession)
{
    TwoFactorHandler handler;

    const TwoFactorHandler::Result challenge = start(handler);
    const QByteArray secret = secretFromUri(challenge.challenge.otpauth_uri);
    ASSERT_FALSE(secret.isEmpty());

    const QByteArray other_secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, other_secret, 100));

    EXPECT_EQ(submitCode(handler, Totp::code(secret, kNow), kNow).action,
              TwoFactorHandler::Action::CLOSE);
    EXPECT_EQ(findUser(admin_.entry_id).otp_secret, other_secret);

    TwoFactorHandler next;
    EXPECT_FALSE(start(next).challenge.code_rejected);
}

//--------------------------------------------------------------------------------------------------
// The mode is decided by the stored state every time the stage opens, and nothing from a previous
// round of it leaks into the verification: an abandoned enrollment must not become the secret a
// later round checks the code against.
TEST_F(TwoFactorHandlerTest, ReopenedStageFollowsTheStoredState)
{
    TwoFactorHandler handler;
    ASSERT_EQ(start(handler).challenge.mode, proto::router::TWO_FACTOR_MODE_ENROLL);

    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    const TwoFactorHandler::Result reopened = start(handler);
    ASSERT_EQ(reopened.action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_EQ(reopened.challenge.mode, proto::router::TWO_FACTOR_MODE_ACTIVE);

    EXPECT_EQ(submitCode(handler, Totp::code(secret, kNow), kNow).action,
              TwoFactorHandler::Action::ACCEPT);
}

//--------------------------------------------------------------------------------------------------
// An enrolled user is asked for a code, and a valid one produces a device token so the next login
// can skip the prompt.
TEST_F(TwoFactorHandlerTest, ActiveStageAcceptsCodeAndIssuesToken)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    TwoFactorHandler handler;

    const TwoFactorHandler::Result challenge = start(handler);
    ASSERT_EQ(challenge.action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_EQ(challenge.challenge.mode, proto::router::TWO_FACTOR_MODE_ACTIVE);
    EXPECT_TRUE(challenge.challenge.otpauth_uri.empty());
    EXPECT_FALSE(challenge.challenge.token_rejected);

    const TwoFactorHandler::Result accepted =
        submitCode(handler, Totp::code(secret, kNow), kNow);

    ASSERT_EQ(accepted.action, TwoFactorHandler::Action::ACCEPT);
    EXPECT_FALSE(accepted.new_token.empty());
    EXPECT_EQ(tokenCount(admin_.entry_id), 1u);

    // The step that was accepted is consumed.
    EXPECT_EQ(findUser(admin_.entry_id).otp_counter,
              static_cast<quint64>(kNow / Totp::kDefaultStepSec));
}

//--------------------------------------------------------------------------------------------------
// The same code must not open a second session: whoever reads it over the shoulder (or off the
// wire of a broken client) gets nothing.
TEST_F(TwoFactorHandlerTest, ReplayedCodeIsRefused)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    const QString code = Totp::code(secret, kNow);

    TwoFactorHandler first;
    ASSERT_EQ(start(first).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    ASSERT_EQ(submitCode(first, code, kNow).action, TwoFactorHandler::Action::ACCEPT);

    TwoFactorHandler second;
    ASSERT_EQ(start(second).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_EQ(submitCode(second, code, kNow).action, TwoFactorHandler::Action::CLOSE);
}

//--------------------------------------------------------------------------------------------------
// A replay is refused but not counted: the code came out of the secret, so it is not a guess. A
// session that drops right after a login brings the user back to the prompt with the same code
// still on their screen, and typing it again must not spend the attempts they are about to need.
TEST_F(TwoFactorHandlerTest, ReplayedCodeIsNotAFailedAttempt)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    const QString code = Totp::code(secret, kNow);

    TwoFactorHandler first;
    ASSERT_EQ(start(first).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    ASSERT_EQ(submitCode(first, code, kNow).action, TwoFactorHandler::Action::ACCEPT);

    for (int i = 0; i < TwoFactorHandler::kMaxFailedAttempts; ++i)
    {
        TwoFactorHandler replay;
        ASSERT_EQ(start(replay).action, TwoFactorHandler::Action::SEND_CHALLENGE);
        ASSERT_EQ(submitCode(replay, code, kNow).action, TwoFactorHandler::Action::CLOSE);
    }

    // The code of the next step opens a session, so the user was never blocked.
    const qint64 next_step = kNow + Totp::kDefaultStepSec;

    TwoFactorHandler after;
    ASSERT_EQ(start(after).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_EQ(submitCode(after, Totp::code(secret, next_step), next_step).action,
              TwoFactorHandler::Action::ACCEPT);
}

//--------------------------------------------------------------------------------------------------
// A wrong code takes the session down with it, so guessing means reconnecting for every attempt
// and no single session sees the series. The router counts the failed attempts of the user across
// sessions and stops verifying codes for a while.
TEST_F(TwoFactorHandlerTest, GuessingIsStoppedAfterTooManyFailedAttempts)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    const QString wrong = wrongCode(secret, kNow);

    for (int i = 0; i < TwoFactorHandler::kMaxFailedAttempts; ++i)
    {
        TwoFactorHandler attempt;
        ASSERT_EQ(start(attempt).action, TwoFactorHandler::Action::SEND_CHALLENGE);
        ASSERT_EQ(submitCode(attempt, wrong, kNow).action, TwoFactorHandler::Action::CLOSE);
    }

    // What is refused now is the user and not the guess. Even the code they really have is no
    // longer looked at.
    TwoFactorHandler blocked;
    ASSERT_EQ(start(blocked).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_EQ(submitCode(blocked, Totp::code(secret, kNow), kNow).action,
              TwoFactorHandler::Action::CLOSE);

    // And once it has run out, the owner of the secret gets back in.
    const qint64 later =
        kNow + DurationCast<Seconds>(TwoFactorHandler::kFailedAttemptsBlock).count();

    TwoFactorHandler after;
    ASSERT_EQ(start(after).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_EQ(submitCode(after, Totp::code(secret, later), later).action,
              TwoFactorHandler::Action::ACCEPT);
}

//--------------------------------------------------------------------------------------------------
// A block that has run out closes the series it was imposed for: those attempts are paid for.
// The miss after it starts a fresh count of its own instead of stacking onto the old one and
// re-imposing the block at once.
TEST_F(TwoFactorHandlerTest, ExpiredBlockClosesTheSeriesItPunished)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    const QString wrong = wrongCode(secret, kNow);

    for (int i = 0; i < TwoFactorHandler::kMaxFailedAttempts; ++i)
    {
        TwoFactorHandler attempt;
        ASSERT_EQ(start(attempt).action, TwoFactorHandler::Action::SEND_CHALLENGE);
        ASSERT_EQ(submitCode(attempt, wrong, kNow).action, TwoFactorHandler::Action::CLOSE);
    }

    const qint64 later =
        kNow + DurationCast<Seconds>(TwoFactorHandler::kFailedAttemptsBlock).count();
    const QString wrong_later = wrongCode(secret, later);

    // The first miss after the block is refused as a wrong code, not punished as the eleventh
    // of a series already paid for.
    TwoFactorHandler eleventh;
    ASSERT_EQ(start(eleventh, later).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    ASSERT_EQ(submitCode(eleventh, wrong_later, later).action, TwoFactorHandler::Action::CLOSE);

    TwoFactorHandler unblocked;
    EXPECT_EQ(start(unblocked, later).challenge.blocked_seconds, 0);

    // The fresh series carries its own count: the next block takes the full number of misses.
    for (int i = 0; i < TwoFactorHandler::kMaxFailedAttempts - 1; ++i)
    {
        TwoFactorHandler attempt;
        ASSERT_EQ(start(attempt, later).action, TwoFactorHandler::Action::SEND_CHALLENGE);
        ASSERT_EQ(submitCode(attempt, wrong_later, later).action, TwoFactorHandler::Action::CLOSE);
    }

    TwoFactorHandler blocked;
    EXPECT_GT(start(blocked, later).challenge.blocked_seconds, 0);
}

//--------------------------------------------------------------------------------------------------
// The drift window accepts the previous step, but not after a newer one was already consumed.
TEST_F(TwoFactorHandlerTest, CodeOfAnAlreadyConsumedStepIsRefused)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    TwoFactorHandler first;
    ASSERT_EQ(start(first).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    ASSERT_EQ(submitCode(first, Totp::code(secret, kNow), kNow).action,
              TwoFactorHandler::Action::ACCEPT);

    // The code of the previous step is still inside the drift window of |kNow|.
    const qint64 previous_step = kNow - Totp::kDefaultStepSec;

    TwoFactorHandler second;
    ASSERT_EQ(start(second).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_EQ(submitCode(second, Totp::code(secret, previous_step), kNow).action,
              TwoFactorHandler::Action::CLOSE);
}

//--------------------------------------------------------------------------------------------------
// The next step is a code the user has not used yet.
TEST_F(TwoFactorHandlerTest, NextStepIsAccepted)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    TwoFactorHandler first;
    ASSERT_EQ(start(first).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    ASSERT_EQ(submitCode(first, Totp::code(secret, kNow), kNow).action,
              TwoFactorHandler::Action::ACCEPT);

    const qint64 next_step = kNow + Totp::kDefaultStepSec;

    TwoFactorHandler second;
    ASSERT_EQ(start(second).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_EQ(submitCode(second, Totp::code(secret, next_step), next_step).action,
              TwoFactorHandler::Action::ACCEPT);
}

//--------------------------------------------------------------------------------------------------
TEST_F(TwoFactorHandlerTest, EmptyCodeClosesTheSession)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    TwoFactorHandler handler;
    ASSERT_EQ(start(handler).action, TwoFactorHandler::Action::SEND_CHALLENGE);

    proto::router::TwoFactorResponse response; // Neither a code nor a token.
    EXPECT_EQ(handler.handleResponse(db_, caller_, response, "127.0.0.1", kNow).action,
              TwoFactorHandler::Action::CLOSE);
}

//--------------------------------------------------------------------------------------------------
// The state is re-read before the verification: an administrator can reset the secret while the
// prompt is open, and the code of the old secret must not open the session.
TEST_F(TwoFactorHandlerTest, SecretResetWhileThePromptIsOpenClosesTheSession)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    TwoFactorHandler handler;
    ASSERT_EQ(start(handler).action, TwoFactorHandler::Action::SEND_CHALLENGE);

    ASSERT_EQ(db_.resetUserOtp(admin_.entry_id), proto::router::kErrorOk);

    EXPECT_EQ(submitCode(handler, Totp::code(secret, kNow), kNow).action,
              TwoFactorHandler::Action::CLOSE);
}

//--------------------------------------------------------------------------------------------------
// The account can be deleted between the challenge and the answer.
TEST_F(TwoFactorHandlerTest, DeletedUserClosesTheSession)
{
    const RouterUser client = addUser("client",
                                      proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_TRUE(client.isValid());

    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(client.entry_id, secret, 0));

    caller_.user_id = client.entry_id;
    caller_.name = client.name.toStdString();
    caller_.session_type = proto::router::SESSION_TYPE_OPERATOR;

    TwoFactorHandler handler;
    ASSERT_EQ(start(handler).action, TwoFactorHandler::Action::SEND_CHALLENGE);

    ASSERT_EQ(db_.removeUser(client.entry_id), proto::router::kErrorOk);

    EXPECT_EQ(submitCode(handler, Totp::code(secret, kNow), kNow).action,
              TwoFactorHandler::Action::CLOSE);
}

//--------------------------------------------------------------------------------------------------
// The user is gone before the stage even opens.
TEST_F(TwoFactorHandlerTest, StageOfAnUnknownUserClosesTheSession)
{
    caller_.user_id = 12345;
    caller_.name = "ghost";

    TwoFactorHandler handler;
    EXPECT_EQ(start(handler).action, TwoFactorHandler::Action::CLOSE);
}

//--------------------------------------------------------------------------------------------------
// A device token from a previous successful code skips the prompt entirely and does not produce a
// second token.
TEST_F(TwoFactorHandlerTest, ValidTokenSkipsThePrompt)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    TwoFactorHandler first;
    ASSERT_EQ(start(first).action, TwoFactorHandler::Action::SEND_CHALLENGE);

    const TwoFactorHandler::Result accepted = submitCode(first, Totp::code(secret, kNow), kNow);
    ASSERT_EQ(accepted.action, TwoFactorHandler::Action::ACCEPT);
    ASSERT_FALSE(accepted.new_token.empty());

    TwoFactorHandler second;
    ASSERT_EQ(start(second).action, TwoFactorHandler::Action::SEND_CHALLENGE);

    const TwoFactorHandler::Result by_token = submitToken(second, accepted.new_token);

    EXPECT_EQ(by_token.action, TwoFactorHandler::Action::ACCEPT);
    EXPECT_TRUE(by_token.new_token.empty());
    EXPECT_EQ(by_token.token_id, accepted.token_id);
    EXPECT_EQ(tokenCount(admin_.entry_id), 1u);
}

//--------------------------------------------------------------------------------------------------
// The token came out of a verified code, so a login by it is as much of a success as the code
// itself: the refusal and the failed attempts of the old life end with it, the way the challenge
// describes the flag.
TEST_F(TwoFactorHandlerTest, TokenLoginForgetsTheUser)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    TwoFactorHandler first;
    ASSERT_EQ(start(first).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    const TwoFactorHandler::Result accepted = submitCode(first, Totp::code(secret, kNow), kNow);
    ASSERT_EQ(accepted.action, TwoFactorHandler::Action::ACCEPT);
    ASSERT_FALSE(accepted.new_token.empty());

    TwoFactorHandler second;
    ASSERT_EQ(start(second).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    ASSERT_EQ(submitCode(second, wrongCode(secret, kNow), kNow).action,
              TwoFactorHandler::Action::CLOSE);

    TwoFactorHandler third;
    ASSERT_TRUE(start(third).challenge.code_rejected);
    ASSERT_EQ(submitToken(third, accepted.new_token).action, TwoFactorHandler::Action::ACCEPT);

    EXPECT_FALSE(TwoFactorHandlerTestPeer::hasUserState(admin_.entry_id));

    TwoFactorHandler fourth;
    EXPECT_FALSE(start(fourth).challenge.code_rejected);
}

//--------------------------------------------------------------------------------------------------
// A revoked token does not tear the session down: the user still knows their code, so the stage is
// re-opened with the flag that tells the client to drop its local copy.
TEST_F(TwoFactorHandlerTest, RevokedTokenReopensTheStage)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    TwoFactorHandler first;
    ASSERT_EQ(start(first).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    const TwoFactorHandler::Result accepted = submitCode(first, Totp::code(secret, kNow), kNow);
    ASSERT_EQ(accepted.action, TwoFactorHandler::Action::ACCEPT);

    ASSERT_EQ(db_.revokeUserClientDeviceTokens(admin_.entry_id), proto::router::kErrorOk);

    TwoFactorHandler second;
    ASSERT_EQ(start(second).action, TwoFactorHandler::Action::SEND_CHALLENGE);

    const TwoFactorHandler::Result rejected = submitToken(second, accepted.new_token);

    ASSERT_EQ(rejected.action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_EQ(rejected.challenge.mode, proto::router::TWO_FACTOR_MODE_ACTIVE);
    EXPECT_TRUE(rejected.challenge.token_rejected);

    // The code still works, and it issues a new token.
    const qint64 next_step = kNow + Totp::kDefaultStepSec;
    const TwoFactorHandler::Result by_code = submitCode(second, Totp::code(secret, next_step),
                                                        next_step);
    EXPECT_EQ(by_code.action, TwoFactorHandler::Action::ACCEPT);
    EXPECT_FALSE(by_code.new_token.empty());
}

//--------------------------------------------------------------------------------------------------
// The stored state may change while the client sits on its token. A secret that vanished in
// between re-opens the stage as an enrollment, not as a code prompt nothing can answer.
TEST_F(TwoFactorHandlerTest, SecretResetBeforeTheTokenReopensAsEnrollment)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    TwoFactorHandler first;
    ASSERT_EQ(start(first).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    const TwoFactorHandler::Result accepted = submitCode(first, Totp::code(secret, kNow), kNow);
    ASSERT_EQ(accepted.action, TwoFactorHandler::Action::ACCEPT);

    TwoFactorHandler second;
    ASSERT_EQ(start(second).action, TwoFactorHandler::Action::SEND_CHALLENGE);

    // An administrator resets the account while the session sits on the prompt.
    ASSERT_EQ(db_.resetUserOtp(admin_.entry_id), proto::router::kErrorOk);

    const TwoFactorHandler::Result rejected = submitToken(second, accepted.new_token);

    ASSERT_EQ(rejected.action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_EQ(rejected.challenge.mode, proto::router::TWO_FACTOR_MODE_ENROLL);
    EXPECT_TRUE(rejected.challenge.token_rejected);

    // The enrollment completes with a code of the freshly handed out secret.
    const QByteArray new_secret = secretFromUri(rejected.challenge.otpauth_uri);
    ASSERT_FALSE(new_secret.isEmpty());
    EXPECT_EQ(submitCode(second, Totp::code(new_secret, kNow), kNow).action,
              TwoFactorHandler::Action::ACCEPT);
}

//--------------------------------------------------------------------------------------------------
// A token of another account must never authenticate this one.
TEST_F(TwoFactorHandlerTest, TokenOfAnotherUserIsRejected)
{
    const RouterUser client = addUser("client",
                                      proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_TRUE(client.isValid());

    std::string foreign_token;
    qint64 foreign_token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(client.entry_id, "127.0.0.1", &foreign_token,
                                           &foreign_token_id));

    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, Totp::generateSecret(), 0));

    TwoFactorHandler handler;
    ASSERT_EQ(start(handler).action, TwoFactorHandler::Action::SEND_CHALLENGE);

    const TwoFactorHandler::Result rejected = submitToken(handler, foreign_token);

    ASSERT_EQ(rejected.action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_TRUE(rejected.challenge.token_rejected);

    // The token of the other user is untouched - it is theirs, not ours to invalidate.
    EXPECT_EQ(tokenCount(client.entry_id), 1u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(TwoFactorHandlerTest, UnknownTokenIsRejected)
{
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, Totp::generateSecret(), 0));

    TwoFactorHandler handler;
    ASSERT_EQ(start(handler).action, TwoFactorHandler::Action::SEND_CHALLENGE);

    const TwoFactorHandler::Result rejected =
        submitToken(handler, std::string(32, 'x'));

    ASSERT_EQ(rejected.action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_TRUE(rejected.challenge.token_rejected);
}

//--------------------------------------------------------------------------------------------------
// After a rejection the client is told to go to the code prompt, so a second token on the same
// session is not part of the flow. Answering it would leave an authenticated peer free to spin the
// lookup, which takes a write lock of the database, as fast as the network allows.
TEST_F(TwoFactorHandlerTest, SecondTokenOnTheSameSessionClosesIt)
{
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, Totp::generateSecret(), 0));

    TwoFactorHandler handler;
    ASSERT_EQ(start(handler).action, TwoFactorHandler::Action::SEND_CHALLENGE);

    const TwoFactorHandler::Result first = submitToken(handler, std::string(32, 'x'));
    ASSERT_EQ(first.action, TwoFactorHandler::Action::SEND_CHALLENGE);
    ASSERT_TRUE(first.challenge.token_rejected);

    EXPECT_EQ(submitToken(handler, std::string(32, 'y')).action,
              TwoFactorHandler::Action::CLOSE);
}

//--------------------------------------------------------------------------------------------------
// A repeated start() opens a clean stage: the flag of a rejected token belongs to the caller
// that re-opens it, so a token presented after a re-open gets its own attempt instead of being
// cut off as repeated.
TEST_F(TwoFactorHandlerTest, ReopenedStageAllowsATokenAgain)
{
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, Totp::generateSecret(), 0));

    TwoFactorHandler handler;
    ASSERT_EQ(start(handler).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    ASSERT_EQ(submitToken(handler, std::string(32, 'x')).action,
              TwoFactorHandler::Action::SEND_CHALLENGE);

    ASSERT_EQ(start(handler).action, TwoFactorHandler::Action::SEND_CHALLENGE);

    const TwoFactorHandler::Result again = submitToken(handler, std::string(32, 'y'));

    EXPECT_EQ(again.action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_TRUE(again.challenge.token_rejected);
}

//--------------------------------------------------------------------------------------------------
// The code prompt is what the client was sent to, so it still works after the token was refused.
TEST_F(TwoFactorHandlerTest, CodeIsAcceptedAfterATokenWasRejected)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    TwoFactorHandler handler;
    ASSERT_EQ(start(handler).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    ASSERT_EQ(submitToken(handler, std::string(32, 'x')).action,
              TwoFactorHandler::Action::SEND_CHALLENGE);

    const TwoFactorHandler::Result accepted =
        submitCode(handler, Totp::code(secret, kNow), kNow);

    EXPECT_EQ(accepted.action, TwoFactorHandler::Action::ACCEPT);
}

//--------------------------------------------------------------------------------------------------
// The token lives on a sliding window: one that has not been used within its lifetime is not a
// credential any more, and the row goes with it instead of lingering in the table.
TEST_F(TwoFactorHandlerTest, ExpiredTokenIsRejectedAndDropped)
{
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, Totp::generateSecret(), 0));

    std::string token;
    qint64 token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &token, &token_id));

    // Eight days without a single use - one more than the lifetime of a token.
    ASSERT_TRUE(ageToken(token_id, 8));

    TwoFactorHandler handler;
    ASSERT_EQ(start(handler).action, TwoFactorHandler::Action::SEND_CHALLENGE);

    const TwoFactorHandler::Result rejected = submitToken(handler, token);

    ASSERT_EQ(rejected.action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_TRUE(rejected.challenge.token_rejected);
    EXPECT_EQ(storedTokenCount(), 0);
}

//--------------------------------------------------------------------------------------------------
// Every use pushes the lifetime forward, so a device in daily use is never asked for a code again.
TEST_F(TwoFactorHandlerTest, TokenUseRefreshesItsLifetime)
{
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, Totp::generateSecret(), 0));

    std::string token;
    qint64 token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &token, &token_id));

    // Six days: still inside the lifetime.
    ASSERT_TRUE(ageToken(token_id, 6));

    TwoFactorHandler first;
    ASSERT_EQ(start(first).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    ASSERT_EQ(submitToken(first, token).action, TwoFactorHandler::Action::ACCEPT);

    // The use moved the window: three more days must not expire it.
    ASSERT_TRUE(ageToken(token_id, 3));

    TwoFactorHandler second;
    ASSERT_EQ(start(second).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_EQ(submitToken(second, token).action, TwoFactorHandler::Action::ACCEPT);
}

//--------------------------------------------------------------------------------------------------
// A token presented during an enrollment is not a credential: the account being enrolled has no
// secret, so nothing could have issued a usable token for it.
TEST_F(TwoFactorHandlerTest, TokenIsIgnoredDuringEnrollment)
{
    std::string token;
    qint64 token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &token, &token_id));

    TwoFactorHandler handler;
    const TwoFactorHandler::Result challenge = start(handler);
    ASSERT_EQ(challenge.challenge.mode, proto::router::TWO_FACTOR_MODE_ENROLL);

    // The token alone leaves the response without a code: the enrollment cannot be confirmed.
    EXPECT_EQ(submitToken(handler, token).action, TwoFactorHandler::Action::CLOSE);
    EXPECT_TRUE(findUser(admin_.entry_id).otp_secret.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The token list answers "which devices can log in as this user without a code". A token past its
// lifetime cannot, so listing it would answer that question wrongly - and an administrator reading
// it would see access that nobody has.
TEST_F(TwoFactorHandlerTest, ExpiredTokenIsNotListed)
{
    std::string token;
    qint64 token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &token, &token_id));
    ASSERT_EQ(tokenCount(admin_.entry_id), 1u);

    ASSERT_TRUE(ageToken(token_id, 8));

    EXPECT_EQ(tokenCount(admin_.entry_id), 0u);
}

//--------------------------------------------------------------------------------------------------
// A device that is never used again leaves a row nobody can ever present. Issuing the next token
// of the same user is the moment to drop those: without it the table only grows.
TEST_F(TwoFactorHandlerTest, IssuingATokenDropsTheDeadOnesOfTheSameUser)
{
    std::string dead_token;
    qint64 dead_token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &dead_token,
                                           &dead_token_id));
    ASSERT_TRUE(ageToken(dead_token_id, 8));

    std::string token;
    qint64 token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.2", &token, &token_id));

    EXPECT_EQ(storedTokenCount(), 1);
    EXPECT_EQ(tokenCount(admin_.entry_id), 1u);
}

//--------------------------------------------------------------------------------------------------
// Only the dead ones: a user with several devices in use keeps all of them.
TEST_F(TwoFactorHandlerTest, IssuingATokenKeepsTheLiveOnesOfTheSameUser)
{
    std::string first;
    qint64 first_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &first, &first_id));
    ASSERT_TRUE(ageToken(first_id, 6));

    std::string second;
    qint64 second_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.2", &second, &second_id));

    EXPECT_EQ(tokenCount(admin_.entry_id), 2u);
}

//--------------------------------------------------------------------------------------------------
// The sweep stays inside the account being served: one login must not turn into a pass over the
// whole table, and a row of somebody else is invisible to them anyway.
TEST_F(TwoFactorHandlerTest, PruningIsLimitedToTheUserBeingIssuedAToken)
{
    const RouterUser other = addUser("client", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_TRUE(other.isValid());

    std::string foreign_token;
    qint64 foreign_token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(other.entry_id, "127.0.0.1", &foreign_token,
                                           &foreign_token_id));
    ASSERT_TRUE(ageToken(foreign_token_id, 8));

    std::string token;
    qint64 token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.2", &token, &token_id));

    EXPECT_EQ(storedTokenCount(), 2);
}

//--------------------------------------------------------------------------------------------------
// A refusal closes the session that sent the code without an answer, so the challenge of the next
// session is where the refusal is announced. A successful login ends the announcement.
TEST_F(TwoFactorHandlerTest, RefusedCodeIsCarriedByTheNextChallenge)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    TwoFactorHandler first;
    EXPECT_FALSE(start(first).challenge.code_rejected);
    ASSERT_EQ(submitCode(first, wrongCode(secret, kNow), kNow).action,
              TwoFactorHandler::Action::CLOSE);

    TwoFactorHandler second;
    EXPECT_TRUE(start(second).challenge.code_rejected);
    ASSERT_EQ(submitCode(second, Totp::code(secret, kNow), kNow).action,
              TwoFactorHandler::Action::ACCEPT);

    TwoFactorHandler third;
    EXPECT_FALSE(start(third).challenge.code_rejected);
}

//--------------------------------------------------------------------------------------------------
// A replay is refused without counting as a guess, but it is still a code that did not get the
// user in, so the next challenge announces it all the same.
TEST_F(TwoFactorHandlerTest, ReplayedCodeCountsAsRefusedForTheNextChallenge)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    const QString code = Totp::code(secret, kNow);

    TwoFactorHandler first;
    ASSERT_EQ(start(first).action, TwoFactorHandler::Action::SEND_CHALLENGE);
    ASSERT_EQ(submitCode(first, code, kNow).action, TwoFactorHandler::Action::ACCEPT);

    TwoFactorHandler replay;
    EXPECT_FALSE(start(replay).challenge.code_rejected);
    ASSERT_EQ(submitCode(replay, code, kNow).action, TwoFactorHandler::Action::CLOSE);

    TwoFactorHandler next;
    EXPECT_TRUE(start(next).challenge.code_rejected);
}

//--------------------------------------------------------------------------------------------------
// While the block runs the router does not look at codes, so the challenge tells the client how
// long the wait is instead of asking for a code it would throw away. A challenge opened after
// the block has run out carries no wait.
TEST_F(TwoFactorHandlerTest, BlockAnnouncesItsRemainingTimeInTheChallenge)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    const QString wrong = wrongCode(secret, kNow);

    for (int i = 0; i < TwoFactorHandler::kMaxFailedAttempts; ++i)
    {
        TwoFactorHandler attempt;
        ASSERT_EQ(start(attempt).action, TwoFactorHandler::Action::SEND_CHALLENGE);
        ASSERT_EQ(submitCode(attempt, wrong, kNow).action, TwoFactorHandler::Action::CLOSE);
    }

    TwoFactorHandler blocked;
    const TwoFactorHandler::Result challenge = start(blocked);
    ASSERT_EQ(challenge.action, TwoFactorHandler::Action::SEND_CHALLENGE);
    EXPECT_EQ(challenge.challenge.blocked_seconds,
              DurationCast<Seconds>(TwoFactorHandler::kFailedAttemptsBlock).count());
    EXPECT_TRUE(challenge.challenge.code_rejected);

    const qint64 later =
        kNow + DurationCast<Seconds>(TwoFactorHandler::kFailedAttemptsBlock).count();
    EXPECT_EQ(start(blocked, later).challenge.blocked_seconds, 0);
}

//--------------------------------------------------------------------------------------------------
// The block runs on the wall clock, the one the TOTP step needs. A clock that jumps back must not
// stretch the announced wait by the size of the jump: the remainder is capped by the full length
// of the block, and the re-seated block runs out counted from the new clock.
TEST_F(TwoFactorHandlerTest, ClockJumpBackDoesNotStretchTheBlock)
{
    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 0));

    const QString wrong = wrongCode(secret, kNow);

    for (int i = 0; i < TwoFactorHandler::kMaxFailedAttempts; ++i)
    {
        TwoFactorHandler attempt;
        ASSERT_EQ(start(attempt).action, TwoFactorHandler::Action::SEND_CHALLENGE);
        ASSERT_EQ(submitCode(attempt, wrong, kNow).action, TwoFactorHandler::Action::CLOSE);
    }

    const qint64 full = DurationCast<Seconds>(TwoFactorHandler::kFailedAttemptsBlock).count();
    const qint64 jumped_back = kNow - 3600;

    TwoFactorHandler blocked;
    EXPECT_EQ(start(blocked, jumped_back).challenge.blocked_seconds, full);

    TwoFactorHandler after;
    EXPECT_EQ(start(after, jumped_back + full).challenge.blocked_seconds, 0);
}
