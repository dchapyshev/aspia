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

#ifndef BASE_PEER_USER_LIST_H
#define BASE_PEER_USER_LIST_H

#include "base/peer/user_list_base.h"

namespace base {

class UserList final : public UserListBase
{
public:
    ~UserList() final;

    static std::unique_ptr<UserList> createEmpty();
    std::unique_ptr<UserList> duplicate() const;

    // UserListBase implementation.
    void add(const User& user) final;
    User find(const QString& username) const final;
    const QByteArray& seedKey() const final { return seed_key_; }
    void setSeedKey(const QByteArray& seed_key) final;
    QVector<User> list() const final { return list_; }

private:
    UserList();
    UserList(const QVector<User>& list, const QByteArray& seed_key);

    QByteArray seed_key_;
    QVector<User> list_;

    Q_DISABLE_COPY(UserList)
};

} // namespace base

#endif // BASE_PEER_USER_LIST_H
