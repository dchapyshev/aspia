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

#include "router/workspace_request_handler.h"

#include <set>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/peer/host_id.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "router/database.h"
#include "router/workers/client_worker.h"
#include "router/workspace.h"

namespace {

using Result = WorkspaceRequestHandler::Result;

//--------------------------------------------------------------------------------------------------
// The access list of the request. |self_present| tells whether the sender kept its own entry: an
// administrator has access to every workspace, so a list without the sender is malformed - the
// group key would be sealed for everyone but the one who has it.
QList<Workspace::Access> accessList(const proto::router::Workspace& workspace, qint64 caller_user_id,
                                    bool* self_present)
{
    *self_present = false;

    QList<Workspace::Access> access_list;
    access_list.reserve(workspace.access_size());

    for (int i = 0; i < workspace.access_size(); ++i)
    {
        const proto::router::WorkspaceAccess& src = workspace.access(i);

        Workspace::Access& dst = access_list.emplaceBack();
        dst.user_id    = src.user_id();
        dst.wrapped_gk = src.wrapped_gk();
        dst.public_key = src.public_key();

        if (dst.user_id == caller_user_id)
            *self_present = true;
    }

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
void handleAdd(Database& database, const RequestCaller& caller,
               const proto::router::Workspace& workspace, Result* result)
{
    const std::set<HostId> host_ids = desiredHostIds(workspace);

    LOG(INFO) << "Workspace add request:" << workspace.name() << "with" << workspace.access_size()
              << "access entries and" << host_ids.size() << "hosts";

    bool self_present = false;
    const QList<Workspace::Access> initial_access =
        accessList(workspace, caller.user_id, &self_present);

    if (!self_present)
    {
        LOG(ERROR) << "Admin" << caller.name << "tried to create workspace without own access";
        result->error_code = proto::router::kErrorInvalidData;
        return;
    }

    qint64 new_id = -1;
    const std::string_view error_code = database.addWorkspace(
        strTrimmed(workspace.name()), workspace.comment(), initial_access, host_ids, &new_id);
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
void handleModify(Database& database, const RequestCaller& caller,
                  const proto::router::Workspace& workspace, Result* result)
{
    const std::set<HostId> host_ids = desiredHostIds(workspace);

    LOG(INFO) << "Workspace modify request:" << workspace.entry_id() << workspace.name()
              << "with" << workspace.access_size() << "access entries and"
              << host_ids.size() << "hosts";

    bool self_present = false;
    const QList<Workspace::Access> desired_access =
        accessList(workspace, caller.user_id, &self_present);

    if (!self_present)
    {
        LOG(ERROR) << "Admin" << caller.name << "tried to revoke own access to workspace"
                   << workspace.entry_id();
        result->error_code = proto::router::kErrorInvalidData;
        return;
    }

    const std::string_view error_code = database.modifyWorkspace(
        workspace.entry_id(), workspace.revision(), strTrimmed(workspace.name()),
        workspace.comment(), desired_access, host_ids);
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
// static
WorkspaceRequestHandler::Result WorkspaceRequestHandler::handle(
    Database& database, const RequestCaller& caller,
    const proto::router::WorkspaceRequest& request)
{
    Result result;
    const std::string& command_name = request.command_name();

    if (command_name == proto::router::kCommandWorkspaceAdd)
    {
        handleAdd(database, caller, request.workspace(), &result);
    }
    else if (command_name == proto::router::kCommandWorkspaceModify)
    {
        handleModify(database, caller, request.workspace(), &result);
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
