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

#ifndef ROUTER_DATABASE_SQLITE_H
#define ROUTER_DATABASE_SQLITE_H

#include "base/macros_magic.h"
#include "router/database.h"

#include <filesystem>

#include <sqlite3.h>

namespace router {

class DatabaseSqlite final : public Database
{
public:
    ~DatabaseSqlite() final;

    static std::unique_ptr<DatabaseSqlite> create();
    static std::unique_ptr<DatabaseSqlite> open();
    static std::filesystem::path filePath();

    // Database implementation.
    std::vector<base::User> userList() const final;
    bool addUser(const base::User& user) final;
    bool modifyUser(const base::User& user) final;
    bool removeUser(int64_t entry_id) final;
    base::User findUser(std::u16string_view username) final;
    ErrorCode hostId(const base::ByteArray& key_hash, base::HostId* host_id) const final;
    bool addHost(const base::ByteArray& key_hash) final;

private:
    explicit DatabaseSqlite(sqlite3* db);
    static std::filesystem::path databaseDirectory();

    sqlite3* db_;

    DISALLOW_COPY_AND_ASSIGN(DatabaseSqlite);
};

} // namespace router

#endif // ROUTER_DATABASE_SQLITE_H
