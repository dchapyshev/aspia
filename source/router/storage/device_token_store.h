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

#ifndef ROUTER_STORAGE_DEVICE_TOKEN_STORE_H
#define ROUTER_STORAGE_DEVICE_TOKEN_STORE_H

#include <vector>

#include "router/storage/database_store.h"

// Metadata for a client device token returned to admin callers. Intentionally omits the token
// hash and any other material that could identify the token outside the router.
struct DeviceToken
{
    qint64 token_id     = 0; // client_device_tokens.token_id. Opaque to admins.
    qint64 created_at   = 0; // Unix seconds.
    qint64 last_used_at = 0; // Unix seconds.
    std::string address;     // Address of the session at last use (or issue, if untouched).
};

// Client device tokens (bearer "remember this device" credentials issued during client sessions).
class DeviceTokenStore final : public DatabaseStore
{
public:
    explicit DeviceTokenStore(Database& db);

    // Issues a new client device token for |user_id|. |address| is stored as the address of
    // the session that requested the issue (shown to admins). On success writes the freshly
    // generated opaque token into |token| and, when not null, the router-side row id into
    // |token_id|. Returns false on database error.
    bool issue(qint64 user_id, std::string_view address, std::string* token, qint64* token_id = nullptr);

    // Looks up a token by its opaque value. On success writes the owner user_id into |user_id|
    // and, when not null, the router-side row id into |token_id|. Returns kErrorNotFound for a
    // token that is absent or expired (the expired row is dropped), kErrorInternalError when the
    // database did not answer.
    std::string_view find(std::string_view token, qint64* user_id, qint64* token_id = nullptr);

    // Updates the token's last_used_at timestamp and last seen |address|. Called after a
    // successful token lookup.
    bool touch(std::string_view token, std::string_view address);

    // Removes the given tokens by their router-side row ids, but only those belonging to
    // |user_id|. The user_id check is defense in depth - the admin channel is already
    // privileged, but the extra predicate prevents a malformed request from touching another
    // user's row. All deletes run in one transaction: if any token id does not match a row the
    // whole batch is rolled back. Returns kErrorNotFound if a token is missing, kErrorInternalError
    // on database failure, kErrorOk when every token was revoked (or the list was empty).
    std::string_view revoke(qint64 user_id, const std::vector<qint64>& token_ids);

    // Removes every device token of |user_id| in a single statement. Succeeds for an existing user
    // with no tokens, but returns kErrorNotFound if the user row is absent.
    std::string_view revokeAll(qint64 user_id);

    // Fills |tokens| with the device tokens of |user_id| that are still inside their lifetime;
    // an expired row is not a credential any more and is not listed. The router never exposes
    // token material to admins - only the opaque numeric id and timestamp metadata. Returns
    // kErrorNotFound if the user row is absent; a user with no tokens answers kErrorOk and an
    // empty list.
    std::string_view list(qint64 user_id, std::vector<DeviceToken>* tokens) const;
};

#endif // ROUTER_STORAGE_DEVICE_TOKEN_STORE_H
