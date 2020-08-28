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

#include "base/peer/user_list.h"

#include "base/strings/string_util.h"

namespace base {

void UserList::add(const User& user)
{
    if (user.isValid())
        list_.emplace_back(user);
}

void UserList::add(User&& user)
{
    if (user.isValid())
        list_.emplace_back(std::move(user));
}

void UserList::merge(const UserList& user_list)
{
    for (const auto& user : user_list.list_)
        add(user);
}

void UserList::merge(UserList&& user_list)
{
    for (auto& user : user_list.list_)
        add(std::move(user));
}

const User& UserList::find(std::u16string_view username) const
{
    const User* user = &User::kInvalidUser;

    for (const auto& item : list_)
    {
        if (compareCaseInsensitive(username, item.name) == 0)
            user = &item;
    }

    return *user;
}

void UserList::setSeedKey(const ByteArray& seed_key)
{
    seed_key_ = seed_key;
}

void UserList::setSeedKey(ByteArray&& seed_key)
{
    seed_key_ = std::move(seed_key);
}

UserList::Iterator::Iterator(const UserList& list)
    : list_(list.list_),
    pos_(list.list_.cbegin())
{
    // Nothing
}

UserList::Iterator::~Iterator() = default;

const User& UserList::Iterator::user() const
{
    if (isAtEnd())
        return User::kInvalidUser;

    return *pos_;
}

bool UserList::Iterator::isAtEnd() const
{
    return pos_ == list_.cend();
}

void UserList::Iterator::advance()
{
    ++pos_;
}

} // namespace base
