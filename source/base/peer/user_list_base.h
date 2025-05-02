//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_PEER_USER_LIST_BASE_H
#define BASE_PEER_USER_LIST_BASE_H

#include "base/peer/user.h"

#include <QVector>

namespace base {

class UserListBase
{
public:
    virtual ~UserListBase() = default;

    virtual void add(const User& user) = 0;
    virtual User find(const QString& username) const = 0;
    virtual const QByteArray& seedKey() const = 0;
    virtual void setSeedKey(const QByteArray& seed_key) = 0;
    virtual QVector<User> list() const = 0;
};

} // namespace base

#endif // BASE_PEER_USER_LIST_BASE_H
