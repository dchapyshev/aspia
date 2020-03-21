//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__USER_H
#define HOST__USER_H

#include "base/memory/byte_array.h"

namespace host {

class User
{
public:
    User() = default;
    ~User() = default;

    User(const User& other) = default;
    User& operator=(const User& other) = default;

    User(User&& other) noexcept = default;
    User& operator=(User&& other) noexcept = default;

    enum Flags { ENABLED = 1 };

    static User create(std::u16string_view name, std::u16string_view password);
    bool isValid() const;

    std::u16string name;
    base::ByteArray salt;
    base::ByteArray verifier;
    base::ByteArray number;
    base::ByteArray generator;
    uint32_t sessions = 0;
    uint32_t flags = 0;
};

class UserList
{
public:
    UserList() = default;

    UserList(const UserList& other) = default;
    UserList& operator=(const UserList& other) = default;

    UserList(UserList&& other) noexcept = default;
    UserList& operator=(UserList&& other) noexcept = default;

    void add(const User& user);
    void add(User&& user);
    void merge(const UserList& user_list);
    void merge(UserList&& user_list);

    const User& find(std::u16string_view username) const;
    size_t count() const { return list_.size(); }
    bool empty() const { return list_.empty(); }

    const base::ByteArray& seedKey() const { return seed_key_; }
    void setSeedKey(const base::ByteArray& seed_key);
    void setSeedKey(base::ByteArray&& seed_key);

    class Iterator
    {
    public:
        Iterator(const UserList& list);
        ~Iterator();

        const User& user() const;
        bool isAtEnd() const;
        void advance();

    private:
        const std::vector<User>& list_;
        std::vector<User>::const_iterator pos_;
    };

private:
    base::ByteArray seed_key_;
    std::vector<User> list_;
};

} // namespace host

#endif // HOST__USER_H
