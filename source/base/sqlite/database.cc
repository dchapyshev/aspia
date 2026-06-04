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

#include "base/sqlite/database.h"

#include <sqlite3.h>

#include "base/logging.h"

namespace sqlite {

//--------------------------------------------------------------------------------------------------
Database::~Database()
{
    close();
}

//--------------------------------------------------------------------------------------------------
bool Database::open(const QString& file_path)
{
    if (db_)
    {
        LOG(ERROR) << "Database is already open";
        return false;
    }

    const QByteArray utf8_path = file_path.toUtf8();

    // NOMUTEX selects multi-thread mode: the connection carries no internal mutex because it is
    // never touched by more than one thread at a time.
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX;

    const int result = sqlite3_open_v2(utf8_path.constData(), &db_, flags, nullptr);
    if (result != SQLITE_OK)
    {
        LOG(ERROR) << "Unable to open database:" << sqlite3_errstr(result) << "path:" << file_path;
        close();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void Database::close()
{
    if (!db_)
        return;

    // close_v2 tolerates outstanding prepared statements: the connection becomes a zombie that
    // is reclaimed once the last Statement finalizes. close() (without _v2) would fail instead.
    const int result = sqlite3_close_v2(db_);
    if (result != SQLITE_OK)
        LOG(ERROR) << "Unable to close database:" << sqlite3_errstr(result);

    db_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
bool Database::execute(const char* sql)
{
    if (!db_)
    {
        LOG(ERROR) << "Database is not open";
        return false;
    }

    char* error_message = nullptr;
    const int result = sqlite3_exec(db_, sql, nullptr, nullptr, &error_message);
    if (result != SQLITE_OK)
    {
        LOG(ERROR) << "Unable to execute statement:" << (error_message ? error_message : "")
                   << "SQL:" << sql;
        sqlite3_free(error_message);
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::beginTransaction()
{
    return execute("BEGIN");
}

//--------------------------------------------------------------------------------------------------
bool Database::commitTransaction()
{
    return execute("COMMIT");
}

//--------------------------------------------------------------------------------------------------
bool Database::rollbackTransaction()
{
    return execute("ROLLBACK");
}

//--------------------------------------------------------------------------------------------------
bool Database::setBusyTimeout(int ms)
{
    if (!db_)
    {
        LOG(ERROR) << "Database is not open";
        return false;
    }

    const int result = sqlite3_busy_timeout(db_, ms);
    if (result != SQLITE_OK)
    {
        LOG(ERROR) << "Unable to set busy timeout:" << sqlite3_errstr(result);
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
qint64 Database::lastInsertRowId() const
{
    if (!db_)
        return 0;
    return sqlite3_last_insert_rowid(db_);
}

//--------------------------------------------------------------------------------------------------
int Database::changes() const
{
    if (!db_)
        return 0;
    return sqlite3_changes(db_);
}

//--------------------------------------------------------------------------------------------------
QString Database::lastError() const
{
    if (!db_)
        return QString();
    return QString::fromUtf8(sqlite3_errmsg(db_));
}

//--------------------------------------------------------------------------------------------------
int Database::lastErrorCode() const
{
    if (!db_)
        return SQLITE_ERROR;
    return sqlite3_errcode(db_);
}

} // namespace sqlite
