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

#include "router/group_store.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"

//--------------------------------------------------------------------------------------------------
GroupStore::GroupStore(Database& db)
    : DatabaseStore(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void GroupStore::list(qint64 workspace_id, proto::router::GroupList* out) const
{
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (workspace_id <= 0)
    {
        LOG(ERROR) << "Invalid workspace id:" << workspace_id;
        out->set_error_code(proto::router::kErrorInvalidData);
        return;
    }

    const char kSql[] =
        "SELECT id, revision, IFNULL(parent_id, 0), name, comment FROM host_groups "
        "WHERE workspace_id=?";
    SqlQuery query(sql(), kSql);
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    query.addInt64(workspace_id);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // An error reply must not carry the partial list scanned so far.
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
            out->clear_group();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        proto::router::Group* group = out->add_group();
        group->set_entry_id(query.columnInt64(0));
        group->set_revision(query.columnInt64(1));
        group->set_parent_id(query.columnInt64(2));
        group->set_name(query.columnTextView(3));
        group->set_comment(query.columnTextView(4));
    }

    out->set_error_code(proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
std::string_view GroupStore::find(qint64 workspace_id, qint64 entry_id, Group* group) const
{
    CHECK(group);

    *group = Group();

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (workspace_id <= 0 || entry_id <= 0)
    {
        LOG(ERROR) << "Invalid arguments: workspace_id=" << workspace_id << "entry_id=" << entry_id;
        return proto::router::kErrorNotFound;
    }

    const char kSql[] =
        "SELECT id, revision, IFNULL(parent_id, 0), name, comment FROM host_groups "
        "WHERE id=? AND workspace_id=?";
    SqlQuery query(sql(), kSql);
    query.addInt64(entry_id);
    query.addInt64(workspace_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        // A failed read must not pass for a missing group.
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return proto::router::kErrorInternalError;
    }

    if (step != SqlQuery::StepResult::ROW)
        return proto::router::kErrorNotFound;

    group->entry_id  = query.columnInt64(0);
    group->revision  = query.columnInt64(1);
    group->parent_id = query.columnInt64(2);
    group->name      = query.columnTextView(3);
    group->comment   = query.columnTextView(4);
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view GroupStore::add(qint64 workspace_id, qint64 parent_id, std::string_view name,
    std::string_view comment, qint64* entry_id)
{
    CHECK(entry_id);

    *entry_id = -1;

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (workspace_id <= 0)
    {
        LOG(ERROR) << "Invalid workspace id:" << workspace_id;
        return proto::router::kErrorInvalidData;
    }

    if (parent_id < 0)
    {
        LOG(ERROR) << "Invalid parent id:" << parent_id;
        return proto::router::kErrorInvalidData;
    }

    if (strTrimmed(name).empty() || strLength(strTrimmed(name)) > proto::router::kMaxEntryNameLength)
    {
        LOG(ERROR) << "Invalid group name";
        return proto::router::kErrorInvalidData;
    }

    if (strLength(comment) > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Group comment is too long:" << strLength(comment);
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(sql());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << sql().lastError();
        return proto::router::kErrorInternalError;
    }

    // The parent (if any) must exist in this workspace. Filtering on workspace_id here is the
    // only safeguard against the API silently linking groups across workspace boundaries.
    if (parent_id != 0)
    {
        SqlQuery parent_check(sql(), "SELECT 1 FROM host_groups WHERE id=? AND workspace_id=?");
        parent_check.addInt64(parent_id);
        parent_check.addInt64(workspace_id);

        if (!parent_check.isValid())
        {
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
            return proto::router::kErrorInternalError;
        }

        if (parent_check.next() != SqlQuery::StepResult::ROW)
            return proto::router::kErrorInvalidData;
    }

    const char kSql[] =
        "INSERT INTO host_groups (workspace_id, parent_id, name, comment) VALUES (?, ?, ?, ?)";
    SqlQuery insert(sql(), kSql);
    insert.addInt64(workspace_id);
    if (parent_id == 0)
        insert.addNull();
    else
        insert.addInt64(parent_id);
    insert.addText(name);
    insert.addText(comment);

    if (!insert.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return proto::router::kErrorInternalError;
    }

    const qint64 new_id = sql().lastInsertRowId();

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << sql().lastError();
        return proto::router::kErrorInternalError;
    }

    *entry_id = new_id;
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view GroupStore::modify(qint64 workspace_id, qint64 entry_id, qint64 base_revision,
    qint64 new_parent_id, std::string_view name, std::string_view comment)
{
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (workspace_id <= 0 || entry_id <= 0 || new_parent_id < 0)
    {
        LOG(ERROR) << "Invalid arguments: workspace_id=" << workspace_id
                   << "entry_id=" << entry_id << "new_parent_id=" << new_parent_id;
        return proto::router::kErrorInvalidData;
    }

    if (strTrimmed(name).empty() || strLength(strTrimmed(name)) > proto::router::kMaxEntryNameLength)
    {
        LOG(ERROR) << "Invalid group name";
        return proto::router::kErrorInvalidData;
    }

    if (strLength(comment) > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Group comment is too long:" << strLength(comment);
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(sql());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << sql().lastError();
        return proto::router::kErrorInternalError;
    }

    // Verify the group being modified exists in this workspace.
    SqlQuery select_self(sql(), "SELECT revision FROM host_groups WHERE id=? AND workspace_id=?");
    select_self.addInt64(entry_id);
    select_self.addInt64(workspace_id);

    if (!select_self.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return proto::router::kErrorInternalError;
    }

    if (select_self.next() != SqlQuery::StepResult::ROW)
        return proto::router::kErrorNotFound;

    if (select_self.columnInt64(0) != base_revision)
    {
        LOG(ERROR) << "Group" << entry_id << "was changed concurrently (stored revision"
                   << select_self.columnInt64(0) << ", request based on" << base_revision << ")";
        return proto::router::kErrorConflict;
    }

    if (new_parent_id != 0)
    {
        // The new parent must exist in this workspace.
        SqlQuery parent_check(sql(), "SELECT 1 FROM host_groups WHERE id=? AND workspace_id=?");
        parent_check.addInt64(new_parent_id);
        parent_check.addInt64(workspace_id);

        if (!parent_check.isValid())
        {
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
            return proto::router::kErrorInternalError;
        }

        if (parent_check.next() != SqlQuery::StepResult::ROW)
            return proto::router::kErrorInvalidData;

        // Cycle protection. Walk parent links from new_parent_id upward; if entry_id appears
        // anywhere on that chain, the move would put the group inside its own subtree. The
        // recursive CTE bounds itself naturally on a well-formed tree (parent_id IS NULL marks
        // the top) and SQLite caps runaway recursion at SQLITE_MAX_RECURSION_DEPTH. parent_id
        // links never cross workspace boundaries by construction (enforced at insert and
        // modify), so no workspace_id filter is needed inside the recursion.
        const char kSql[] =
            "WITH RECURSIVE ancestors(id) AS ("
            "    SELECT id FROM host_groups WHERE id=?"
            "    UNION ALL"
            "    SELECT g.parent_id FROM host_groups g JOIN ancestors a ON g.id = a.id "
            "      WHERE g.parent_id IS NOT NULL"
            ") SELECT 1 FROM ancestors WHERE id=? LIMIT 1";
        SqlQuery cycle_check(sql(), kSql);
        cycle_check.addInt64(new_parent_id);
        cycle_check.addInt64(entry_id);

        if (!cycle_check.isValid())
        {
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
            return proto::router::kErrorInternalError;
        }

        if (cycle_check.next() == SqlQuery::StepResult::ROW)
            return proto::router::kErrorInvalidData;
    }

    const char kSql[] =
        "UPDATE host_groups SET revision=revision+1, parent_id=?, name=?, comment=? "
        "WHERE id=? AND workspace_id=?";
    SqlQuery update_self(sql(), kSql);
    if (new_parent_id == 0)
        update_self.addNull();
    else
        update_self.addInt64(new_parent_id);
    update_self.addText(name);
    update_self.addText(comment);
    update_self.addInt64(entry_id);
    update_self.addInt64(workspace_id);

    if (!update_self.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return proto::router::kErrorInternalError;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << sql().lastError();
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view GroupStore::remove(qint64 workspace_id, qint64 entry_id)
{
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (workspace_id <= 0 || entry_id <= 0)
    {
        LOG(ERROR) << "Invalid arguments: workspace_id=" << workspace_id << "entry_id=" << entry_id;
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(sql());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << sql().lastError();
        return proto::router::kErrorInternalError;
    }

    const char kReleaseHostsSql[] =
        "WITH RECURSIVE deleted_groups(id) AS ("
        "    SELECT id FROM host_groups WHERE id=? AND workspace_id=?"
        "    UNION ALL"
        "    SELECT child.id FROM host_groups child "
        "    JOIN deleted_groups parent ON child.parent_id = parent.id "
        "    WHERE child.workspace_id=?"
        ") UPDATE hosts SET revision=revision+1, group_id=0 WHERE workspace_id=? "
        "AND group_id IN (SELECT id FROM deleted_groups)";
    SqlQuery release_hosts(sql(), kReleaseHostsSql);
    release_hosts.addInt64(entry_id);
    release_hosts.addInt64(workspace_id);
    release_hosts.addInt64(workspace_id);
    release_hosts.addInt64(workspace_id);

    if (!release_hosts.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery delete_group(sql(), "DELETE FROM host_groups WHERE id=? AND workspace_id=?");
    delete_group.addInt64(entry_id);
    delete_group.addInt64(workspace_id);

    if (!delete_group.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return proto::router::kErrorInternalError;
    }

    if (sql().changes() == 0)
        return proto::router::kErrorNotFound;

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << sql().lastError();
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}
