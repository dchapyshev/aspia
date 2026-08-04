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

#include "router/host_request_handler.h"

#include "base/logging.h"
#include "base/peer/host_id.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"
#include "router/database.h"
#include "router/workers/client_worker.h"

//--------------------------------------------------------------------------------------------------
// static
HostRequestHandler::Result HostRequestHandler::handle(
    Database& database, const RequestCaller& caller, const proto::router::HostRequest& request)
{
    Result result;

    if (request.command_name() != proto::router::kCommandHostModify)
    {
        LOG(ERROR) << "Unknown host edit command:" << request.command_name();
        result.error_code = proto::router::kErrorInvalidRequest;
        return result;
    }

    const proto::router::Host& host = request.host();
    const HostId host_id = host.host_id();

    // "Not found" and "could not check" are different answers - a database error must not be
    // reported as a missing host.
    bool workspace_known = false;
    const qint64 workspace_id = database.hostWorkspaceId(host_id, &workspace_known);
    if (!workspace_known)
    {
        LOG(ERROR) << "Unable to resolve workspace of host" << host_id;
        result.error_code = proto::router::kErrorInternalError;
        return result;
    }

    if (workspace_id < 0)
    {
        LOG(ERROR) << "Host not found:" << host_id;
        result.error_code = proto::router::kErrorNotFound;
        return result;
    }

    // Hosts that are not assigned to a workspace cannot be edited from manager/admin clients.
    // Editor must also be a member of the host's workspace; admins are auto-included by design.
    // "No access" and "could not check" are different answers - a database error must not be
    // reported as a denial.
    bool access_known = false;
    const bool has_access =
        workspace_id != 0 && database.hasWorkspaceAccess(caller.user_id, workspace_id, &access_known);
    if (workspace_id != 0 && !access_known)
    {
        LOG(ERROR) << "Unable to check access to workspace" << workspace_id;
        result.error_code = proto::router::kErrorInternalError;
        return result;
    }

    if (!has_access)
    {
        LOG(ERROR) << "User" << caller.user_id << "cannot edit host" << host_id
                   << "(workspace_id=" << workspace_id << ")";
        result.error_code = proto::router::kErrorAccessDenied;
        return result;
    }

    // group_id == 0 keeps the host at the workspace root; any other value must reference a group
    // in the host's current workspace. A negative or unknown id is rejected here (the hosts table
    // has no foreign key on group_id, so an unchecked value would orphan the host). Cross-workspace
    // moves are not allowed.
    const qint64 group_id = host.group_id();
    if (group_id != 0)
    {
        bool group_known = false;
        const Group group = database.findGroup(workspace_id, group_id, &group_known);
        if (!group_known)
        {
            LOG(ERROR) << "Unable to check group" << group_id << "in workspace" << workspace_id;
            result.error_code = proto::router::kErrorInternalError;
            return result;
        }

        if (group.entry_id == 0)
        {
            LOG(ERROR) << "Group" << group_id << "not found in workspace" << workspace_id;
            result.error_code = proto::router::kErrorInvalidData;
            return result;
        }
    }

    if (!database.modifyHost(host_id, group_id, host.display_name(), host.comment(),
                             host.user_name(), host.password()))
    {
        result.error_code = proto::router::kErrorInternalError;
        return result;
    }

    result.error_code = proto::router::kErrorOk;
    result.notify_flags = ClientWorker::NOTIFY_HOSTS;
    return result;
}
