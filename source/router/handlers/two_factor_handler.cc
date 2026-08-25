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

#include "base/logging.h"
#include "base/crypto/totp.h"
#include "base/peer/router_user.h"
#include "proto/router_constants.h"
#include "router/database.h"

namespace {

const char kOtpIssuer[] = "Aspia Router";

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::unordered_map<qint64, TwoFactorHandler::UserState> TwoFactorHandler::user_states_;

//--------------------------------------------------------------------------------------------------
TwoFactorHandler::Result TwoFactorHandler::start(Database& database, const RequestCaller& caller, qint64 now)
{
    Result result;

    RouterUser user;
    if (database.findUser(caller.user_id, &user) != proto::router::kErrorOk)
    {
        // SRP already validated the user; reaching this branch implies the row vanished between
        // authentication and this stage (or the database stopped answering).
        LOG(WARNING) << "Authenticated user" << caller.name << "disappeared from database";
        result.action = Action::CLOSE;
        return result;
    }

    user_otp_secret_ = user.otp_secret;
    user_otp_counter_ = user.otp_counter;

    // A re-opened stage starts clean. The one caller that re-opens it, the rejected-token
    // branch, raises the flag again itself after this returns.
    token_rejected_ = false;

    result.action = Action::SEND_CHALLENGE;
    result.challenge.code_rejected = isCodeRejected(caller.user_id);

    if (user_otp_secret_.isEmpty())
    {
        // First login or after an administrator reset. The secret is kept per user, so the QR
        // code survives the reconnects of the stage; the database gets it only once a valid code
        // confirms it, so an abandoned dialog leaves the user un-enrolled.
        tentative_otp_secret_ = tentativeSecret(caller.user_id);

        result.challenge.mode = proto::router::TWO_FACTOR_MODE_ENROLL;
        result.challenge.otpauth_uri =
            Totp::buildUri(kOtpIssuer, user.name, tentative_otp_secret_).toStdString();
    }
    else
    {
        // The user is enrolled, so an enrollment left over from an earlier round of this stage
        // must not be verified against.
        tentative_otp_secret_.clear();
        dropTentativeSecret(caller.user_id);
        result.challenge.mode = proto::router::TWO_FACTOR_MODE_ACTIVE;
        result.challenge.blocked_seconds = blockedSecondsLeft(caller.user_id, now);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
TwoFactorHandler::Result TwoFactorHandler::handleResponse(
    Database& database, const RequestCaller& caller,
    const proto::router::TwoFactorResponse& response, std::string_view address, qint64 now)
{
    Result result;

    const bool enroll = !tentative_otp_secret_.isEmpty();

    if (!enroll && !response.token().empty())
    {
        // One token per session. The client was already told to drop it and ask the user for a
        // code, so a second one is not the flow going wrong but a peer spinning a lookup that
        // takes a write lock of the database.
        if (token_rejected_)
        {
            LOG(INFO) << "Repeated device token from user" << caller.name << ". Closing connection";
            result.action = Action::CLOSE;
            return result;
        }

        // Token path: the client presented a previously issued bearer token. Validate by lookup
        // and check that its owner is the user that just passed SRP.
        const std::string_view token = response.token();

        qint64 stored_user_id = 0;
        qint64 token_id = 0;
        const std::string_view error_code =
            database.findClientDeviceToken(token, &stored_user_id, &token_id);

        if (error_code != proto::router::kErrorOk && error_code != proto::router::kErrorNotFound)
        {
            // The database gave no verdict, so the client must not be told to drop the token.
            // It keeps its copy and presents it again on the next connection.
            LOG(ERROR) << "Device token lookup failed for user" << caller.name << ". Closing connection";
            result.action = Action::CLOSE;
            return result;
        }

        if (error_code != proto::router::kErrorOk || stored_user_id != caller.user_id)
        {
            // The presented token is gone or owned by someone else. Re-opening the stage
            // re-reads the stored state, so a secret that vanished in between leads to the
            // enrollment and not to a code prompt nothing can answer.
            LOG(INFO) << "Device token rejected for user" << caller.name << "- asking for TOTP";

            Result reopened = start(database, caller, now);
            if (reopened.action == Action::SEND_CHALLENGE)
            {
                token_rejected_ = true;
                reopened.challenge.token_rejected = true;
            }
            return reopened;
        }

        // Failure to refresh the sliding lifetime is non-fatal. The token was judged valid, so
        // the user is let in and the lifetime stays where the previous use left it.
        if (!database.touchClientDeviceToken(token, address))
            LOG(WARNING) << "Failed to touch device token for user" << caller.name;

        // The token came out of a verified code, so this login proves the same thing the code
        // does and ends the stored state of the user the same way.
        forgetUser(caller.user_id);

        result.action = Action::ACCEPT;
        result.token_id = token_id;
        return result;
    }

    if (!enroll)
    {
        // The secret and the counter are re-read instead of trusting the copies cached when the
        // challenge was sent: an administrator can reset the OTP or delete the user while the
        // session sits at the prompt, and the verification below must see that.
        RouterUser user;
        if (database.findUser(caller.user_id, &user) != proto::router::kErrorOk)
        {
            LOG(INFO) << "User" << caller.name << "is gone. Closing connection";
            result.action = Action::CLOSE;
            return result;
        }

        user_otp_secret_ = user.otp_secret;
        user_otp_counter_ = user.otp_counter;
    }

    const QByteArray& secret = enroll ? tentative_otp_secret_ : user_otp_secret_;

    if (secret.isEmpty() || response.totp_code().empty())
    {
        LOG(INFO) << "Empty TOTP code or no OTP secret for user" << caller.name
                  << ". Closing connection";
        result.action = Action::CLOSE;
        return result;
    }

    // During enrollment the client confirms a secret it has just been handed and there is nothing
    // to guess, so only the prompt of an enrolled user is bounded.
    if (!enroll && isBlockedAttempt(caller.user_id, now))
    {
        LOG(INFO) << "Too many failed TOTP attempts for user" << caller.name << ". Closing connection";
        markCodeRejected(caller.user_id);
        result.action = Action::CLOSE;
        return result;
    }

    const QString code = QString::fromStdString(response.totp_code());
    quint64 matched_counter = 0;
    if (!Totp::verify(secret, code, now, Totp::kDefaultStepSec, Totp::kDefaultDigits,
                      Totp::kDefaultWindowSteps, &matched_counter))
    {
        LOG(INFO) << "Invalid TOTP code for user" << caller.name << ". Closing connection";
        if (!enroll)
            registerFailedAttempt(caller.user_id, now);
        markCodeRejected(caller.user_id);
        result.action = Action::CLOSE;
        return result;
    }

    // Replay protection: refuse any code whose step has already been consumed. ENROLL starts
    // from counter 0, so the first valid code (any positive step) is accepted.
    //
    // Not counted as a failed attempt. Getting here takes a code that came out of the secret, so
    // it is not a guess, and the block exists to stop guessing. A session dropped right after a
    // login brings the user back to the prompt while their code is still on screen, and typing it
    // again must not spend the attempts they need.
    if (!enroll && matched_counter <= user_otp_counter_)
    {
        LOG(INFO) << "Replayed TOTP code for user" << caller.name << ". Closing connection";
        markCodeRejected(caller.user_id);
        result.action = Action::CLOSE;
        return result;
    }

    if (enroll)
    {
        if (!database.setUserOtp(caller.user_id, tentative_otp_secret_, matched_counter))
        {
            // Another session of the same user completed the enrollment meanwhile, or the record
            // is gone, or the write failed. The code itself was right, so the refusal flag stays
            // down: the next challenge asks about the stored state without blaming the user.
            LOG(ERROR) << "Failed to persist OTP secret for user" << caller.name
                       << ". Closing connection";
            result.action = Action::CLOSE;
            return result;
        }

        tentative_otp_secret_.clear();
    }
    else
    {
        const std::string_view error_code =
            database.consumeUserOtpCounter(caller.user_id, matched_counter);
        if (error_code == proto::router::kErrorNotFound)
        {
            // The step was consumed by another session after this one took its snapshot (or the
            // user is gone), so the predicate of the database is the replay check that caught it.
            LOG(INFO) << "TOTP counter was already consumed for user" << caller.name
                      << ". Closing connection";
            markCodeRejected(caller.user_id);
            result.action = Action::CLOSE;
            return result;
        }
        if (error_code != proto::router::kErrorOk)
        {
            // The code was right and the database refused the write, which is not a refusal of
            // the code.
            LOG(ERROR) << "Failed to consume OTP counter for user" << caller.name
                       << ". Closing connection";
            result.action = Action::CLOSE;
            return result;
        }
    }

    // A code that came out of the secret proves the peer holds what only the user holds, so the
    // stored state of the user ends here, failed attempts and shared secret alike.
    forgetUser(caller.user_id);

    // Any successful TOTP submission produces a fresh bearer token. Failure to persist the token
    // is non-fatal: the user is still let in, they will be prompted for TOTP again next time.
    std::string new_token;
    qint64 new_token_id = 0;
    if (!database.issueClientDeviceToken(caller.user_id, address, &new_token, &new_token_id))
        LOG(WARNING) << "Failed to issue device token for user" << caller.name;

    result.action = Action::ACCEPT;
    result.new_token = std::move(new_token);
    result.token_id = new_token_id;
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
void TwoFactorHandler::forgetUser(qint64 user_id)
{
    user_states_.erase(user_id);
}

//--------------------------------------------------------------------------------------------------
// static
bool TwoFactorHandler::isBlockedAttempt(qint64 user_id, qint64 now)
{
    return blockedSecondsLeft(user_id, now) > 0;
}

//--------------------------------------------------------------------------------------------------
// static
qint64 TwoFactorHandler::blockedSecondsLeft(qint64 user_id, qint64 now)
{
    const auto it = user_states_.find(user_id);
    if (it == user_states_.end() || now >= it->second.blocked_until)
        return 0;

    // The block runs on the wall clock, the one the TOTP step needs. A clock that jumped back
    // would stretch the remainder by the size of the jump, so it is capped by the full length
    // of the block.
    const qint64 full = DurationCast<Seconds>(kFailedAttemptsBlock).count();
    if (it->second.blocked_until - now > full)
        it->second.blocked_until = now + full;

    return it->second.blocked_until - now;
}

//--------------------------------------------------------------------------------------------------
// static
bool TwoFactorHandler::isCodeRejected(qint64 user_id)
{
    const auto it = user_states_.find(user_id);
    return it != user_states_.end() && it->second.code_rejected;
}

//--------------------------------------------------------------------------------------------------
// static
void TwoFactorHandler::markCodeRejected(qint64 user_id)
{
    user_states_[user_id].code_rejected = true;
}

//--------------------------------------------------------------------------------------------------
// static
void TwoFactorHandler::registerFailedAttempt(qint64 user_id, qint64 now)
{
    UserState& state = user_states_[user_id];

    // A block that has run out closes the series it was imposed for. Those attempts are paid for
    // already and the next one starts counting from scratch.
    if (state.blocked_until && now >= state.blocked_until)
    {
        state.failures = 0;
        state.blocked_until = 0;
    }

    ++state.failures;

    if (state.failures >= kMaxFailedAttempts)
        state.blocked_until = now + DurationCast<Seconds>(kFailedAttemptsBlock).count();
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray TwoFactorHandler::tentativeSecret(qint64 user_id)
{
    QByteArray& secret = user_states_[user_id].tentative_secret;
    if (secret.isEmpty())
        secret = Totp::generateSecret();
    return secret;
}

//--------------------------------------------------------------------------------------------------
// static
void TwoFactorHandler::dropTentativeSecret(qint64 user_id)
{
    const auto it = user_states_.find(user_id);
    if (it != user_states_.end())
        it->second.tentative_secret.clear();
}
