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

#include "base/peer/user_list.h"

namespace base {

//--------------------------------------------------------------------------------------------------
UserList::UserList() = default;

//--------------------------------------------------------------------------------------------------
UserList::UserList(const QVector<User>& list, const QByteArray& seed_key)
    : seed_key_(seed_key),
      list_(list)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
UserList::~UserList() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<UserList> UserList::createEmpty()
{
    return std::unique_ptr<UserList>(new UserList());
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<UserList> UserList::duplicate() const
{
    return std::unique_ptr<UserList>(new UserList(list_, seed_key_));
}

//--------------------------------------------------------------------------------------------------
void UserList::add(const User& user)
{
    if (user.isValid())
        list_.emplace_back(user);
}

//--------------------------------------------------------------------------------------------------
User UserList::find(const QString& username) const
{
    const User* user = &User::kInvalidUser;

    for (const auto& item : list_)
    {
        if (username.compare(item.name, Qt::CaseInsensitive) == 0)
            user = &item;
    }

    return *user;
}

//--------------------------------------------------------------------------------------------------
void UserList::setSeedKey(const QByteArray& seed_key)
{
    seed_key_ = seed_key;
}

} // namespace base
