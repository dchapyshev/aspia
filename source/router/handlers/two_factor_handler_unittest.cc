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
#include "router/router_test_base.h"

// The count of failed attempts outlives the sessions on purpose, so it also outlives a test case.
class TwoFactorHandlerTestPeer
{
public:
    static void forgetFailedAttempts() { TwoFactorHandler::attempts_.clear(); }
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

        TwoFactorHandlerTestPeer::forgetFailedAttempts();
    }

    TwoFactorHandler::Result start(TwoFactorHandler& handler)
    {
        return handler.start(db_, caller_);
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
        if (!db_.listClientDeviceTokens(user_id, &tokens))
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
    ASSERT_EQ(start(handler).action, TwoFactorHandler::Action::SEND_CHALLENGE);

    EXPECT_EQ(submitCode(handler, "000000", kNow).action,
              TwoFactorHandler::Action::CLOSE);
    EXPECT_TRUE(findUser(admin_.entry_id).otp_secret.isEmpty());
    EXPECT_EQ(tokenCount(admin_.entry_id), 0u);
}

//--------------------------------------------------------------------------------------------------
// Another session of the same user finished the enrollment while this one was still at the prompt:
// the secret this client scanned is not the one on file, so the session goes away instead of
// overwriting it.
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

    ASSERT_EQ(db_.clearUserOtp(admin_.entry_id), proto::router::kErrorOk);

    EXPECT_EQ(submitCode(handler, Totp::code(secret, kNow), kNow).action,
              TwoFactorHandler::Action::CLOSE);
}

//--------------------------------------------------------------------------------------------------
// The account can be deleted between the challenge and the answer.
TEST_F(TwoFactorHandlerTest, DeletedUserClosesTheSession)
{
    const RouterUser client = addUser("client",
                                      proto::router::SESSION_TYPE_CLIENT);
    ASSERT_TRUE(client.isValid());

    const QByteArray secret = Totp::generateSecret();
    ASSERT_TRUE(db_.setUserOtp(client.entry_id, secret, 0));

    caller_.user_id = client.entry_id;
    caller_.name = client.name.toStdString();
    caller_.session_type = proto::router::SESSION_TYPE_CLIENT;

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
// A token of another account must never authenticate this one.
TEST_F(TwoFactorHandlerTest, TokenOfAnotherUserIsRejected)
{
    const RouterUser client = addUser("client",
                                      proto::router::SESSION_TYPE_CLIENT);
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
// The stage re-opens when the session rotates its own password, which issues the client a new
// token. That token gets its own attempt.
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
    EXPECT_EQ(tokenCount(admin_.entry_id), 0u);
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
    const RouterUser other = addUser("client", proto::router::SESSION_TYPE_CLIENT);
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
