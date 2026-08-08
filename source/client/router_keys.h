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

#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_string.h"

namespace proto::router {
class UserKeys;
} // namespace proto::router

// The identity of a router session and the key material it holds: who we are and the private key
// the password opens.
class RouterKeys
{
public:
    RouterKeys() = default;
    ~RouterKeys() = default;

    enum class Result
    {
        OK,                       // The identity is loaded.
        PASSWORD_CHANGE_REQUIRED, // The record has no wrapped private key yet.
        DECRYPT_FAILED            // The password does not open the stored private key.
    };

    // Takes the identity of the session from the UserKeys message.
    Result apply(const proto::router::UserKeys& user_keys, const SecureString& password);

    void clear();

    qint64 userId() const { return user_id_; }
    const QString& userName() const { return user_name_; }
    bool hasPrivateKey() const { return !user_private_key_.isEmpty(); }

private:
    qint64 user_id_ = 0;
    QString user_name_;
    SecureByteArray user_private_key_;

    Q_DISABLE_COPY_MOVE(RouterKeys)
};

#endif // CLIENT_ROUTER_KEYS_H
