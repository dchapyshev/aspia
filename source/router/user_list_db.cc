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

#include "router/user_list_db.h"

#include "base/logging.h"
#include "router/database.h"
#include "router/database_factory.h"

namespace router {

//--------------------------------------------------------------------------------------------------
UserListDb::UserListDb(std::unique_ptr<Database> db)
    : db_(std::move(db))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
UserListDb::~UserListDb() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<UserListDb> UserListDb::open(const DatabaseFactory& factory)
{
    std::unique_ptr<Database> db = factory.openDatabase();
    if (!db)
    {
        LOG(LS_ERROR) << "Unable to open database";
        return nullptr;
    }

    return std::unique_ptr<UserListDb>(new UserListDb(std::move(db)));
}

//--------------------------------------------------------------------------------------------------
void UserListDb::add(const base::User& user)
{
    db_->addUser(user);
}

//--------------------------------------------------------------------------------------------------
base::User UserListDb::find(const QString& username) const
{
    return db_->findUser(username);
}

//--------------------------------------------------------------------------------------------------
const QByteArray& UserListDb::seedKey() const
{
    return seed_key_;
}

//--------------------------------------------------------------------------------------------------
void UserListDb::setSeedKey(const QByteArray& seed_key)
{
    seed_key_ = seed_key;
}

//--------------------------------------------------------------------------------------------------
QVector<base::User> UserListDb::list() const
{
    return db_->userList();
}

} // namespace router
