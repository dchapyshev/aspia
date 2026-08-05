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

#ifndef ROUTER_TWO_FACTOR_HANDLER_H
#define ROUTER_TWO_FACTOR_HANDLER_H

#include <QByteArray>

#include <string>
#include <string_view>

#include "proto/router_client.h"
#include "router/request_caller.h"

class Database;

// The two-factor stage of a client session: the self-enrollment of a user that has no secret yet,
// the verification of a TOTP code and the bearer "remember this device" token issued for it. It
// holds the state of the stage and needs only the database, so the whole flow - including the
// replay protection and the rules that tear a session down - is testable; the session only sends
// the messages the returned decision describes.
class TwoFactorHandler
{
public:
    struct Challenge
    {
        proto::router::TwoFactorMode mode = proto::router::TWO_FACTOR_MODE_ACTIVE;

        // ENROLL only: the otpauth:// URI the client turns into a QR code.
        std::string otpauth_uri;

        // The device token the client presented is dead (revoked, expired, or issued to somebody
        // else). Tells the client to drop its local copy instead of presenting it again.
        bool token_rejected = false;
    };

    enum class Action
    {
        SEND_CHALLENGE, // Ask the client for a code; |challenge| is what to send.
        ACCEPT,         // The stage passed. |new_token| may carry a freshly issued device token.
        CLOSE           // Tear the session down.
    };

    struct Result
    {
        Action action = Action::CLOSE;
        Challenge challenge;

        // ACCEPT only. Empty when the client authenticated with a token it already had: an empty
        // message cannot be sent over the wire, so in that case the success is signalled by the
        // user keys alone.
        std::string new_token;
        qint64 token_id = 0;
    };

    // Opens the stage. Called on a fresh connection and again when the session re-authenticates
    // (its own password change revokes every device token, this one included).
    Result start(Database& database, const RequestCaller& caller);

    // Handles the answer of the client. |now| is the wall clock in seconds - the TOTP step comes
    // from it - and |address| is stored with the issued device token.
    Result handleResponse(Database& database, const RequestCaller& caller,
                          const proto::router::TwoFactorResponse& response,
                          std::string_view address, qint64 now);

private:
    // Set while the user is being walked through the enrollment: it reaches the database only
    // once the user confirms it with a valid code, so an abandoned dialog leaves it un-enrolled.
    QByteArray tentative_otp_secret_;

    // The stored state as it was when the challenge was sent. Re-read before every verification:
    // an administrator can reset the secret or delete the user while the prompt is open.
    QByteArray user_otp_secret_;
    quint64 user_otp_counter_ = 0;

    // The client already presented a token and was told to go to the code prompt instead.
    bool token_rejected_ = false;
};

#endif // ROUTER_TWO_FACTOR_HANDLER_H
