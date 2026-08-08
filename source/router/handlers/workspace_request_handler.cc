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

#include "router/handlers/workspace_request_handler.h"

#include <set>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/peer/host_id.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "router/database.h"
#include "router/workspace.h"
#include "router/workers/client_worker.h"

namespace {

using Result = RequestResult;

//--------------------------------------------------------------------------------------------------
std::vector<Workspace::Access> accessList(const proto::router::Workspace& workspace)
{
    std::vector<Workspace::Access> access_list;
    access_list.reserve(size_t(workspace.access_size()));

    for (int i = 0; i < workspace.access_size(); ++i)
        access_list.emplace_back().user_id = workspace.access(i).user_id();

    return access_list;
}

//--------------------------------------------------------------------------------------------------
std::set<HostId> desiredHostIds(const proto::router::Workspace& workspace)
{
    std::set<HostId> host_ids;
    for (int i = 0; i < workspace.host_id_size(); ++i)
        host_ids.insert(workspace.host_id(i));
    return host_ids;
}

//--------------------------------------------------------------------------------------------------
void handleAdd(Database& database, const proto::router::Workspace& workspace, Result* result)
{
    const std::set<HostId> host_ids = desiredHostIds(workspace);

    LOG(INFO) << "Workspace add request:" << workspace.name() << "with" << workspace.access_size()
              << "access entries and" << host_ids.size() << "hosts";

    qint64 new_id = -1;
    const std::string_view error_code = database.addWorkspace(
        strTrimmed(workspace.name()), workspace.comment(), accessList(workspace), host_ids, &new_id);
    result->error_code = error_code;

    if (error_code != proto::router::kErrorOk)
        return;

    result->entry_id = new_id;
    result->notify_flags = ClientWorker::NOTIFY_WORKSPACES;

    // A creation can only claim hosts, so the host lists are stale only when it did.
    if (!host_ids.empty())
        result->notify_flags |= ClientWorker::NOTIFY_HOSTS;
}

//--------------------------------------------------------------------------------------------------
void handleModify(Database& database, const proto::router::Workspace& workspace, Result* result)
{
    const std::set<HostId> host_ids = desiredHostIds(workspace);

    LOG(INFO) << "Workspace modify request:" << workspace.entry_id() << workspace.name()
              << "with" << workspace.access_size() << "access entries and"
              << host_ids.size() << "hosts";

    const std::string_view error_code = database.modifyWorkspace(
        workspace.entry_id(), workspace.revision(), strTrimmed(workspace.name()),
        workspace.comment(), accessList(workspace), host_ids);
    result->error_code = error_code;

    if (error_code != proto::router::kErrorOk)
        return;

    // The host assignments can change even when the desired set is empty (all the hosts of the
    // workspace released), so the hosts are refetched in any case.
    result->notify_flags = ClientWorker::NOTIFY_WORKSPACES | ClientWorker::NOTIFY_HOSTS;
}

//--------------------------------------------------------------------------------------------------
void handleDelete(Database& database, qint64 entry_id, Result* result)
{
    LOG(INFO) << "Workspace delete request:" << entry_id;

    const std::string_view error_code = database.removeWorkspace(entry_id);
    result->error_code = error_code;

    if (error_code != proto::router::kErrorOk)
        return;

    // Deleting a workspace also releases its hosts to workspace_id=0 and takes its group tree with
    // it (the cascade of host_groups.workspace_id), so all three lists the clients cache are stale.
    result->notify_flags = ClientWorker::NOTIFY_WORKSPACES | ClientWorker::NOTIFY_HOSTS |
                           ClientWorker::NOTIFY_GROUPS;
}

} // namespace

//--------------------------------------------------------------------------------------------------
std::string_view checkWorkspaceAccess(Database& database, const RequestCaller& caller,
                                      qint64 workspace_id)
{
    // An administrator manages every workspace of the router, membership or not.
    if (caller.session_type == proto::router::SESSION_TYPE_ADMIN)
        return proto::router::kErrorOk;

    bool access_known = false;
    const bool has_access = database.hasWorkspaceAccess(caller.user_id, workspace_id, &access_known);
    if (!access_known)
    {
        LOG(ERROR) << "Unable to check access to workspace" << workspace_id;
        return proto::router::kErrorInternalError;
    }

    if (!has_access)
    {
        LOG(ERROR) << "User" << caller.user_id << "has no access to workspace" << workspace_id;
        return proto::router::kErrorAccessDenied;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
RequestResult handleWorkspaceRequest(Database& database, const RequestCaller& caller,
                                     const proto::router::WorkspaceRequest& request)
{
    RequestResult result;
    const std::string& command_name = request.command_name();

    if (command_name == proto::router::kCommandWorkspaceAdd)
    {
        handleAdd(database, request.workspace(), &result);
    }
    else if (command_name == proto::router::kCommandWorkspaceModify)
    {
        handleModify(database, request.workspace(), &result);
    }
    else if (command_name == proto::router::kCommandWorkspaceDelete)
    {
        handleDelete(database, request.workspace().entry_id(), &result);
    }
    else
    {
        LOG(ERROR) << "Unknown workspace request command:" << command_name;
        result.error_code = proto::router::kErrorInvalidRequest;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
void handleWorkspaceList(Database& database, const RequestCaller& caller,
                         const proto::router::WorkspaceListRequest& request,
                         proto::router::WorkspaceList* out)
{
    // An administrator manages every workspace of the router and needs the membership of each;
    // any other session sees the workspaces it is a member of, and the membership of a workspace
    // is not theirs to see. workspace_id == 0 means every workspace of that scope; > 0 narrows to
    // a single entry.
    if (caller.session_type == proto::router::SESSION_TYPE_ADMIN)
        database.workspaceListForAdmin(request.workspace_id(), out);
    else
        database.workspaceListForUser(caller.user_id, request.workspace_id(), out);
}
