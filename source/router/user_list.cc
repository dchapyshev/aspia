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

#include "router/user_list.h"

#include "base/logging.h"

namespace router {

//--------------------------------------------------------------------------------------------------
UserList::UserList(Database&& db)
    : db_(std::move(db))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
UserList::~UserList() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<UserList> UserList::open()
{
    Database db = Database::open();
    if (!db.isValid())
    {
        LOG(ERROR) << "Unable to open database";
        return nullptr;
    }

    return std::unique_ptr<UserList>(new UserList(std::move(db)));
}

//--------------------------------------------------------------------------------------------------
void UserList::add(const base::User& user)
{
    db_.addUser(user);
}

//--------------------------------------------------------------------------------------------------
base::User UserList::find(const QString& username) const
{
    return db_.findUser(username);
}

//--------------------------------------------------------------------------------------------------
const QByteArray& UserList::seedKey() const
{
    return seed_key_;
}

//--------------------------------------------------------------------------------------------------
void UserList::setSeedKey(const QByteArray& seed_key)
{
    seed_key_ = seed_key;
}

//--------------------------------------------------------------------------------------------------
QVector<base::User> UserList::list() const
{
    return db_.userList();
}

} // namespace router
