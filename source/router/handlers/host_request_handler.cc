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

#include "router/handlers/host_request_handler.h"

#include <set>

#include "base/logging.h"
#include "base/peer/host_id.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"
#include "router/database.h"
#include "router/handlers/workspace_request_handler.h"
#include "router/workers/client_worker.h"

//--------------------------------------------------------------------------------------------------
RequestResult handleHostRequest(Database& database, const RequestCaller& caller,
                                const proto::router::HostRequest& request)
{
    RequestResult result;

    if (request.command_name() != proto::router::kCommandHostModify)
    {
        LOG(ERROR) << "Unknown host edit command:" << request.command_name();
        result.error_code = proto::router::kErrorInvalidRequest;
        return result;
    }

    const proto::router::Host& host = request.host();
    const HostId host_id = host.host_id();

    if (host.display_name().size() > proto::router::kMaxEntryNameLength ||
        host.comment().size() > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Oversized field in host edit request for host" << host_id;
        result.error_code = proto::router::kErrorInvalidData;
        return result;
    }

    // "Not found" and "could not read" are different answers - a database error must not be
    // reported as a missing host.
    qint64 workspace_id = 0;
    const std::string_view workspace_code = database.hostWorkspaceId(host_id, &workspace_id);
    if (workspace_code != proto::router::kErrorOk)
    {
        LOG(ERROR) << "Unable to resolve workspace of host" << host_id;
        result.error_code = workspace_code;
        return result;
    }

    // Only an administrator moves a host between workspaces. Anybody else edits the host inside
    // the workspace it already sits in and must be a member of that workspace. An administrator
    // manages every workspace of the router and needs no membership.
    const bool is_admin = caller.session_type == proto::router::SESSION_TYPE_ADMIN;
    const qint64 target_workspace_id = host.workspace_id();

    if (target_workspace_id < 0)
    {
        LOG(ERROR) << "Invalid workspace id in host edit request:" << target_workspace_id;
        result.error_code = proto::router::kErrorInvalidData;
        return result;
    }

    if (!is_admin)
    {
        if (target_workspace_id != workspace_id)
        {
            LOG(ERROR) << "User" << caller.user_id << "cannot move host" << host_id
                       << "from workspace" << workspace_id << "to" << target_workspace_id;
            result.error_code = proto::router::kErrorAccessDenied;
            return result;
        }

        if (workspace_id == 0)
        {
            LOG(ERROR) << "User" << caller.user_id << "cannot edit unassigned host" << host_id;
            result.error_code = proto::router::kErrorAccessDenied;
            return result;
        }

        const std::string_view access_code =
            database.checkWorkspaceAccess(caller.user_id, workspace_id);
        if (access_code != proto::router::kErrorOk)
        {
            LOG(ERROR) << "User" << caller.user_id << "cannot edit host" << host_id
                       << "(workspace_id=" << workspace_id << ")";
            result.error_code = access_code;
            return result;
        }
    }

    // group_id == 0 keeps the host at the workspace root; any other value must reference a group
    // in the workspace the host ends up in. A negative or unknown id is rejected here (the hosts
    // table has no foreign key on group_id, so an unchecked value would orphan the host). A
    // released host keeps no group, so there is nothing to check.
    const qint64 group_id = host.group_id();
    if (group_id != 0 && target_workspace_id != 0)
    {
        Group group;
        const std::string_view group_code = database.findGroup(target_workspace_id, group_id, &group);
        if (group_code == proto::router::kErrorNotFound)
        {
            LOG(ERROR) << "Group" << group_id << "not found in workspace" << target_workspace_id;
            result.error_code = proto::router::kErrorInvalidData;
            return result;
        }

        if (group_code != proto::router::kErrorOk)
        {
            LOG(ERROR) << "Unable to check group" << group_id << "in workspace" << target_workspace_id;
            result.error_code = group_code;
            return result;
        }
    }

    const std::string_view error_code = database.modifyHost(
        host_id, host.revision(), target_workspace_id, group_id, host.display_name(), host.comment());
    if (error_code != proto::router::kErrorOk)
    {
        result.error_code = error_code;
        return result;
    }

    result.error_code = proto::router::kErrorOk;
    result.notify_flags = ClientWorker::NOTIFY_HOSTS;
    return result;
}

//--------------------------------------------------------------------------------------------------
void handleHostList(Database& database, const RequestCaller& caller,
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
        // "Any group" narrows nothing without a workspace. Such a request would list every host
        // of the router, which only MODE_ALL does and only for an administrator.
        if (group_id < 0 && workspace_id <= 0)
        {
            LOG(ERROR) << "Any-group host list without a workspace";
            out->set_error_code(proto::router::kErrorInvalidRequest);
            return;
        }

        const std::string_view access_code = checkWorkspaceAccess(database, caller, workspace_id);
        if (access_code != proto::router::kErrorOk)
        {
            out->set_error_code(access_code);
            return;
        }
    }

    // A zero count from a failed query would make the client truncate its pagination while the
    // list itself arrives non-empty - so a count failure fails the whole request.
    qint64 total_count = 0;
    const std::string_view count_code = mode == proto::router::HostListRequest::MODE_ALL
        ? database.hostCount(&total_count)
        : database.hostCount(workspace_id, group_id, &total_count);
    if (count_code != proto::router::kErrorOk)
    {
        out->set_error_code(count_code);
        return;
    }

    out->set_total_count(total_count);

    if (mode == proto::router::HostListRequest::MODE_ALL)
        database.hosts(request.offset(), request.count(), out);
    else
        database.hosts(workspace_id, group_id, request.offset(), request.count(), out);

    // hosts() drops the partial list from an error reply; the count computed up front must not
    // survive it either.
    if (out->error_code() != proto::router::kErrorOk)
        out->clear_total_count();
}

//--------------------------------------------------------------------------------------------------
void handleHostSearch(Database& database, const RequestCaller& caller,
                      const proto::router::HostSearchRequest& request,
                      proto::router::HostSearchResult* out)
{
    // Search is scoped to the workspaces the caller can reach: every one of them for an
    // administrator, the ones it is a member of for anybody else.
    const bool is_admin = caller.session_type == proto::router::SESSION_TYPE_ADMIN;

    std::set<qint64> workspace_ids;
    const bool ids_known = is_admin ? database.workspaceIds(&workspace_ids)
                                    : database.workspaceAccessIdsForUser(caller.user_id, &workspace_ids);
    if (!ids_known)
    {
        LOG(ERROR) << "Failed to read the workspace scope of user" << caller.user_id;
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    database.searchHosts(request.query(), workspace_ids, request.offset(), request.count(), out);
}
