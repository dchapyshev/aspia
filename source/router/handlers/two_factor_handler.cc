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
std::unordered_map<qint64, TwoFactorHandler::Attempts> TwoFactorHandler::attempts_;

//--------------------------------------------------------------------------------------------------
TwoFactorHandler::Result TwoFactorHandler::start(Database& database, const RequestCaller& caller)
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

    // A re-opened stage is a fresh one, and the token the client holds is not the one it presented
    // before: the password change that re-opens the stage issues a new one.
    token_rejected_ = false;

    result.action = Action::SEND_CHALLENGE;

    if (user_otp_secret_.isEmpty())
    {
        // First login or after an administrator reset. The tentative secret is handed to the
        // client and only reaches the database once the user confirms it with a valid code, so an
        // abandoned dialog leaves the user un-enrolled.
        tentative_otp_secret_ = Totp::generateSecret();

        result.challenge.mode = proto::router::TWO_FACTOR_MODE_ENROLL;
        result.challenge.otpauth_uri =
            Totp::buildUri(kOtpIssuer, QString::fromStdString(caller.name),
                           tentative_otp_secret_).toStdString();
    }
    else
    {
        // The user is enrolled, so an enrollment left over from an earlier round of this stage
        // must not be verified against.
        tentative_otp_secret_.clear();
        result.challenge.mode = proto::router::TWO_FACTOR_MODE_ACTIVE;
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
        if (!database.findClientDeviceToken(token, &stored_user_id, &token_id) ||
            stored_user_id != caller.user_id)
        {
            // The presented token is gone or owned by someone else (revoked, password change,
            // database wiped, expired). The user still has a valid TOTP secret, so instead of
            // tearing the connection down we re-open the stage and ask for a code.
            LOG(INFO) << "Device token rejected for user" << caller.name << "- asking for TOTP";

            token_rejected_ = true;
            result.action = Action::SEND_CHALLENGE;
            result.challenge.mode = proto::router::TWO_FACTOR_MODE_ACTIVE;
            result.challenge.token_rejected = true;
            return result;
        }

        database.touchClientDeviceToken(token, address);

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
        result.action = Action::CLOSE;
        return result;
    }

    // Replay protection: refuse any code whose step has already been consumed. ENROLL starts
    // from counter 0, so the first valid code (any positive step) is accepted.
    if (!enroll && matched_counter <= user_otp_counter_)
    {
        LOG(INFO) << "Replayed TOTP code for user" << caller.name << ". Closing connection";
        registerFailedAttempt(caller.user_id, now);
        result.action = Action::CLOSE;
        return result;
    }

    if (enroll)
    {
        if (!database.setUserOtp(caller.user_id, tentative_otp_secret_, matched_counter))
        {
            // Another session of the same user completed the enrollment meanwhile, or the record
            // is gone: the secret this client scanned is not the one on file.
            LOG(ERROR) << "Failed to persist OTP secret for user" << caller.name
                       << ". Closing connection";
            result.action = Action::CLOSE;
            return result;
        }

        tentative_otp_secret_.clear();
    }
    else
    {
        if (!database.consumeUserOtpCounter(caller.user_id, matched_counter))
        {
            LOG(INFO) << "TOTP counter was already consumed for user" << caller.name
                      << ". Closing connection";
            result.action = Action::CLOSE;
            return result;
        }
    }

    // A code that came out of the secret proves the peer holds what only the user holds, so the
    // series of failed attempts ends here.
    resetAttempts(caller.user_id);

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
bool TwoFactorHandler::isBlockedAttempt(qint64 user_id, qint64 now)
{
    const auto it = attempts_.find(user_id);
    return it != attempts_.end() && now < it->second.blocked_until;
}

//--------------------------------------------------------------------------------------------------
// static
void TwoFactorHandler::registerFailedAttempt(qint64 user_id, qint64 now)
{
    Attempts& attempts = attempts_[user_id];

    // A block that has run out closes the series it was imposed for. Those attempts are paid for
    // already and the next one starts counting from scratch.
    if (attempts.blocked_until && now >= attempts.blocked_until)
    {
        attempts.failures = 0;
        attempts.blocked_until = 0;
    }

    ++attempts.failures;

    if (attempts.failures >= kMaxFailedAttempts)
        attempts.blocked_until = now + DurationCast<Seconds>(kFailedAttemptsBlock).count();
}

//--------------------------------------------------------------------------------------------------
// static
void TwoFactorHandler::resetAttempts(qint64 user_id)
{
    attempts_.erase(user_id);
}
