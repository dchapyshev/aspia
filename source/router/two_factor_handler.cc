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

#include "router/two_factor_handler.h"

#include "base/logging.h"
#include "base/crypto/totp.h"
#include "base/peer/router_user.h"
#include "router/database.h"

namespace {

const char kOtpIssuer[] = "Aspia Router";

} // namespace

//--------------------------------------------------------------------------------------------------
TwoFactorHandler::Result TwoFactorHandler::start(Database& database, const RequestCaller& caller)
{
    Result result;

    const RouterUser user = database.findUser(caller.user_id);
    if (!user.isValid())
    {
        // SRP already validated the user; reaching this branch implies the row vanished between
        // authentication and this stage.
        LOG(WARNING) << "Authenticated user" << caller.name << "disappeared from database";
        result.action = Action::CLOSE;
        return result;
    }

    user_otp_secret_ = user.otp_secret;
    user_otp_counter_ = user.otp_counter;

    result.action = Action::SEND_CHALLENGE;

    if (user_otp_secret_.isEmpty())
    {
        // First login or after an administrator reset. The tentative secret is handed to the
        // client and only reaches the database once the user confirms it with a valid code, so an
        // abandoned dialog leaves the user un-enrolled.
        tentative_otp_secret_ = Totp::generateSecret();

        result.challenge.mode = proto::router::TWO_FACTOR_MODE_ENROLL;
        result.challenge.otpauth_uri =
            Totp::buildUri(kOtpIssuer, caller.name, tentative_otp_secret_).toStdString();
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
        const RouterUser user = database.findUser(caller.user_id);
        if (!user.isValid())
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

    const QString code = QString::fromStdString(response.totp_code());
    quint64 matched_counter = 0;
    if (!Totp::verify(secret, code, now, Totp::kDefaultStepSec, Totp::kDefaultDigits,
                      Totp::kDefaultWindowSteps, &matched_counter))
    {
        LOG(INFO) << "Invalid TOTP code for user" << caller.name << ". Closing connection";
        result.action = Action::CLOSE;
        return result;
    }

    // Replay protection: refuse any code whose step has already been consumed. ENROLL starts
    // from counter 0, so the first valid code (any positive step) is accepted.
    if (!enroll && matched_counter <= user_otp_counter_)
    {
        LOG(INFO) << "Replayed TOTP code for user" << caller.name << ". Closing connection";
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
