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

#ifndef BASE_PEER_ROUTER_USER_H
#define BASE_PEER_ROUTER_USER_H

#include "base/peer/user.h"

namespace proto::router {
class User;
} // namespace proto::router

class RouterUser final : public User
{
public:
    RouterUser() = default;
    ~RouterUser() = default;

    RouterUser(const RouterUser& other) = default;
    RouterUser& operator=(const RouterUser& other) = default;

    RouterUser(RouterUser&& other) noexcept = default;
    RouterUser& operator=(RouterUser&& other) noexcept = default;

    static RouterUser create(const QString& name, const SecureString& password);
    bool isValid() const;

    static RouterUser parseFrom(const proto::router::User& serialized_user);
    proto::router::User serialize() const;

    QByteArray public_key;
    QByteArray wrap_private_key;
    QByteArray wrap_salt;
    QByteArray otp_secret;
    quint64 otp_counter = 0;
};

#endif // BASE_PEER_ROUTER_USER_H
