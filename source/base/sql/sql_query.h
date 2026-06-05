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

#ifndef BASE_SQL_SQL_QUERY_H
#define BASE_SQL_SQL_QUERY_H

#include <QByteArray>
#include <QString>

#include <string_view>

struct sqlite3_stmt;

class SqlDatabase;

// RAII wrapper over a prepared sqlite3_stmt. Bind parameters are 1-based (native sqlite); result
// columns are 0-based (matching the value(i) convention the project moved off of). Bound values
// are copied into sqlite, so the caller's buffers need not outlive the bind call.
//
// The column*View() accessors return non-owning views straight into sqlite's own row buffer -
// no transcoding, no allocation. Such a view is valid only until the next next()/reset() call or
// until the SqlQuery is destroyed; copy it (or use the copying accessors) if it must outlive the
// row.
class SqlQuery final
{
public:
    // Prepares |sql| against |db|. Check isValid() before use; a prepare failure is logged.
    SqlQuery(SqlDatabase& db, std::string_view sql);
    ~SqlQuery();

    bool isValid() const { return stmt_ != nullptr; }

    //----------------------------------------------------------------------------------------------
    // Binding by explicit 1-based parameter index.
    //----------------------------------------------------------------------------------------------

    bool bindNull(int index);
    bool bindInt64(int index, qint64 value);
    bool bindUInt64(int index, quint64 value);
    bool bindText(int index, std::string_view value);
    bool bindText(int index, const QString& value);
    bool bindBlob(int index, const void* data, qsizetype size);
    bool bindBlob(int index, std::string_view value);
    bool bindBlob(int index, const QByteArray& value);

    //----------------------------------------------------------------------------------------------
    // Sequential binding: each call targets the next parameter,
    // starting at 1. reset() rewinds the cursor back to the first parameter.
    //----------------------------------------------------------------------------------------------

    SqlQuery& addNull();
    SqlQuery& addInt64(qint64 value);
    SqlQuery& addUInt64(quint64 value);
    SqlQuery& addText(std::string_view value);
    SqlQuery& addText(const QString& value);
    SqlQuery& addBlob(std::string_view value);
    SqlQuery& addBlob(const QByteArray& value);

    //----------------------------------------------------------------------------------------------
    // Execution.
    //----------------------------------------------------------------------------------------------

    // Advances to the next row. Returns true when a row is available, false on completion or
    // error (errors are logged). Use in SELECT loops: while (stmt.next()) { ... }.
    bool next();

    // Runs a statement that yields no rows (INSERT/UPDATE/DELETE/DDL) to completion. Returns
    // true on success.
    bool exec();

    // Resets execution state and clears all bindings so the statement can be re-bound and
    // re-run. Returns false on error.
    bool reset();

    //----------------------------------------------------------------------------------------------
    // Column access by 0-based index, valid after next() returned true.
    //----------------------------------------------------------------------------------------------

    int columnCount() const;
    bool columnIsNull(int column) const;
    qint64 columnInt64(int column) const;
    quint64 columnUInt64(int column) const;
    double columnDouble(int column) const;

    // Zero-copy UTF-8 / blob views into sqlite's row buffer. Valid until the next next()/reset()
    // or destruction. Empty for NULL values.
    std::string_view columnTextView(int column) const;
    std::string_view columnBlobView(int column) const;

    // Copying accessors for values that must outlive the current row.
    QString columnText(int column) const;
    QByteArray columnBlob(int column) const;

private:
    void logError(const char* what) const;

    SqlDatabase& db_;
    sqlite3_stmt* stmt_ = nullptr;
    int next_bind_index_ = 1;

    Q_DISABLE_COPY_MOVE(SqlQuery)
};

#endif // BASE_SQL_SQL_QUERY_H
