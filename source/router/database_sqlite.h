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

#include <filesystem>

#include <sqlite3.h>

namespace router {

class DatabaseSqlite : public Database
{
public:
    ~DatabaseSqlite();

    static std::unique_ptr<DatabaseSqlite> open();
    static std::filesystem::path filePath();

    // Database implementation.
    std::vector<base::User> userList() const override;
    bool addUser(const base::User& user) override;
    bool modifyUser(const base::User& user) override;
    bool removeUser(int64_t entry_id) override;
    base::User findUser(std::u16string_view username) override;
    base::HostId hostId(const base::ByteArray& keyHash) const override;
    bool addHost(const base::ByteArray& keyHash) override;

private:
    explicit DatabaseSqlite(sqlite3* db);

    sqlite3* db_;

    DISALLOW_COPY_AND_ASSIGN(DatabaseSqlite);
};

} // namespace router

#endif // ROUTER__DATABASE_SQLITE_H
