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

#include "router/host_store.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"

namespace {

// How long a queued host removal waits for the host to acknowledge it before the record is dropped
// anyway. Long enough for a machine that spends months switched off; after it the id is gone and
// the host has to be approved again.
constexpr qint64 kHostRemovalTtlSec = 180 * 24 * 3600;

//--------------------------------------------------------------------------------------------------
bool isHostFilterValid(const proto::router::HostFilter& filter)
{
    if (filter.workspace_id_size() > proto::router::kMaxHostFilterWorkspaces)
        return false;

    for (qint64 workspace_id : filter.workspace_id())
    {
        if (workspace_id < 0)
            return false;
    }

    const std::string_view texts[] =
        { filter.address(), filter.os_name(), filter.version(), filter.cpu_arch(), filter.display_name() };

    for (std::string_view text : texts)
    {
        if (strLength(text) > proto::router::kMaxHostQueryLength)
            return false;
    }

    if (filter.host_id().size() > proto::router::kMaxHostIdLength)
        return false;

    for (char symbol : filter.host_id())
    {
        if (symbol < '0' || symbol > '9')
            return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
std::string placeholders(int count)
{
    std::string result;
    result.reserve(static_cast<size_t>(count) * 2);

    for (int i = 0; i < count; ++i)
        result += i ? ",?" : "?";

    return result;
}

//--------------------------------------------------------------------------------------------------
// A LIKE pattern that finds |text| anywhere in the value. The wildcards in |text| are escaped, so it
// is matched literally.
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
// The WHERE clause of |filter|, empty when the filter narrows nothing. bindHostFilter() binds the
// values in the order of the placeholders.
std::string hostFilterClause(const proto::router::HostFilter& filter)
{
    std::vector<std::string> conditions;

    if (filter.workspace_id_size() > 0)
        conditions.emplace_back(strCat({"workspace_id IN (", placeholders(filter.workspace_id_size()), ")"}));
    if (!filter.address().empty())
        conditions.emplace_back("address LIKE ? ESCAPE '\\'");
    if (!filter.os_name().empty())
        conditions.emplace_back("os_name LIKE ? ESCAPE '\\'");
    if (!filter.version().empty())
        conditions.emplace_back("version LIKE ? ESCAPE '\\'");
    if (!filter.cpu_arch().empty())
        conditions.emplace_back("cpu_arch LIKE ? ESCAPE '\\'");
    if (!filter.display_name().empty())
        conditions.emplace_back("casefold(display_name) LIKE casefold(?) ESCAPE '\\'");
    if (!filter.host_id().empty())
        conditions.emplace_back("CAST(id AS TEXT) LIKE ? ESCAPE '\\'");

    if (conditions.empty())
        return std::string();

    return " WHERE " + strJoin(conditions, " AND ");
}

//--------------------------------------------------------------------------------------------------
void bindHostFilter(const proto::router::HostFilter& filter, SqlQuery* query)
{
    CHECK(query);

    for (qint64 workspace_id : filter.workspace_id())
        query->addInt64(workspace_id);

    if (!filter.address().empty())
        query->addText(substringPattern(filter.address()));
    if (!filter.os_name().empty())
        query->addText(substringPattern(filter.os_name()));
    if (!filter.version().empty())
        query->addText(substringPattern(filter.version()));
    if (!filter.cpu_arch().empty())
        query->addText(substringPattern(filter.cpu_arch()));
    if (!filter.display_name().empty())
        query->addText(substringPattern(filter.display_name()));
    if (!filter.host_id().empty())
        query->addText(substringPattern(filter.host_id()));
}

} // namespace

//--------------------------------------------------------------------------------------------------
HostStore::HostStore(Database& db)
    : DatabaseStore(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
std::string_view HostStore::findByKey(std::string_view key_hash, HostId* host_id) const
{
    CHECK(host_id);

    *host_id = kInvalidHostId;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (key_hash.empty())
    {
        LOG(ERROR) << "Invalid key hash";
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(db());
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery query(db(), "SELECT id FROM hosts WHERE key=?");
    query.addBlob(key_hash);

    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    const SqlQuery::StepResult host_step = query.next();
    if (host_step == SqlQuery::StepResult::FAILED)
    {
        // A failed read must not pass for a missing row: NotFound sends the caller down the
        // approval path for a host that is already registered.
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (host_step == SqlQuery::StepResult::ROW)
    {
        *host_id = query.columnUInt64(0);
        return proto::router::kErrorOk;
    }

    // Not found in hosts - check hosts_remove. A reconnecting host whose removal was scheduled
    // while it was offline will be matched here; the caller is expected to send the remove
    // command using the returned host_id and wait for the ack before finalizing the deletion.
    SqlQuery pending(db(), "SELECT host_id FROM hosts_remove WHERE key=?");
    pending.addBlob(key_hash);

    if (!pending.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    const SqlQuery::StepResult pending_step = pending.next();
    if (pending_step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (pending_step != SqlQuery::StepResult::ROW)
        return proto::router::kErrorNotFound;

    *host_id = pending.columnUInt64(0);
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
bool HostStore::add(std::string_view key_hash, std::string_view hwid)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (key_hash.empty() || hwid.empty())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db().lastError();
        return false;
    }

    // Permanent host IDs are produced by AUTOINCREMENT and must never enter the temporary host ID range.
    // sqlite_sequence itself appears with the first AUTOINCREMENT insert into the database, so a
    // missing table is a legitimately empty sequence - only a failed read of an existing one must
    // not pass for it (the range check below would be skipped).
    HostId next_id = 1;
    SqlQuery seq_exists(db(),
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='sqlite_sequence'");
    const SqlQuery::StepResult exists_step = seq_exists.next();
    if (exists_step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to check host id sequence table:" << db().lastError();
        return false;
    }

    if (exists_step == SqlQuery::StepResult::ROW)
    {
        SqlQuery seq_query(db(), "SELECT seq FROM sqlite_sequence WHERE name='hosts'");
        const SqlQuery::StepResult seq_step = seq_query.next();
        if (seq_step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to read host id sequence:" << db().lastError();
            return false;
        }
        if (seq_step == SqlQuery::StepResult::ROW)
            next_id = static_cast<HostId>(seq_query.columnInt64(0)) + 1;
    }

    if (next_id >= kMinTempHostId)
    {
        LOG(ERROR) << "Permanent host id space is exhausted";
        return false;
    }

    SqlQuery query(db(), "INSERT INTO hosts (id, key, hwid) VALUES (NULL, ?, ?)");
    query.addBlob(key_hash);
    query.addBlob(hwid);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool HostStore::updateInfo(HostId host_id, std::string_view hwid, std::string_view computer_name,
    std::string_view cpu_arch, std::string_view version, std::string_view os_name,
    std::string_view address)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (host_id == kInvalidHostId)
    {
        LOG(ERROR) << "Invalid host id";
        return false;
    }

    const qint64 timestamp = secondsSinceEpoch();

    // The hosts row is created by add() before this method runs, so a plain UPDATE is
    // enough. If display name has never been set by the admin we seed it from computer_name so
    // the host has a readable label in the UI.
    const char kSql[] =
        "UPDATE hosts SET "
        "display_name = CASE WHEN display_name='' THEN ? ELSE display_name END, "
        "hwid=?, computer_name=?, cpu_arch=?, version=?, os_name=?, address=?, last_connect=? "
        "WHERE id=?";
    SqlQuery query(db(), kSql);
    query.addText(computer_name);
    query.addBlob(hwid);
    query.addText(computer_name);
    query.addText(cpu_arch);
    query.addText(version);
    query.addText(os_name);
    query.addText(address);
    query.addInt64(timestamp);
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    // The row is expected to exist (add() runs first); a zero-row update means it was removed
    // in between, so report that instead of a silent success.
    return db().changes() > 0;
}

//--------------------------------------------------------------------------------------------------
bool HostStore::updateTelemetry(HostId host_id, std::string_view telemetry)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db(), "UPDATE hosts SET telemetry=? WHERE id=?");
    query.addText(telemetry);
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return db().changes() > 0;
}

//--------------------------------------------------------------------------------------------------
std::string_view HostStore::telemetry(HostId host_id, std::string* telemetry) const
{
    CHECK(telemetry);

    telemetry->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    SqlQuery query(db(), "SELECT telemetry FROM hosts WHERE id=?");
    query.addUInt64(host_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (step != SqlQuery::StepResult::ROW)
        return proto::router::kErrorNotFound;

    *telemetry = query.columnTextView(0);
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view HostStore::checkEntry(HostId host_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    SqlQuery query(db(), "SELECT 1 FROM hosts WHERE id=?");
    query.addUInt64(host_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (step != SqlQuery::StepResult::ROW)
        return proto::router::kErrorNotFound;

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view HostStore::workspaceId(HostId host_id, qint64* workspace_id) const
{
    CHECK(workspace_id);

    *workspace_id = 0;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    SqlQuery query(db(), "SELECT workspace_id FROM hosts WHERE id=?");
    query.addUInt64(host_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        // A failed read must not pass for a missing host: the caller would answer NotFound for
        // a row that is still there.
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (step != SqlQuery::StepResult::ROW)
        return proto::router::kErrorNotFound;

    *workspace_id = query.columnInt64(0);
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view HostStore::modify(HostId host_id, qint64 base_revision, qint64 workspace_id,
    qint64 group_id, std::string_view display_name, std::string_view comment)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (host_id == kInvalidHostId || workspace_id < 0 || group_id < 0)
    {
        LOG(ERROR) << "Invalid host edit:" << host_id << workspace_id << group_id;
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery current(db(), "SELECT revision FROM hosts WHERE id=?");
    current.addUInt64(host_id);

    const SqlQuery::StepResult current_step = current.next();
    if (current_step == SqlQuery::StepResult::FAILED)
    {
        // A failed read must not pass for a missing host.
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (current_step != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Host not found:" << host_id;
        return proto::router::kErrorNotFound;
    }

    if (current.columnInt64(0) != base_revision)
    {
        // The edit was based on an older state of the host. Applying it would silently overwrite
        // whatever the concurrent edit did (a released host would be claimed back, a rename or a
        // move would be undone), so the client must refetch and retry instead.
        LOG(ERROR) << "Host" << host_id << "was changed concurrently (stored revision"
                   << current.columnInt64(0) << ", request based on" << base_revision << ")";
        return proto::router::kErrorConflict;
    }

    if (workspace_id > 0)
    {
        SqlQuery workspace_check(db(), "SELECT 1 FROM workspaces WHERE id=?");
        workspace_check.addInt64(workspace_id);

        const SqlQuery::StepResult check_step = workspace_check.next();
        if (check_step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return proto::router::kErrorInternalError;
        }

        if (check_step != SqlQuery::StepResult::ROW)
        {
            // The workspace of the snapshot is gone - the same stale-snapshot race as above.
            LOG(ERROR) << "Workspace not found:" << workspace_id;
            return proto::router::kErrorConflict;
        }
    }

    // The place in the tree belongs to the workspace the host is leaving, so it goes with it.
    // The note is the host's own and stays, whether the host sits in a workspace or not.
    const bool released = workspace_id == 0;
    const qint64 timestamp = secondsSinceEpoch();

    const char kSql[] =
        "UPDATE hosts SET revision=revision+1, workspace_id=?, display_name=?, group_id=?, "
        "comment=?, last_modify=? WHERE id=?";
    SqlQuery query(db(), kSql);
    query.addInt64(workspace_id);
    query.addText(display_name);
    query.addInt64(released ? 0 : group_id);
    query.addText(comment);
    query.addInt64(timestamp);
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
void HostStore::list(const proto::router::HostFilter& filter, qint64 offset, qint64 count,
    proto::router::HostList* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (!isPageValid(offset, count, proto::router::kMaxHostPageSize))
    {
        LOG(ERROR) << "Invalid host list page: offset" << offset << "count" << count;
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }

    if (!isHostFilterValid(filter))
    {
        LOG(ERROR) << "Invalid host filter";
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }

    const std::string sql = strCat({
        "SELECT id, revision, workspace_id, group_id, display_name, computer_name, cpu_arch, "
        "version, os_name, address, comment, last_connect, last_modify FROM hosts",
        hostFilterClause(filter),
        " ORDER BY id LIMIT ? OFFSET ?"});

    SqlQuery query(db(), sql);
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    bindHostFilter(filter, &query);
    query.addInt64(count);
    query.addInt64(offset);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // An error reply must not carry the partial list scanned so far.
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            out->clear_host();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        proto::router::Host* host = out->add_host();
        host->set_host_id(query.columnUInt64(0));
        host->set_revision(query.columnInt64(1));
        host->set_workspace_id(query.columnInt64(2));
        host->set_group_id(query.columnInt64(3));
        host->set_display_name(query.columnTextView(4));
        host->set_computer_name(query.columnTextView(5));
        host->set_cpu_arch(query.columnTextView(6));
        host->set_version(query.columnTextView(7));
        host->set_os_name(query.columnTextView(8));
        host->set_address(query.columnTextView(9));
        host->set_comment(query.columnTextView(10));
        host->set_last_connect(query.columnInt64(11));
        host->set_last_modify(query.columnInt64(12));
    }

    out->set_error_code(proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
void HostStore::list(qint64 workspace_id, qint64 group_id, qint64 offset,
    qint64 count, proto::router::HostList* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (!isPageValid(offset, count, proto::router::kMaxHostPageSize))
    {
        LOG(ERROR) << "Invalid host list page: offset" << offset << "count" << count;
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }
    // A negative group_id asks for the hosts of the workspace whatever group they sit in.
    const bool any_group = group_id < 0;
    const std::string sql = strCat({
        "SELECT id, revision, workspace_id, group_id, display_name, computer_name, cpu_arch, "
        "version, os_name, address, comment, last_connect, last_modify "
        "FROM hosts WHERE workspace_id=?",
        any_group ? "" : " AND group_id=?",
        " ORDER BY id LIMIT ? OFFSET ?"});

    SqlQuery query(db(), sql);
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    query.addInt64(workspace_id);
    if (!any_group)
        query.addInt64(group_id);
    query.addInt64(count);
    query.addInt64(offset);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // An error reply must not carry the partial list scanned so far.
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            out->clear_host();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        proto::router::Host* host = out->add_host();
        host->set_host_id(query.columnUInt64(0));
        host->set_revision(query.columnInt64(1));
        host->set_workspace_id(query.columnInt64(2));
        host->set_group_id(query.columnInt64(3));
        host->set_display_name(query.columnTextView(4));
        host->set_computer_name(query.columnTextView(5));
        host->set_cpu_arch(query.columnTextView(6));
        host->set_version(query.columnTextView(7));
        host->set_os_name(query.columnTextView(8));
        host->set_address(query.columnTextView(9));
        host->set_comment(query.columnTextView(10));
        host->set_last_connect(query.columnInt64(11));
        host->set_last_modify(query.columnInt64(12));
    }

    out->set_error_code(proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
std::string_view HostStore::count(const proto::router::HostFilter& filter, qint64* count) const
{
    CHECK(count);

    *count = 0;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (!isHostFilterValid(filter))
    {
        LOG(ERROR) << "Invalid host filter";
        return proto::router::kErrorInvalidRequest;
    }

    SqlQuery query(db(), "SELECT COUNT(*) FROM hosts" + hostFilterClause(filter));
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    bindHostFilter(filter, &query);

    // COUNT(*) always yields exactly one row, so anything but ROW is a database error - it must
    // not pass for an empty table.
    if (query.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    *count = query.columnInt64(0);
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view HostStore::count(qint64 workspace_id, qint64 group_id, qint64* count) const
{
    CHECK(count);

    *count = 0;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    // Same scope as the matching list() overload, including the "any group" reading of a
    // negative group_id.
    const bool any_group = group_id < 0;
    const std::string sql = strCat({
        "SELECT COUNT(*) FROM hosts WHERE workspace_id=?",
        any_group ? "" : " AND group_id=?"});

    SqlQuery query(db(), sql);
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    query.addInt64(workspace_id);
    if (!any_group)
        query.addInt64(group_id);

    // See the unfiltered overload: a missing row is an error, not an empty scope.
    if (query.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return proto::router::kErrorInternalError;
    }

    *count = query.columnInt64(0);
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
void HostStore::search(std::string_view query_text, const std::set<qint64>& workspace_ids,
    qint64 offset, qint64 count, proto::router::HostSearchResult* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (!isPageValid(offset, count, proto::router::kMaxHostPageSize))
    {
        LOG(ERROR) << "Invalid host search page: offset" << offset << "count" << count;
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }

    if (strLength(query_text) > proto::router::kMaxHostQueryLength)
    {
        LOG(ERROR) << "Host search query is too long:" << strLength(query_text);
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }

    if (workspace_ids.empty() || query_text.empty())
    {
        out->set_total_count(0);
        out->set_error_code(proto::router::kErrorOk);
        return;
    }

    // Escape LIKE wildcards so user input is matched literally. The escaped characters are ASCII,
    // which never occurs inside a UTF-8 multibyte sequence, so the byte-wise walk is safe.
    std::string pattern;
    pattern.reserve(query_text.size() + 2);
    pattern += '%';
    for (char symbol : query_text)
    {
        if (symbol == '\\' || symbol == '%' || symbol == '_')
            pattern += '\\';
        pattern += symbol;
    }
    pattern += '%';

    std::string placeholders;
    placeholders.reserve(workspace_ids.size() * 2);

    for (size_t i = 0; i < workspace_ids.size(); ++i)
        placeholders += i ? ",?" : "?";

    // The predicate is written once and used by both statements, so the count and the page can
    // never disagree about what a match is.
    const std::string where =
        " FROM hosts WHERE workspace_id IN (" + placeholders + ") "
        "AND (casefold(display_name) LIKE casefold(?) ESCAPE '\\' "
        "OR casefold(comment) LIKE casefold(?) ESCAPE '\\' "
        "OR CAST(id AS TEXT) LIKE ? ESCAPE '\\')";

    // A zero count from a failed query would make the client truncate its pagination while the
    // page itself arrives non-empty, so a count failure fails the whole request.
    const std::string count_sql = "SELECT COUNT(*)" + where;
    SqlQuery count_query(db(), count_sql);
    if (!count_query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    for (qint64 workspace_id : workspace_ids)
        count_query.addInt64(workspace_id);
    count_query.addText(pattern);
    count_query.addText(pattern);
    count_query.addText(pattern);

    // COUNT(*) always yields exactly one row, so anything but ROW is a database error.
    if (count_query.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    out->set_total_count(count_query.columnInt64(0));

    const std::string sql =
        "SELECT id, revision, workspace_id, group_id, display_name, computer_name, cpu_arch, "
        "version, os_name, address, comment, last_connect, last_modify" +
        where + " ORDER BY display_name LIMIT ? OFFSET ?";

    SqlQuery query(db(), sql);
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        out->clear_total_count();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    for (qint64 workspace_id : workspace_ids)
        query.addInt64(workspace_id);
    query.addText(pattern);
    query.addText(pattern);
    query.addText(pattern);
    query.addInt64(count);
    query.addInt64(offset);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // An error reply must not carry the partial list scanned so far, nor the count that
            // was read before it.
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            out->clear_host();
            out->clear_total_count();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        proto::router::Host* host = out->add_host();
        host->set_host_id(query.columnUInt64(0));
        host->set_revision(query.columnInt64(1));
        host->set_workspace_id(query.columnInt64(2));
        host->set_group_id(query.columnInt64(3));
        host->set_display_name(query.columnTextView(4));
        host->set_computer_name(query.columnTextView(5));
        host->set_cpu_arch(query.columnTextView(6));
        host->set_version(query.columnTextView(7));
        host->set_os_name(query.columnTextView(8));
        host->set_address(query.columnTextView(9));
        host->set_comment(query.columnTextView(10));
        host->set_last_connect(query.columnInt64(11));
        host->set_last_modify(query.columnInt64(12));
    }

    out->set_error_code(proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
bool HostStore::scheduleRemoval(HostId host_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (host_id == kInvalidHostId)
    {
        LOG(ERROR) << "Invalid host id";
        return false;
    }

    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db().lastError();
        return false;
    }

    SqlQuery select(db(), "SELECT key FROM hosts WHERE id=?");
    select.addUInt64(host_id);

    if (!select.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (select.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Host not found:" << host_id;
        return false;
    }

    const QByteArray key = select.columnBlob(0);
    const qint64 timestamp = secondsSinceEpoch();

    // OR REPLACE keeps re-scheduling idempotent: a stale hosts_remove row (same host_id, or same
    // key from a host that re-enrolled under a new id) is overwritten instead of aborting on the
    // PRIMARY KEY / UNIQUE constraint and leaving the host un-removable.
    const char kSql[] = "INSERT OR REPLACE INTO hosts_remove (host_id, key, timestamp) VALUES (?, ?, ?)";
    SqlQuery insert(db(), kSql);
    insert.addUInt64(host_id);
    insert.addBlob(key);
    insert.addInt64(timestamp);

    if (!insert.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    SqlQuery del(db(), "DELETE FROM hosts WHERE id=?");
    del.addUInt64(host_id);

    if (!del.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool HostStore::hasPendingRemoval(HostId host_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (host_id == kInvalidHostId)
        return false;

    SqlQuery query(db(), "SELECT 1 FROM hosts_remove WHERE host_id=?");
    query.addUInt64(host_id);

    return query.next() == SqlQuery::StepResult::ROW;
}

//--------------------------------------------------------------------------------------------------
bool HostStore::finalizeRemoval(HostId host_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (host_id == kInvalidHostId)
    {
        LOG(ERROR) << "Invalid host id";
        return false;
    }

    SqlQuery query(db(), "DELETE FROM hosts_remove WHERE host_id=?");
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    // Idempotent: the desired end state is "no hosts_remove row for this id". A second finalization
    // (duplicate ack, or another session already finalized) deletes zero rows but still succeeded,
    // so report success instead of a spurious failure.
    return true;
}

//--------------------------------------------------------------------------------------------------
bool HostStore::pruneExpiredRemovals()
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db(), "DELETE FROM hosts_remove WHERE timestamp < ?");
    query.addInt64(secondsSinceEpoch() - kHostRemovalTtlSec);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to prune expired host removals:" << db().lastError();
        return false;
    }

    const int count = db().changes();
    if (count > 0)
        LOG(INFO) << "Dropped" << count << "host removals nobody acknowledged";

    return true;
}
