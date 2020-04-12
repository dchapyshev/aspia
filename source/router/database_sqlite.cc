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
#include "base/strings/unicode.h"
#include "build/build_config.h"

#include <optional>

namespace router {

namespace {

template <typename T>
std::optional<T> readInteger(sqlite3_stmt* statement, int column)
{
    int column_type = sqlite3_column_type(statement, column);
    if (column_type != SQLITE_INTEGER)
    {
        LOG(LS_ERROR) << "Type is not SQLITE_INTEGER";
        return std::nullopt;
    }

    return static_cast<T>(sqlite3_column_int64(statement, column));
}

std::optional<base::ByteArray> readBlob(sqlite3_stmt* statement, int column)
{
    int column_type = sqlite3_column_type(statement, column);
    if (column_type != SQLITE_BLOB)
    {
        LOG(LS_ERROR) << "Type is not SQLITE_BLOB";
        return std::nullopt;
    }

    int blob_size = sqlite3_column_bytes(statement, column);
    if (blob_size <= 0)
    {
        LOG(LS_ERROR) << "Field has an invalid size";
        return std::nullopt;
    }

    const void* blob = sqlite3_column_blob(statement, column);
    if (!blob)
    {
        LOG(LS_ERROR) << "Failed to get the pointer to the field";
        return std::nullopt;
    }

    return base::fromData(blob, static_cast<size_t>(blob_size));
}

std::optional<std::string> readText(sqlite3_stmt* statement, int column)
{
    int column_type = sqlite3_column_type(statement, column);
    if (column_type != SQLITE_TEXT)
    {
        LOG(LS_ERROR) << "Type is not SQLITE_TEXT";
        return std::nullopt;
    }

    int string_size = sqlite3_column_bytes(statement, column);
    if (string_size <= 0)
    {
        LOG(LS_ERROR) << "Field has an invalid size";
        return std::nullopt;
    }

    const uint8_t* string = sqlite3_column_text(statement, column);
    if (!string)
    {
        LOG(LS_ERROR) << "Failed to get the pointer to the field";
        return std::nullopt;
    }

    return std::string(reinterpret_cast<const char*>(string), string_size);
}

std::optional<std::u16string> readText16(sqlite3_stmt* statement, int column)
{
    std::optional<std::string> str = readText(statement, column);
    if (!str.has_value())
        return std::nullopt;

    return base::utf16FromUtf8(str.value());
}

std::optional<net::ServerUser> readUser(sqlite3_stmt* statement)
{
    std::optional<uint64_t> entry_id = readInteger<uint64_t>(statement, 0);
    if (!entry_id.has_value())
    {
        LOG(LS_ERROR) << "Failed to get field 'id'";
        return std::nullopt;
    }

    std::optional<std::u16string> name = readText16(statement, 1);
    if (!name.has_value())
    {
        LOG(LS_ERROR) << "Failed to get field 'name'";
        return std::nullopt;
    }

    std::optional<std::string> group = readText(statement, 2);
    if (!group.has_value())
    {
        LOG(LS_ERROR) << "Failed to get field 'group'";
        return std::nullopt;
    }

    std::optional<base::ByteArray> salt = readBlob(statement, 3);
    if (!salt.has_value())
    {
        LOG(LS_ERROR) << "Failed to get field 'salt'";
        return std::nullopt;
    }

    std::optional<base::ByteArray> verifier = readBlob(statement, 4);
    if (!verifier.has_value())
    {
        LOG(LS_ERROR) << "Failed to get field 'verifier'";
        return std::nullopt;
    }

    std::optional<uint32_t> sessions = readInteger<uint32_t>(statement, 5);
    if (!sessions.has_value())
    {
        LOG(LS_ERROR) << "Failed to get field 'sessions'";
        return std::nullopt;
    }

    std::optional<uint32_t> flags = readInteger<uint32_t>(statement, 6);
    if (!flags.has_value())
    {
        LOG(LS_ERROR) << "Failed to get field 'flags'";
        return std::nullopt;
    }

    net::ServerUser user;

    user.entry_id  = entry_id.value();
    user.name      = std::move(name.value());
    user.group     = std::move(group.value());
    user.salt      = std::move(salt.value());
    user.verifier  = std::move(verifier.value());
    user.sessions  = sessions.value();
    user.flags     = flags.value();

    return user;
}

} // namespace

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

    sqlite3* db = nullptr;

    int error_code = sqlite3_open(file_path.u8string().c_str(), &db);
    if (error_code != SQLITE_OK)
    {
        LOG(LS_WARNING) << "sqlite3_open failed: " << sqlite3_errstr(error_code);
        return nullptr;
    }

    return std::unique_ptr<DatabaseSqlite>(new DatabaseSqlite(db));
}

// static
std::filesystem::path DatabaseSqlite::filePath()
{
    std::filesystem::path file_path;

#if defined(OS_WIN)
    if (!base::BasePaths::currentExecDir(&file_path))
        return std::filesystem::path();

    file_path.append(u"aspia_router.db3");
#else // defined(OS_*)
#error Not implemented
#endif // defined(OS_*)

    return file_path;
}

net::ServerUserList DatabaseSqlite::userList() const
{
    const char kQuery[] = "SELECT * FROM users";

    sqlite3_stmt* statement;
    int error_code = sqlite3_prepare(db_, kQuery, std::size(kQuery), &statement, nullptr);
    if (error_code != SQLITE_OK)
    {
        LOG(LS_ERROR) << "sqlite3_prepare failed: " << sqlite3_errstr(error_code);
        return net::ServerUserList();
    }

    net::ServerUserList users;
    for (;;)
    {
        if (sqlite3_step(statement) != SQLITE_ROW)
            break;

        std::optional<net::ServerUser> user = readUser(statement);
        if (user.has_value())
            users.add(std::move(user.value()));
    }

    sqlite3_finalize(statement);
    return users;
}

bool DatabaseSqlite::addUser(const net::ServerUser& user)
{
    NOTIMPLEMENTED();
    return false;
}

bool DatabaseSqlite::removeUser(uint64_t entry_id)
{
    NOTIMPLEMENTED();
    return false;
}

uint64_t DatabaseSqlite::peerId(std::string_view key) const
{
    NOTIMPLEMENTED();
    return 0;
}

} // namespace router
