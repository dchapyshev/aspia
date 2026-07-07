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

#include "base/sql/sql_query.h"

#include <sqlite3.h>

#include "base/logging.h"
#include "base/sql/sql_database.h"

//--------------------------------------------------------------------------------------------------
SqlQuery::SqlQuery(SqlDatabase& db, std::string_view sql)
    : db_(db)
{
    if (!db_.isOpen())
    {
        LOG(ERROR) << "Database is not open";
        return;
    }

    const int result = sqlite3_prepare_v2(
        db_.handle(), sql.data(), static_cast<int>(sql.size()), &stmt_, nullptr);
    if (result != SQLITE_OK)
    {
        LOG(ERROR) << "Unable to prepare statement:" << db_.lastError() << "SQL:" << sql;
        stmt_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
SqlQuery::~SqlQuery()
{
    if (stmt_)
        sqlite3_finalize(stmt_);
}

//--------------------------------------------------------------------------------------------------
bool SqlQuery::bindNull(int index)
{
    if (!stmt_)
        return false;

    if (sqlite3_bind_null(stmt_, index) != SQLITE_OK)
    {
        logError("bind null");
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool SqlQuery::bindInt64(int index, qint64 value)
{
    if (!stmt_)
        return false;

    if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK)
    {
        logError("bind int64");
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool SqlQuery::bindUInt64(int index, quint64 value)
{
    return bindInt64(index, static_cast<qint64>(value));
}

//--------------------------------------------------------------------------------------------------
bool SqlQuery::bindText(int index, std::string_view value)
{
    if (!stmt_)
        return false;

    // A null data pointer would bind SQL NULL; use a valid empty buffer so an empty view becomes
    // an empty string instead.
    const char* data = value.data() ? value.data() : "";

    if (sqlite3_bind_text(stmt_, index, data, static_cast<int>(value.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK)
    {
        logError("bind text");
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool SqlQuery::bindText(int index, const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return bindText(index, std::string_view(utf8.constData(), utf8.size()));
}

//--------------------------------------------------------------------------------------------------
bool SqlQuery::bindBlob(int index, const void* data, qsizetype size)
{
    if (!stmt_)
        return false;

    // A null data pointer would bind SQL NULL; use a valid empty buffer so a null input becomes
    // an empty blob, consistently with the other bindBlob overloads.
    if (!data)
        data = "";

    if (sqlite3_bind_blob(stmt_, index, data, static_cast<int>(size),
                          SQLITE_TRANSIENT) != SQLITE_OK)
    {
        logError("bind blob");
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool SqlQuery::bindBlob(int index, std::string_view value)
{
    return bindBlob(index, value.data(), static_cast<qsizetype>(value.size()));
}

//--------------------------------------------------------------------------------------------------
bool SqlQuery::bindBlob(int index, const QByteArray& value)
{
    // constData() is never null, so a zero-length QByteArray binds an empty blob rather than NULL.
    return bindBlob(index, value.constData(), value.size());
}

//--------------------------------------------------------------------------------------------------
SqlQuery& SqlQuery::addNull()
{
    bindNull(next_bind_index_++);
    return *this;
}

//--------------------------------------------------------------------------------------------------
SqlQuery& SqlQuery::addInt64(qint64 value)
{
    bindInt64(next_bind_index_++, value);
    return *this;
}

//--------------------------------------------------------------------------------------------------
SqlQuery& SqlQuery::addUInt64(quint64 value)
{
    bindUInt64(next_bind_index_++, value);
    return *this;
}

//--------------------------------------------------------------------------------------------------
SqlQuery& SqlQuery::addText(std::string_view value)
{
    bindText(next_bind_index_++, value);
    return *this;
}

//--------------------------------------------------------------------------------------------------
SqlQuery& SqlQuery::addText(const QString& value)
{
    bindText(next_bind_index_++, value);
    return *this;
}

//--------------------------------------------------------------------------------------------------
SqlQuery& SqlQuery::addBlob(std::string_view value)
{
    bindBlob(next_bind_index_++, value);
    return *this;
}

//--------------------------------------------------------------------------------------------------
SqlQuery& SqlQuery::addBlob(const QByteArray& value)
{
    bindBlob(next_bind_index_++, value);
    return *this;
}

//--------------------------------------------------------------------------------------------------
bool SqlQuery::next()
{
    if (!stmt_)
        return false;

    const int result = sqlite3_step(stmt_);
    if (result == SQLITE_ROW)
        return true;

    if (result != SQLITE_DONE)
        logError("step");
    return false;
}

//--------------------------------------------------------------------------------------------------
bool SqlQuery::exec()
{
    if (!stmt_)
        return false;

    const int result = sqlite3_step(stmt_);
    if (result == SQLITE_DONE || result == SQLITE_ROW)
        return true;

    logError("execute");
    return false;
}

//--------------------------------------------------------------------------------------------------
bool SqlQuery::reset()
{
    if (!stmt_)
        return false;

    next_bind_index_ = 1;

    // sqlite3_reset reports the error of the previous run, not of the reset itself; clearing
    // bindings is what we actually need before re-binding.
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
    return true;
}

//--------------------------------------------------------------------------------------------------
int SqlQuery::columnCount() const
{
    if (!stmt_)
        return 0;
    return sqlite3_column_count(stmt_);
}

//--------------------------------------------------------------------------------------------------
bool SqlQuery::columnIsNull(int column) const
{
    if (!stmt_)
        return true;
    return sqlite3_column_type(stmt_, column) == SQLITE_NULL;
}

//--------------------------------------------------------------------------------------------------
qint64 SqlQuery::columnInt64(int column) const
{
    if (!stmt_)
        return 0;
    return sqlite3_column_int64(stmt_, column);
}

//--------------------------------------------------------------------------------------------------
quint64 SqlQuery::columnUInt64(int column) const
{
    return static_cast<quint64>(columnInt64(column));
}

//--------------------------------------------------------------------------------------------------
double SqlQuery::columnDouble(int column) const
{
    if (!stmt_)
        return 0.0;
    return sqlite3_column_double(stmt_, column);
}

//--------------------------------------------------------------------------------------------------
std::string_view SqlQuery::columnTextView(int column) const
{
    if (!stmt_)
        return {};

    // Per the sqlite API the text pointer must be fetched before the byte count so the length
    // reflects any UTF-8 conversion sqlite performed.
    const unsigned char* text = sqlite3_column_text(stmt_, column);
    if (!text)
        return {};

    const int size = sqlite3_column_bytes(stmt_, column);
    return std::string_view(reinterpret_cast<const char*>(text), static_cast<size_t>(size));
}

//--------------------------------------------------------------------------------------------------
std::string_view SqlQuery::columnBlobView(int column) const
{
    if (!stmt_)
        return {};

    const void* blob = sqlite3_column_blob(stmt_, column);
    if (!blob)
        return {};

    const int size = sqlite3_column_bytes(stmt_, column);
    return std::string_view(static_cast<const char*>(blob), static_cast<size_t>(size));
}

//--------------------------------------------------------------------------------------------------
QString SqlQuery::columnText(int column) const
{
    const std::string_view view = columnTextView(column);
    return QString::fromUtf8(view.data(), static_cast<qsizetype>(view.size()));
}

//--------------------------------------------------------------------------------------------------
QByteArray SqlQuery::columnBlob(int column) const
{
    const std::string_view view = columnBlobView(column);
    return QByteArray(view.data(), static_cast<qsizetype>(view.size()));
}

//--------------------------------------------------------------------------------------------------
void SqlQuery::logError(const char* what) const
{
    LOG(ERROR) << "Unable to" << what << ":" << db_.lastError();
}
