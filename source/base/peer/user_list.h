//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/macros_magic.h"
#include "base/peer/user_list_base.h"

namespace base {

class UserList : public UserListBase
{
public:
    ~UserList() override;

    static std::unique_ptr<UserList> createEmpty();
    std::unique_ptr<UserList> duplicate() const;
    void merge(const UserList& user_list);

    // UserListBase implementation.
    void add(const User& user) override;
    User find(std::u16string_view username) const override;
    const ByteArray& seedKey() const override { return seed_key_; }
    void setSeedKey(const ByteArray& seed_key) override;
    std::vector<User> list() const override { return list_; }

private:
    UserList();
    UserList(const std::vector<User>& list, const ByteArray& seed_key);

    ByteArray seed_key_;
    std::vector<User> list_;

    DISALLOW_COPY_AND_ASSIGN(UserList);
};

} // namespace base

#endif // BASE_PEER_USER_LIST_H
