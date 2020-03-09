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

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "build/build_config.h"

namespace router {

DatabaseSqlite::DatabaseSqlite(sqlite3* db)
    : db_(db)
{
    DCHECK(db_);
}

DatabaseSqlite::~DatabaseSqlite()
{
    sqlite3_close(db_);
}

// static
std::unique_ptr<DatabaseSqlite> DatabaseSqlite::open()
{
    std::filesystem::path file_path = filePath();
    if (file_path.empty())
    {
        LOG(LS_WARNING) << "Invalid file path";
        return nullptr;
    }

    sqlite3* db;

    int error_code = sqlite3_open(file_path.u8string().c_str(), &db);
    if (error_code != SQLITE_OK)
    {
        LOG(LS_WARNING) << "sqlite3_open failed: " << sqlite3_errstr(error_code);
        return nullptr;
    }

    return std::unique_ptr<DatabaseSqlite>(new DatabaseSqlite(db));
}

// static
std::unique_ptr<DatabaseSqlite> DatabaseSqlite::create()
{
    return nullptr;
}

// static
std::filesystem::path DatabaseSqlite::filePath()
{
    std::filesystem::path file_path;

#if defined(OS_WIN)
    if (!base::BasePaths::commonAppData(&file_path))
        return std::filesystem::path();

    file_path.append(u"aspia/router/router.db");
#else // defined(OS_*)
#error Not implemented
#endif // defined(OS_*)

    return file_path;
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
