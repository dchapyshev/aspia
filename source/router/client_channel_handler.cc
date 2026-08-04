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

#include "router/client_channel_handler.h"

#include <set>
#include <unordered_map>

#include "base/logging.h"
#include "base/peer/router_user.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "router/database.h"
#include "router/workers/client_worker.h"

//--------------------------------------------------------------------------------------------------
// static
void ClientChannelHandler::handleHostList(Database& database, const RequestCaller& caller,
                                          const proto::router::HostListRequest& request,
                                          proto::router::HostList* out)
{
    const proto::router::HostListRequest::Mode mode = request.mode();
    const qint64 workspace_id = request.workspace_id();
    const qint64 group_id = request.group_id();
    const bool is_admin = caller.session_type == proto::router::SESSION_TYPE_ADMIN;

    out->set_workspace_id(workspace_id);
    out->set_group_id(group_id);

    if (mode != proto::router::HostListRequest::MODE_ALL &&
        mode != proto::router::HostListRequest::MODE_FILTERED)
    {
        LOG(ERROR) << "Unknown host list mode:" << mode;
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }

    if (mode == proto::router::HostListRequest::MODE_ALL && !is_admin)
    {
        LOG(ERROR) << "Non-admin requested MODE_ALL host list";
        out->set_error_code(proto::router::kErrorAccessDenied);
        return;
    }

    if (mode == proto::router::HostListRequest::MODE_FILTERED)
    {
        // "No access" and "could not check" are different answers - a database error must not be
        // reported as a denial.
        bool access_known = false;
        const bool has_access =
            database.hasWorkspaceAccess(caller.user_id, workspace_id, &access_known);
        if (!access_known)
        {
            LOG(ERROR) << "Unable to check access to workspace" << workspace_id;
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (!has_access)
        {
            LOG(ERROR) << "User" << caller.user_id << "has no access to workspace" << workspace_id;
            out->set_error_code(proto::router::kErrorAccessDenied);
            return;
        }
    }

    // A zero count from a failed query would make the client truncate its pagination while the
    // list itself arrives non-empty - so a count failure fails the whole request.
    bool count_known = false;
    if (mode == proto::router::HostListRequest::MODE_ALL)
    {
        out->set_total_count(database.hostCount(&count_known));
        if (count_known)
            database.hosts(request.start_item(), request.end_item(), out);
    }
    else
    {
        out->set_total_count(database.hostCount(workspace_id, group_id, &count_known));
        if (count_known)
            database.hosts(workspace_id, group_id, request.start_item(), request.end_item(), out);
    }

    if (!count_known)
        out->set_error_code(proto::router::kErrorInternalError);

    // hosts() drops the partial list from an error reply; the count computed up front must not
    // survive it either.
    if (out->error_code() != proto::router::kErrorOk)
        out->clear_total_count();
}

//--------------------------------------------------------------------------------------------------
// static
void ClientChannelHandler::handleHostSearch(Database& database, const RequestCaller& caller,
                                            const proto::router::HostSearchRequest& request,
                                            proto::router::HostSearchResult* out)
{
    // Search is always scoped to every workspace the user can access, regardless of session type.
    // Only the ids are needed here, so avoid pulling each membership's wrapped_gk blob.
    std::set<qint64> workspace_ids;
    if (!database.workspaceAccessIdsForUser(caller.user_id, &workspace_ids))
    {
        LOG(ERROR) << "Failed to read workspace access list for user" << caller.user_id;
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    database.searchHosts(QString::fromStdString(request.query()), workspace_ids, out);
}

//--------------------------------------------------------------------------------------------------
// static
void ClientChannelHandler::handleWorkspaceList(Database& database, const RequestCaller& caller,
                                               const proto::router::WorkspaceListRequest& request,
                                               proto::router::WorkspaceList* out)
{
    // Each session sees only the workspaces it has a workspace_access entry for. Admins get the
    // full access list per workspace (needed to manage membership); other sessions get only their
    // own entry - the membership of a workspace is not theirs to see. workspace_id == 0 means all
    // visible workspaces; > 0 narrows to a single entry.
    if (caller.session_type == proto::router::SESSION_TYPE_ADMIN)
        database.workspaceListWithAllAccess(caller.user_id, request.workspace_id(), out);
    else
        database.workspaceListWithOwnAccess(caller.user_id, request.workspace_id(), out);
}

//--------------------------------------------------------------------------------------------------
// static
void ClientChannelHandler::handleGroupList(Database& database, const RequestCaller& caller,
                                           const proto::router::GroupListRequest& request,
                                           proto::router::GroupList* out)
{
    const qint64 workspace_id = request.workspace_id();
    out->set_workspace_id(workspace_id);

    if (workspace_id <= 0)
    {
        LOG(ERROR) << "Invalid workspace id in group list request:" << workspace_id;
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }

    // "No access" and "could not check" are different answers - a database error must not be
    // reported as a denial.
    bool access_known = false;
    const bool has_access = database.hasWorkspaceAccess(caller.user_id, workspace_id, &access_known);
    if (!access_known)
    {
        LOG(ERROR) << "Unable to check access to workspace" << workspace_id;
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (!has_access)
    {
        LOG(ERROR) << "User" << caller.user_id << "has no access to workspace" << workspace_id;
        out->set_error_code(proto::router::kErrorAccessDenied);
        return;
    }

    database.groupList(workspace_id, out);
}

//--------------------------------------------------------------------------------------------------
// static
ClientChannelHandler::PasswordResult ClientChannelHandler::handleChangePassword(
    Database& database, const RequestCaller& caller,
    const proto::router::ChangePasswordRequest& request)
{
    PasswordResult result;

    // Checked before the lookup below: a user that could not be read must not pass for a user that
    // is not there - the answers mean different things to the client.
    if (!database.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        result.error_code = proto::router::kErrorInternalError;
        return result;
    }

    // Read-modify-write outside a transaction: the window between this findUser and the modifyUser
    // below is closed only by every users/workspaces write going through the single ClientWorker
    // thread. If client sessions are ever spread over several workers, this must move inside one
    // transaction.
    RouterUser user = database.findUser(caller.user_id);
    if (!user.isValid())
    {
        // The same concurrent delete caught a moment later inside modifyUser answers
        // kErrorNotFound - one event, one code.
        LOG(WARNING) << "Authenticated user not found in database (user_id:" << caller.user_id << ")";
        result.error_code = proto::router::kErrorNotFound;
        return result;
    }

    // Replace only the password-derived fields; keep name, group, sessions, flags intact.
    user.salt             = QByteArray::fromStdString(request.salt());
    user.verifier         = QByteArray::fromStdString(request.verifier());
    user.public_key       = QByteArray::fromStdString(request.public_key());
    user.wrap_private_key = QByteArray::fromStdString(request.wrap_private_key());
    user.wrap_salt        = QByteArray::fromStdString(request.wrap_salt());

    if (!user.isValid())
    {
        LOG(ERROR) << "Rotated credentials produced an invalid user record";
        result.error_code = proto::router::kErrorInvalidData;
        return result;
    }

    // The rotation produced a new key pair, so the workspace keys re-sealed by the client to the
    // new public key must replace the stored ones (now sealed to the old, discarded key).
    std::unordered_map<qint64, QByteArray> wrapped_keys;
    wrapped_keys.reserve(request.workspace_key_size());

    for (int i = 0; i < request.workspace_key_size(); ++i)
    {
        const proto::router::ChangePasswordRequest::WorkspaceKey& wk = request.workspace_key(i);
        wrapped_keys.emplace(wk.workspace_id(), QByteArray::fromStdString(wk.wrapped_gk()));
    }

    // Credentials and re-wrapped keys are persisted atomically: the password is rotated only if a
    // re-sealed key is present for every workspace the user can access, so a partial set can never
    // leave the user without workspace access. Only the password-derived fields differ here (the
    // rest were loaded from the database), so reusing modifyUser writes back identical values.
    const std::string_view error_code = database.modifyUser(user, wrapped_keys, caller.user_id);
    result.error_code = error_code;

    if (error_code != proto::router::kErrorOk)
    {
        LOG(ERROR) << "Failed to change password for user" << caller.name << ":" << error_code;
        return result;
    }

    // NOTIFY_USERS only: the repair branch of Database::modifyUser cannot create access entries on
    // this path - the keys of the request come from the user's own cryptor cache, which only ever
    // holds the workspaces the user already has an access entry for.
    result.notify_flags = ClientWorker::NOTIFY_USERS;
    return result;
}
