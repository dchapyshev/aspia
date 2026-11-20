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

#ifndef ROUTER_USER_STORE_H
#define ROUTER_USER_STORE_H

#include "router/database_store.h"

class UserStore final : public DatabaseStore
{
public:
    explicit UserStore(Database& db);

    // Fills |users| with the requested page of the user list, ordered by entry id. The page is
    // mandatory, so a zero count is refused along with a negative offset or a count over the cap.
    // A database error yields an empty list - a partial one never passes for a complete page.
    // Returns a proto::router error code.
    std::string_view list(qint64 offset, qint64 count, std::vector<RouterUser>* users) const;

    // Reads the total user count into |count|, for the pagination of the caller. Returns
    // kErrorInternalError when the answer could not be read - a zero count from a failed query
    // would truncate the pagination.
    std::string_view count(qint64* count) const;

    // Adds a user record. Returns a proto::router error code.
    std::string_view add(const RouterUser& user);

    // Updates a user record. Empty salt/verifier keep the stored credentials, a zero session mask
    // keeps the stored level, an empty address keeps the stored one. A password rotation revokes
    // every device token of the user. |password_changed| and |sessions_changed|, if not null,
    // report whether the rotation and the level change happened. Returns a proto::router error code.
    std::string_view modify(const RouterUser& user, bool* password_changed = nullptr, bool* sessions_changed = nullptr);

    // Removes a user; its workspace_access rows go with it by cascade, and the revision of every
    // affected workspace is bumped in the same transaction.
    std::string_view remove(qint64 entry_id);

    // Reads a user record into |user|. Returns kErrorNotFound when no such user exists and
    // kErrorInternalError when the answer could not be read: a failed read must not pass for a
    // missing record, so the two are separate answers and neither can be dropped by accident.
    std::string_view find(const QString& username, RouterUser* user) const;
    std::string_view find(qint64 entry_id, RouterUser* user) const;
    std::string_view findByGuid(const QString& guid, RouterUser* user) const;

    // Commits the user's TOTP enrollment: writes the shared secret and seeds otp_counter with
    // the step that produced the confirmation code. A non-empty secret column is what activates
    // 2FA for the user. Succeeds only while the user still has no confirmed secret, so parallel
    // enrollment attempts cannot overwrite each other.
    bool setOtp(qint64 user_id, const QByteArray& secret, quint64 counter);

    // Resets the user's TOTP state: clears the secret, zeroes the replay counter and drops every
    // device token of the user, all in one transaction. The tokens were issued against the secret
    // being cleared, so a state where they outlive it would let a device in without a code. The
    // next login triggers self-enrollment. Returns kErrorNotFound if the user row is absent.
    std::string_view resetOtp(qint64 user_id);

    // Atomically consumes a TOTP step. kErrorOk only if |counter| is newer than the value
    // currently stored in the database, so parallel sessions cannot accept the same code.
    // kErrorNotFound when no row matched: the step was consumed meanwhile or the user is gone.
    // kErrorInternalError when the database refused the write.
    std::string_view consumeOtpCounter(qint64 user_id, quint64 counter);
};

#endif // ROUTER_USER_STORE_H
