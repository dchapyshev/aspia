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

#ifndef BASE_SQL_SQL_DATABASE_H
#define BASE_SQL_SQL_DATABASE_H

#include <QString>

#include "base/time_types.h"

struct sqlite3;
struct sqlite3_context;
struct sqlite3_value;

// Thin RAII wrapper over a single sqlite3 connection. An SqlDatabase must be used from a single
// thread at a time, which matches the per-thread connection model used across the project. Errors
// are reported through the return value and logged; no exceptions are thrown.
class SqlDatabase final
{
public:
    SqlDatabase() = default;
    ~SqlDatabase();

    // Opens (creating it if absent) the database at |file_path|. |busy_timeout| sets how long
    // a call blocked on another connection's lock waits before failing with SQLITE_BUSY; zero
    // disables waiting so collisions fail instantly. Returns false and leaves the object closed
    // on failure.
    bool open(const QString& file_path, MilliSeconds busy_timeout = MilliSeconds(5000));

    bool isOpen() const { return db_ != nullptr; }
    void close();

    // Runs one or more statements that return no rows (pragmas without a result, DDL). For
    // statements that need binding or produce rows use an SqlQuery.
    bool exec(const char* sql);

    enum class TransactionMode
    {
        DEFERRED = 0,
        IMMEDIATE = 1
    };

    bool beginTransaction(TransactionMode mode = TransactionMode::DEFERRED);
    bool commitTransaction();
    bool rollbackTransaction();

    // Signature of a custom scalar SQL function: reads its arguments from |argv| and reports the
    // result through |context| using the sqlite3_result_* family.
    using ScalarFunc = void (*)(sqlite3_context* context, int argc, sqlite3_value** argv);

    // Registers a deterministic UTF-8 scalar function callable from SQL under |name|. |arg_count|
    // is the number of arguments (-1 for variadic).
    bool createScalarFunction(const char* name, int arg_count, ScalarFunc func);

    // rowid of the most recent successful INSERT on this connection.
    qint64 lastInsertRowId() const;

    // Number of rows changed by the most recent INSERT/UPDATE/DELETE on this connection.
    int changes() const;

    // Human-readable message and code for the most recent failed call on this connection.
    QString lastError() const;
    int lastErrorCode() const;

    sqlite3* handle() const { return db_; }

private:
    sqlite3* db_ = nullptr;

    Q_DISABLE_COPY_MOVE(SqlDatabase)
};

#endif // BASE_SQL_SQL_DATABASE_H
