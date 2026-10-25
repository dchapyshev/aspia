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

#include "router/access_log.h"

#include <QDir>
#include <QFileInfo>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/files/base_paths.h"
#include "base/sql/sql_query.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"

namespace {

//--------------------------------------------------------------------------------------------------
bool isPageValid(qint64 offset, qint64 count)
{
    return offset >= 0 && count > 0 && count <= proto::router::kMaxAccessLogPageSize;
}

//--------------------------------------------------------------------------------------------------
bool isFilterValid(const proto::router::AccessLogFilter& filter)
{
    if (filter.time_from() < 0 || filter.time_to() < 0)
        return false;

    const std::string_view texts[] = { filter.user_name(), filter.client(), filter.host() };

    for (std::string_view text : texts)
    {
        if (text.size() > proto::router::kMaxAccessLogQueryLength)
            return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
std::string substringPattern(std::string_view text)
{
    std::string pattern;
    pattern.reserve(text.size() + 2);
    pattern += '%';

    for (char symbol : text)
    {
        if (symbol == '\\' || symbol == '%' || symbol == '_')
            pattern += '\\';
        pattern += symbol;
    }

    pattern += '%';
    return pattern;
}

//--------------------------------------------------------------------------------------------------
std::string filterClause(const proto::router::AccessLogFilter& filter)
{
    std::vector<std::string> conditions;
    conditions.emplace_back("id <= ?");

    if (filter.time_from())
        conditions.emplace_back("time >= ?");
    if (filter.time_to())
        conditions.emplace_back("time < ?");
    if (!filter.user_name().empty())
        conditions.emplace_back("casefold(user_name) LIKE casefold(?) ESCAPE '\\'");
    if (!filter.client().empty())
    {
        conditions.emplace_back("(client_address LIKE ? ESCAPE '\\' "
                                "OR casefold(client_computer_name) LIKE casefold(?) ESCAPE '\\')");
    }
    if (!filter.host().empty())
    {
        conditions.emplace_back("(casefold(host_computer_name) LIKE casefold(?) ESCAPE '\\' "
                                "OR host_address LIKE ? ESCAPE '\\' "
                                "OR CAST(host_id AS TEXT) LIKE ? ESCAPE '\\')");
    }
    if (filter.session_type() != proto::peer::SESSION_TYPE_UNKNOWN)
        conditions.emplace_back("session_type = ?");

    return " WHERE " + strJoin(conditions, " AND ");
}

//--------------------------------------------------------------------------------------------------
void bindFilter(const proto::router::AccessLogFilter& filter, qint64 max_entry_id, SqlQuery* query)
{
    CHECK(query);

    query->addInt64(max_entry_id);

    if (filter.time_from())
        query->addInt64(filter.time_from());
    if (filter.time_to())
        query->addInt64(filter.time_to());
    if (!filter.user_name().empty())
        query->addText(substringPattern(filter.user_name()));
    if (!filter.client().empty())
    {
        const std::string pattern = substringPattern(filter.client());
        query->addText(pattern);
        query->addText(pattern);
    }
    if (!filter.host().empty())
    {
        const std::string pattern = substringPattern(filter.host());
        query->addText(pattern);
        query->addText(pattern);
        query->addText(pattern);
    }
    if (filter.session_type() != proto::peer::SESSION_TYPE_UNKNOWN)
        query->addInt64(filter.session_type());
}

//--------------------------------------------------------------------------------------------------
bool ensureSchema(SqlDatabase& db)
{
    // AUTOINCREMENT keeps the ids growing after a prune, so a pinned page does not take in new entries.
    if (!db.exec("CREATE TABLE IF NOT EXISTS \"access_log\" ("
                 "\"id\" INTEGER PRIMARY KEY AUTOINCREMENT,"
                 "\"time\" INTEGER NOT NULL,"
                 "\"user_id\" INTEGER NOT NULL,"
                 "\"user_name\" TEXT NOT NULL,"
                 "\"session_type\" INTEGER NOT NULL DEFAULT 0,"
                 "\"client_address\" TEXT NOT NULL DEFAULT '',"
                 "\"client_computer_name\" TEXT NOT NULL DEFAULT '',"
                 "\"client_os_name\" TEXT NOT NULL DEFAULT '',"
                 "\"client_version\" TEXT NOT NULL DEFAULT '',"
                 "\"host_id\" INTEGER NOT NULL,"
                 "\"host_computer_name\" TEXT NOT NULL DEFAULT '',"
                 "\"host_address\" TEXT NOT NULL DEFAULT '')"))
    {
        LOG(ERROR) << "Unable to create the access log table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE INDEX IF NOT EXISTS \"access_log_time\" ON \"access_log\"(\"time\")"))
    {
        LOG(ERROR) << "Unable to create the access log index:" << db.lastError();
        return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QString AccessLog::filePath()
{
    return BasePaths::appDataDir() + "/access_log.db3";
}

//--------------------------------------------------------------------------------------------------
bool AccessLog::open(const QString& file_path)
{
    if (file_path.isEmpty())
    {
        LOG(ERROR) << "Invalid file path";
        return false;
    }

    if (!QDir().mkpath(QFileInfo(file_path).absolutePath()))
    {
        LOG(ERROR) << "Unable to create directory for access log";
        return false;
    }

    LOG(INFO) << (!QFileInfo::exists(file_path) ? "Creating" : "Opening") << "access log:" << file_path;

    if (!db_.open(file_path))
        return false;

    if (!db_.exec("PRAGMA journal_mode = WAL"))
        LOG(WARNING) << "Unable to enable WAL journal mode:" << db_.lastError();

    if (!ensureSchema(db_))
    {
        db_.close();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void AccessLog::close()
{
    db_.close();
}

//--------------------------------------------------------------------------------------------------
bool AccessLog::isValid() const
{
    return db_.isOpen();
}

//--------------------------------------------------------------------------------------------------
bool AccessLog::addEntry(const Record& record)
{
    if (!isValid())
    {
        LOG(ERROR) << "Access log is not valid";
        return false;
    }

    SqlQuery query(db_,
        "INSERT INTO access_log (time, user_id, user_name, session_type, client_address, "
        "client_computer_name, client_os_name, client_version, host_id, host_computer_name, host_address) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addInt64(secondsSinceEpoch());
    query.addInt64(record.user_id);
    query.addText(record.user_name);
    query.addInt64(record.session_type);
    query.addText(record.client_address);
    query.addText(record.client_computer_name);
    query.addText(record.client_os_name);
    query.addText(record.client_version);
    query.addUInt64(record.host_id);
    query.addText(record.host_computer_name);
    query.addText(record.host_address);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void AccessLog::entries(const proto::router::AccessLogFilter& filter, qint64 max_entry_id, qint64 offset,
    qint64 count, proto::router::AccessLogList* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Access log is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (!isPageValid(offset, count))
    {
        LOG(ERROR) << "Invalid access log page: offset" << offset << "count" << count;
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }

    if (max_entry_id < 0 || !isFilterValid(filter))
    {
        LOG(ERROR) << "Invalid access log filter";
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }

    if (!max_entry_id)
    {
        SqlQuery max_query(db_, "SELECT IFNULL(MAX(id), 0) FROM access_log");
        if (max_query.next() != SqlQuery::StepResult::ROW)
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        max_entry_id = max_query.columnInt64(0);
    }

    const std::string where = filterClause(filter);

    SqlQuery count_query(db_, "SELECT COUNT(*) FROM access_log" + where);
    if (!count_query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    bindFilter(filter, max_entry_id, &count_query);

    if (count_query.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    const qint64 total_count = count_query.columnInt64(0);

    const std::string sql = strCat({
        "SELECT id, time, user_id, user_name, session_type, client_address, client_computer_name, "
        "client_os_name, client_version, host_id, host_computer_name, host_address "
        "FROM access_log",
        where,
        " ORDER BY id DESC LIMIT ? OFFSET ?"});

    SqlQuery query(db_, sql);
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    bindFilter(filter, max_entry_id, &query);
    query.addInt64(count);
    query.addInt64(offset);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->clear_entry();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        proto::router::AccessLogEntry* entry = out->add_entry();
        entry->set_entry_id(query.columnInt64(0));
        entry->set_time(query.columnInt64(1));
        entry->set_user_id(query.columnInt64(2));
        entry->set_user_name(query.columnTextView(3));
        entry->set_session_type(static_cast<proto::peer::SessionType>(query.columnInt64(4)));
        entry->set_client_address(query.columnTextView(5));
        entry->set_client_computer_name(query.columnTextView(6));
        entry->set_client_os_name(query.columnTextView(7));
        entry->set_client_version(query.columnTextView(8));
        entry->set_host_id(query.columnUInt64(9));
        entry->set_host_computer_name(query.columnTextView(10));
        entry->set_host_address(query.columnTextView(11));
    }

    out->set_total_count(total_count);
    out->set_max_entry_id(max_entry_id);
    out->set_error_code(proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
bool AccessLog::prune(int retention_days)
{
    if (!isValid())
    {
        LOG(ERROR) << "Access log is not valid";
        return false;
    }

    if (retention_days <= 0)
    {
        LOG(ERROR) << "Invalid access log retention:" << retention_days;
        return false;
    }

    SqlQuery query(db_, "DELETE FROM access_log WHERE time < ?");
    query.addInt64(secondsSinceEpoch() - static_cast<qint64>(retention_days) * 24 * 3600);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to prune the access log:" << db_.lastError();
        return false;
    }

    const int count = db_.changes();
    if (count > 0)
        LOG(INFO) << "Dropped" << count << "access log entries older than" << retention_days << "days";

    return true;
}
