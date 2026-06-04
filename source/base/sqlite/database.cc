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

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#else
#include <unicode/uchar.h>
#include <unicode/ustring.h>
#endif // defined(Q_OS_WINDOWS)

#include "base/logging.h"

namespace sqlite {

namespace {

//--------------------------------------------------------------------------------------------------
void caseFold(sqlite3_context* context, int argc, sqlite3_value** argv)
{
    if (argc != 1)
    {
        sqlite3_result_null(context);
        return;
    }

    const auto* source = static_cast<const char16_t*>(sqlite3_value_text16(argv[0]));
    if (!source)
    {
        // NULL or non-text argument folds to NULL, matching sqlite's own string functions.
        sqlite3_result_null(context);
        return;
    }

    const int source_length = sqlite3_value_bytes16(argv[0]) / static_cast<int>(sizeof(char16_t));
    if (source_length <= 0)
    {
        sqlite3_result_text16(context, u"", 0, SQLITE_TRANSIENT);
        return;
    }

    std::u16string folded;
    int folded_length = 0;

#if defined(Q_OS_WINDOWS)
    // LCMAP_LOWERCASE applies the Unicode lowercase mapping; the invariant locale keeps the result
    // independent of the machine's locale.
    const auto* win_source = reinterpret_cast<const wchar_t*>(source);
    folded_length = LCMapStringEx(
        LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, win_source, source_length, nullptr, 0, nullptr,
        nullptr, 0);
    if (folded_length <= 0)
    {
        // Folding failed - fall back to the original value rather than dropping the row.
        sqlite3_result_value(context, argv[0]);
        return;
    }

    folded.resize(static_cast<size_t>(folded_length));
    LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, win_source, source_length,
                  reinterpret_cast<wchar_t*>(folded.data()), folded_length, nullptr, nullptr, 0);
#else
    // Preflight to learn the folded length (sets U_BUFFER_OVERFLOW_ERROR), then fold for real.
    UErrorCode status = U_ZERO_ERROR;
    folded_length = u_strFoldCase(nullptr, 0, reinterpret_cast<const UChar*>(source), source_length,
                                  U_FOLD_CASE_DEFAULT, &status);
    if (folded_length <= 0)
    {
        // Folding failed - fall back to the original value rather than dropping the row.
        sqlite3_result_value(context, argv[0]);
        return;
    }

    status = U_ZERO_ERROR;
    folded.resize(static_cast<size_t>(folded_length));
    u_strFoldCase(reinterpret_cast<UChar*>(folded.data()), folded_length,
                  reinterpret_cast<const UChar*>(source), source_length, U_FOLD_CASE_DEFAULT,
                  &status);
    if (U_FAILURE(status))
    {
        sqlite3_result_value(context, argv[0]);
        return;
    }
#endif

    sqlite3_result_text16(context, folded.data(),
                          folded_length * static_cast<int>(sizeof(char16_t)), SQLITE_TRANSIENT);
}

} // namespace

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

    // Register a Unicode-aware casefold() so case-insensitive search works for non-ASCII text.
    // The stock sqlite LIKE folds ASCII only; casefold() folds via the platform Unicode API and
    // the search query folds both operands: casefold(col) LIKE casefold(?). See caseFold above.
    createScalarFunction("casefold", 1, &caseFold);

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
bool Database::createScalarFunction(const char* name, int arg_count, ScalarFunc func)
{
    if (!db_)
    {
        LOG(ERROR) << "Database is not open";
        return false;
    }

    // DETERMINISTIC lets sqlite reuse results for identical inputs and use the function in more
    // contexts (indexed expressions, the WHERE clause optimizer).
    const int result = sqlite3_create_function_v2(
        db_, name, arg_count, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr, func, nullptr, nullptr,
        nullptr);
    if (result != SQLITE_OK)
    {
        LOG(ERROR) << "Unable to create function" << name << ":" << sqlite3_errstr(result);
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
