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

#ifndef ROUTER__DATABASE_SQLITE_H
#define ROUTER__DATABASE_SQLITE_H

#include "base/macros_magic.h"
#include "router/database.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>

namespace router {

class DatabaseSqlite : public Database
{
public:
    ~DatabaseSqlite();

    static std::unique_ptr<DatabaseSqlite> open();
    static std::unique_ptr<DatabaseSqlite> create();
    static std::filesystem::path filePath();

    // Database implementation.
    UserList userList() const override;
    bool addUser(const User& user) override;
    bool removeUser(std::u16string_view name) override;
    std::string id(std::string_view key) const override;

private:
    explicit DatabaseSqlite(sqlite3* db);

    sqlite3* db_;

    DISALLOW_COPY_AND_ASSIGN(DatabaseSqlite);
};

} // namespace router

#endif // ROUTER__DATABASE_SQLITE_H
