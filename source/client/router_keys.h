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

#ifndef CLIENT_ROUTER_KEYS_H
#define CLIENT_ROUTER_KEYS_H

#include <QString>

#include <set>
#include <unordered_map>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_string.h"

namespace proto::router {
class ChangePasswordRequest;
class User;
class UserKeys;
} // namespace proto::router

// The identity of a router session and the key material it holds: who we are, the private key the
// password opens and a group-key cryptor per workspace we can read. Everything encrypted in the
// records of the router is sealed to these keys; whoever decodes or builds such a record borrows
// the cryptor from here.
class RouterKeys
{
public:
    RouterKeys() = default;
    ~RouterKeys() = default;

    enum class Result
    {
        OK,                       // Identity and workspace keys are loaded.
        PASSWORD_CHANGE_REQUIRED, // The record has no wrapped private key yet.
        DECRYPT_FAILED            // The password does not open the stored private key.
    };

    // Takes the identity and the workspace keys of the session from the UserKeys message. A key
    // that cannot be unwrapped is skipped, leaving a hole that no refetch can repair - only a
    // re-grant can.
    Result apply(const proto::router::UserKeys& user_keys, const SecureString& password);

    void clear();

    qint64 userId() const { return user_id_; }
    const QString& userName() const { return user_name_; }
    bool hasPrivateKey() const { return !user_private_key_.isEmpty(); }

    bool hasWorkspaceKey(qint64 workspace_id) const;
    int workspaceKeyCount() const { return static_cast<int>(workspace_cryptors_.size()); }

    // Null when we hold no key of that workspace.
    const DataCryptor* workspaceCryptor(qint64 workspace_id) const;

    // Opens a group key sealed to our public key. Empty on failure.
    SecureByteArray unwrapGroupKey(const QByteArray& wrapped_gk) const;

    // A reply granted (or re-sealed) the key of a workspace.
    void storeWorkspaceKey(qint64 workspace_id, DataCryptor&& cryptor);

    // The complete workspace list is the authoritative answer about what we can access. A key kept
    // past that would let us keep decrypting the records and, worse, hand the key to a user we
    // grant access to - long after we lost it ourselves.
    void dropKeysExcept(const std::set<qint64>& visible_ids);

    // Re-seals every workspace key we hold to |new_public_key| and appends the results to the
    // message (any proto with a repeated WorkspaceKey workspace_key field).
    void resealGroupKeys(const QByteArray& new_public_key, proto::router::User* user) const;
    void resealGroupKeys(const QByteArray& new_public_key,
                         proto::router::ChangePasswordRequest* request) const;

private:
    qint64 user_id_ = 0;
    QString user_name_;
    SecureByteArray user_private_key_;
    std::unordered_map<qint64, DataCryptor> workspace_cryptors_;

    Q_DISABLE_COPY_MOVE(RouterKeys)
};

#endif // CLIENT_ROUTER_KEYS_H
