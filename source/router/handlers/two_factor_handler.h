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

#ifndef ROUTER_HANDLERS_TWO_FACTOR_HANDLER_H
#define ROUTER_HANDLERS_TWO_FACTOR_HANDLER_H

#include <QByteArray>

#include <string>
#include <string_view>
#include <unordered_map>

#include "base/time_types.h"
#include "proto/router_client.h"
#include "router/handlers/request_caller.h"

class Database;

// The two-factor stage of a client session: the self-enrollment of a user that has no secret yet,
// the verification of a TOTP code and the bearer "remember this device" token issued for it. It
// holds the state of the stage and needs only the database, so the whole flow - including the
// replay protection and the rules that tear a session down - is testable; the session only sends
// the messages the returned decision describes.
class TwoFactorHandler
{
public:
    TwoFactorHandler() = default;
    ~TwoFactorHandler() = default;

    struct Challenge
    {
        proto::router::TwoFactorMode mode = proto::router::TWO_FACTOR_MODE_ACTIVE;

        // ENROLL only: the otpauth:// URI the client turns into a QR code.
        std::string otpauth_uri;

        // The device token the client presented is dead (revoked, expired, or issued to somebody
        // else). Tells the client to drop its local copy instead of presenting it again.
        bool token_rejected = false;

        // The last code submitted for this account since its last successful login was refused.
        // The refusal itself closes the session without an answer, so this is how the next
        // session learns of it.
        bool code_rejected = false;

        // ACTIVE only. Seconds left in the block imposed after too many failed attempts, zero
        // when the account is not blocked.
        qint64 blocked_seconds = 0;
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

    // Failed attempts a user may make before the router stops verifying their codes at all, and
    // how long that refusal lasts afterwards.
    static constexpr int kMaxFailedAttempts = 10;
    static constexpr Minutes kFailedAttemptsBlock { 15 };

    // Opens the stage. Called on a fresh connection and again when a presented device token is
    // rejected, because the stored state may have changed since the challenge was sent. |now| is
    // the wall clock in seconds; the challenge carries the time left in a running block.
    Result start(Database& database, const RequestCaller& caller, qint64 now);

    // Handles the answer of the client. |now| is the wall clock in seconds - the TOTP step comes
    // from it - and |address| is stored with the issued device token.
    Result handleResponse(Database& database, const RequestCaller& caller,
                          const proto::router::TwoFactorResponse& response,
                          std::string_view address, qint64 now);

    // Drops the stored state of the user, so the account starts its two-factor life anew.
    // Called when an administrator resets the OTP of the user, rotates their password or
    // deletes the user.
    static void forgetUser(qint64 user_id);

private:
    // The state of one user, surviving their sessions and reconnects. A wrong or replayed code
    // takes the session down together with the handler, so whoever guesses just reconnects and
    // no session sees the whole series. Six digits are few enough to walk through that way. The
    // tentative secret lives here so the QR code survives the reconnects of the stage.
    struct UserState
    {
        int failures = 0;
        qint64 blocked_until = 0;
        bool code_rejected = false;
        QByteArray tentative_secret;
    };

    static bool isBlockedAttempt(qint64 user_id, qint64 now);
    static qint64 blockedSecondsLeft(qint64 user_id, qint64 now);
    static bool isCodeRejected(qint64 user_id);
    static void markCodeRejected(qint64 user_id);
    static void registerFailedAttempt(qint64 user_id, qint64 now);
    static QByteArray tentativeSecret(qint64 user_id);
    static void dropTentativeSecret(qint64 user_id);

    // Keyed by user and not by address. Reaching the code prompt takes a completed SRP exchange,
    // so nobody can run somebody else's account into the block and nobody shakes off their own
    // count by changing address. Only the thread of the client worker touches this.
    static std::unordered_map<qint64, UserState> user_states_;

    // Set while the user is being walked through the enrollment: it reaches the database only
    // once the user confirms it with a valid code, so an abandoned dialog leaves it un-enrolled.
    QByteArray tentative_otp_secret_;

    // The stored state as it was when the challenge was sent. Re-read before every verification:
    // an administrator can reset the secret or delete the user while the prompt is open.
    QByteArray user_otp_secret_;
    quint64 user_otp_counter_ = 0;

    // The client already presented a token and was told to go to the code prompt instead.
    bool token_rejected_ = false;

    friend class TwoFactorHandlerTestPeer;
    Q_DISABLE_COPY_MOVE(TwoFactorHandler)
};

#endif // ROUTER_HANDLERS_TWO_FACTOR_HANDLER_H
