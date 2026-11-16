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

#include "router/database.h"

#include <unordered_map>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"

//--------------------------------------------------------------------------------------------------
void Database::workspaceListForAdmin(qint64 workspace_id, proto::router::WorkspaceList* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    // Indexed by id so the membership query below can attach every member to its workspace.
    std::unordered_map<qint64, proto::router::Workspace*> by_id;
    {
        const std::string sql = strCat({
            "SELECT id, revision, name, comment FROM workspaces",
            workspace_id > 0 ? " WHERE id = ?" : ""});

        SqlQuery query(db_, sql);
        if (!query.isValid())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (workspace_id > 0)
            query.addInt64(workspace_id);

        for (;;)
        {
            const SqlQuery::StepResult step = query.next();
            if (step == SqlQuery::StepResult::FAILED)
            {
                // An error reply must not carry the partial list scanned so far.
                LOG(ERROR) << "Unable to execute query:" << db_.lastError();
                out->clear_workspace();
                out->set_error_code(proto::router::kErrorInternalError);
                return;
            }

            if (step == SqlQuery::StepResult::DONE)
                break;

            proto::router::Workspace* item = out->add_workspace();
            item->set_entry_id(query.columnInt64(0));
            item->set_revision(query.columnInt64(1));
            item->set_name(query.columnTextView(2));
            item->set_comment(query.columnTextView(3));
            by_id.emplace(item->entry_id(), item);
        }
    }

    if (by_id.empty())
    {
        out->set_error_code(proto::router::kErrorOk);
        return;
    }

    // Every membership of the listed workspaces in a single query (no per-workspace round trip).
    const std::string sql = strCat({
        "SELECT workspace_id, user_id FROM workspace_access",
        workspace_id > 0 ? " WHERE workspace_id = ?" : ""});

    SqlQuery query(db_, sql);
    if (!query.isValid())
    {
        // The first loop already filled |out| - an error reply must not carry that partial list.
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->clear_workspace();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (workspace_id > 0)
        query.addInt64(workspace_id);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // An error reply must not carry the partial list scanned so far.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->clear_workspace();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        const auto it = by_id.find(query.columnInt64(0));
        if (it == by_id.end())
            continue;

        it->second->add_user_id(query.columnInt64(1));
    }

    out->set_error_code(proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
void Database::workspaceListForUser(qint64 user_id, qint64 workspace_id,
    proto::router::WorkspaceList* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    // The membership of the others is none of the business of a regular session, so the reply
    // carries the workspaces alone.
    const std::string sql = strCat({
        "SELECT workspaces.id, workspaces.revision, workspaces.name, workspaces.comment "
        "FROM workspaces JOIN workspace_access ON workspace_access.workspace_id = workspaces.id "
        "AND workspace_access.user_id = ?",
        workspace_id > 0 ? " AND workspaces.id = ?" : ""});

    SqlQuery query(db_, sql);
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    query.addInt64(user_id);
    if (workspace_id > 0)
        query.addInt64(workspace_id);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // An error reply must not carry the partial list scanned so far.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->clear_workspace();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        proto::router::Workspace* item = out->add_workspace();
        item->set_entry_id(query.columnInt64(0));
        item->set_revision(query.columnInt64(1));
        item->set_name(query.columnTextView(2));
        item->set_comment(query.columnTextView(3));
    }

    out->set_error_code(proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
Workspace Database::findWorkspace(qint64 entry_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Workspace();
    }

    if (entry_id <= 0)
    {
        LOG(ERROR) << "Invalid workspace id:" << entry_id;
        return Workspace();
    }

    SqlQuery query(db_, "SELECT id, name, comment FROM workspaces WHERE id=?");
    query.addInt64(entry_id);

    if (query.next() != SqlQuery::StepResult::ROW)
        return Workspace();

    Workspace workspace;
    workspace.entry_id = query.columnInt64(0);
    workspace.name     = query.columnTextView(1);
    workspace.comment  = query.columnTextView(2);
    return workspace;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::addWorkspace(std::string_view name, std::string_view comment,
    const std::vector<qint64>& initial_access, qint64* entry_id)
{
    CHECK(entry_id);

    *entry_id = -1;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (!Workspace::isValidName(name))
    {
        LOG(ERROR) << "Invalid workspace name:" << name;
        return proto::router::kErrorInvalidData;
    }

    if (strLength(comment) > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Workspace comment is too long:" << strLength(comment);
        return proto::router::kErrorInvalidData;
    }

    std::set<qint64> initial_ids;
    for (qint64 user_id : initial_access)
    {
        if (user_id <= 0)
        {
            LOG(ERROR) << "Invalid access record (user_id <= 0)";
            return proto::router::kErrorInvalidData;
        }

        if (initial_ids.contains(user_id))
        {
            LOG(ERROR) << "Duplicate user_id in initial access list:" << user_id;
            return proto::router::kErrorInvalidData;
        }

        initial_ids.insert(user_id);
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery check(db_, "SELECT 1 FROM workspaces WHERE name=?");
    check.addText(name);

    if (!check.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    const SqlQuery::StepResult check_step = check.next();
    if (check_step == SqlQuery::StepResult::FAILED)
    {
        // A failed check must not pass for a free name: the INSERT below would then answer with
        // a misleading internal error from the UNIQUE constraint.
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (check_step == SqlQuery::StepResult::ROW)
        return proto::router::kErrorAlreadyExists;

    SqlQuery insert_workspace(db_, "INSERT INTO workspaces (id, name, comment) VALUES (NULL, ?, ?)");
    insert_workspace.addText(name);
    insert_workspace.addText(comment);

    if (!insert_workspace.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    const qint64 new_id = db_.lastInsertRowId();

    SqlQuery insert_access(db_,
        "INSERT INTO workspace_access (workspace_id, user_id) VALUES (?, ?)");

    for (qint64 user_id : initial_access)
    {
        const std::string_view user_error = checkAccessUser(user_id);
        if (user_error != proto::router::kErrorOk)
            return user_error;

        insert_access.reset();
        insert_access.addInt64(new_id);
        insert_access.addInt64(user_id);

        if (!insert_access.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    *entry_id = new_id;
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::modifyWorkspace(qint64 entry_id, qint64 base_revision,
    std::string_view name, std::string_view comment, const std::vector<qint64>& desired_access)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (entry_id <= 0)
    {
        LOG(ERROR) << "Invalid workspace id:" << entry_id;
        return proto::router::kErrorInvalidData;
    }

    if (!Workspace::isValidName(name))
    {
        LOG(ERROR) << "Invalid workspace name:" << name;
        return proto::router::kErrorInvalidData;
    }

    if (strLength(comment) > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Workspace comment is too long:" << strLength(comment);
        return proto::router::kErrorInvalidData;
    }

    std::set<qint64> desired_ids;
    for (qint64 user_id : desired_access)
    {
        if (user_id <= 0)
        {
            LOG(ERROR) << "Invalid access record (user_id <= 0)";
            return proto::router::kErrorInvalidData;
        }

        if (desired_ids.contains(user_id))
        {
            LOG(ERROR) << "Duplicate user_id in desired access list:" << user_id;
            return proto::router::kErrorInvalidData;
        }

        desired_ids.insert(user_id);
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    // COUNT(*) always yields exactly one row, so a failed next() is a database error and not
    // a missing workspace.
    SqlQuery exists_check(db_, "SELECT COUNT(*), IFNULL(MAX(revision), 0) FROM workspaces WHERE id=?");
    exists_check.addInt64(entry_id);

    if (exists_check.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to check workspace existence:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (exists_check.columnInt64(0) == 0)
        return proto::router::kErrorNotFound;

    if (exists_check.columnInt64(1) != base_revision)
    {
        // The edit was based on an older state of the workspace. Applying it would silently
        // overwrite whatever the concurrent change did (revoked access entries would be granted
        // back), so the client must refetch and retry instead.
        LOG(ERROR) << "Workspace" << entry_id << "was changed concurrently (stored revision"
                   << exists_check.columnInt64(1) << ", request based on" << base_revision << ")";
        return proto::router::kErrorConflict;
    }

    SqlQuery name_check(db_, "SELECT id FROM workspaces WHERE name=?");
    name_check.addText(name);

    if (!name_check.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    const SqlQuery::StepResult name_step = name_check.next();
    if (name_step == SqlQuery::StepResult::FAILED)
    {
        // A failed check must not pass for a free name: the UPDATE below would then answer with
        // a misleading internal error from the UNIQUE constraint.
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (name_step == SqlQuery::StepResult::ROW && name_check.columnInt64(0) != entry_id)
        return proto::router::kErrorAlreadyExists;

    SqlQuery update_workspace(db_,
        "UPDATE workspaces SET revision=revision+1, name=?, comment=? WHERE id=?");
    update_workspace.addText(name);
    update_workspace.addText(comment);
    update_workspace.addInt64(entry_id);

    if (!update_workspace.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery select_current(db_, "SELECT user_id FROM workspace_access WHERE workspace_id=?");
    select_current.addInt64(entry_id);

    if (!select_current.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    std::set<qint64> current_ids;
    for (;;)
    {
        const SqlQuery::StepResult step = select_current.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A partial set would make existing entries look new and the inserts below would
            // collide with them.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        current_ids.insert(select_current.columnInt64(0));
    }

    SqlQuery delete_access(db_, "DELETE FROM workspace_access WHERE workspace_id=? AND user_id=?");

    for (qint64 user_id : current_ids)
    {
        if (desired_ids.contains(user_id))
            continue;

        delete_access.reset();
        delete_access.addInt64(entry_id);
        delete_access.addInt64(user_id);

        if (!delete_access.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }
    }

    SqlQuery insert_access(db_,
        "INSERT INTO workspace_access (workspace_id, user_id) VALUES (?, ?)");

    for (qint64 user_id : desired_access)
    {
        if (current_ids.contains(user_id))
            continue;

        const std::string_view user_error = checkAccessUser(user_id);
        if (user_error != proto::router::kErrorOk)
            return user_error;

        insert_access.reset();
        insert_access.addInt64(entry_id);
        insert_access.addInt64(user_id);

        if (!insert_access.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::removeWorkspace(qint64 entry_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (entry_id <= 0)
    {
        LOG(ERROR) << "Invalid workspace id:" << entry_id;
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    // COUNT(*) always yields exactly one row, so a failed next() is a database error and not
    // a missing workspace.
    SqlQuery exists_check(db_, "SELECT COUNT(*) FROM workspaces WHERE id=?");
    exists_check.addInt64(entry_id);

    if (exists_check.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to check workspace existence:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (exists_check.columnInt64(0) == 0)
        return proto::router::kErrorNotFound;

    SqlQuery release_hosts(db_,
        "UPDATE hosts SET revision=revision+1, workspace_id=0, group_id=0 WHERE workspace_id=?");
    release_hosts.addInt64(entry_id);

    if (!release_hosts.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery delete_workspace(db_, "DELETE FROM workspaces WHERE id=?");
    delete_workspace.addInt64(entry_id);

    if (!delete_workspace.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
bool Database::workspaceIds(std::set<qint64>* workspace_ids) const
{
    CHECK(workspace_ids);

    workspace_ids->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "SELECT id FROM workspaces");

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return false;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        workspace_ids->insert(query.columnInt64(0));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::workspaceAccessIdsForUser(qint64 user_id, std::set<qint64>* workspace_ids) const
{
    CHECK(workspace_ids);

    workspace_ids->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "SELECT workspace_id FROM workspace_access WHERE user_id=?");
    query.addInt64(user_id);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return false;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        workspace_ids->insert(query.columnInt64(0));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::checkWorkspaceAccess(qint64 user_id, qint64 workspace_id) const
{
    if (user_id <= 0 || workspace_id <= 0)
        return proto::router::kErrorAccessDenied;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    SqlQuery query(db_, "SELECT 1 FROM workspace_access WHERE workspace_id=? AND user_id=?");
    query.addInt64(workspace_id);
    query.addInt64(user_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        // A failed read must not pass for a denial: an authorization check has to tell "no
        // access" from "cannot tell".
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (step != SqlQuery::StepResult::ROW)
        return proto::router::kErrorAccessDenied;

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::checkAccessUser(qint64 user_id)
{
    SqlQuery select(db_, "SELECT 1 FROM users WHERE id=?");
    select.addInt64(user_id);

    if (!select.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    const SqlQuery::StepResult step = select.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    // An entry for a user that is gone is how a concurrent delete looks from here (the sender
    // still saw the user in its snapshot), and the recovery is a refetch - so a conflict, not
    // bad data.
    if (step != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Access entry for unknown user:" << user_id;
        return proto::router::kErrorConflict;
    }

    return proto::router::kErrorOk;
}
