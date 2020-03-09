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

#include "router/database_sqlite.h"

namespace router {

DatabaseSqlite::DatabaseSqlite()
{

}

DatabaseSqlite::~DatabaseSqlite()
{

}

// static
std::unique_ptr<DatabaseSqlite> DatabaseSqlite::open()
{
    return nullptr;
}

// static
std::unique_ptr<DatabaseSqlite> DatabaseSqlite::create()
{
    return nullptr;
}

DatabaseSqlite::UserList DatabaseSqlite::userList() const
{
    return UserList();
}

bool DatabaseSqlite::addUser(const User& user)
{
    return false;
}

bool DatabaseSqlite::removeUser(std::u16string_view name)
{
    return false;
}

std::string DatabaseSqlite::id(std::string_view key) const
{
    return std::string();
}

} // namespace router
