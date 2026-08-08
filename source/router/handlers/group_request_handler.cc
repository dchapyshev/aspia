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

#include "router/handlers/group_request_handler.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"
#include "router/database.h"
#include "router/handlers/workspace_request_handler.h"
#include "router/workers/client_worker.h"

//--------------------------------------------------------------------------------------------------
RequestResult handleGroupRequest(Database& database, const RequestCaller& caller,
                                 const proto::router::GroupRequest& request)
{
    RequestResult result;

    const qint64 workspace_id = request.workspace_id();
    const proto::router::Group& group = request.group();
    const qint64 entry_id = group.entry_id();
    const qint64 parent_id = group.parent_id();
    const std::string name(strTrimmed(group.name()));
    const std::string_view comment = group.comment();
    const std::string& command_name = request.command_name();

    if (workspace_id <= 0)
    {
        LOG(ERROR) << "Invalid workspace id in group request:" << workspace_id;
        result.error_code = proto::router::kErrorInvalidRequest;
        return result;
    }

    // The caller must reach the target workspace to manage its groups: be a member of it, or
    // manage every workspace of the router as an administrator.
    const std::string_view access_code = checkWorkspaceAccess(database, caller, workspace_id);
    if (access_code != proto::router::kErrorOk)
    {
        result.error_code = access_code;
        return result;
    }

    if (command_name == proto::router::kCommandGroupAdd)
    {
        LOG(INFO) << "Group add request: workspace_id=" << workspace_id
                  << "parent_id=" << parent_id << "name=" << name;

        qint64 new_id = -1;
        const std::string_view error_code =
            database.addGroup(workspace_id, parent_id, name, comment, &new_id);
        result.error_code = error_code;

        if (error_code == proto::router::kErrorOk)
        {
            result.entry_id = new_id;
            result.notify_flags = ClientWorker::NOTIFY_GROUPS;
        }
    }
    else if (command_name == proto::router::kCommandGroupModify)
    {
        LOG(INFO) << "Group modify request: workspace_id=" << workspace_id
                  << "entry_id=" << entry_id << "new_parent_id=" << parent_id
                  << "name=" << name;

        const std::string_view error_code =
            database.modifyGroup(workspace_id, entry_id, parent_id, name, comment);
        result.error_code = error_code;

        if (error_code == proto::router::kErrorOk)
            result.notify_flags = ClientWorker::NOTIFY_GROUPS;
    }
    else if (command_name == proto::router::kCommandGroupDelete)
    {
        LOG(INFO) << "Group delete request: workspace_id=" << workspace_id
                  << "entry_id=" << entry_id;

        const std::string_view error_code = database.removeGroup(workspace_id, entry_id);
        result.error_code = error_code;

        if (error_code == proto::router::kErrorOk)
        {
            // Deleting a group detaches hosts that pointed into its subtree to the workspace root,
            // so signal both lists to refresh.
            result.notify_flags = ClientWorker::NOTIFY_GROUPS | ClientWorker::NOTIFY_HOSTS;
        }
    }
    else
    {
        LOG(ERROR) << "Unknown group request command:" << command_name;
        result.error_code = proto::router::kErrorInvalidRequest;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
void handleGroupList(Database& database, const RequestCaller& caller,
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

    const std::string_view access_code = checkWorkspaceAccess(database, caller, workspace_id);
    if (access_code != proto::router::kErrorOk)
    {
        out->set_error_code(access_code);
        return;
    }

    database.groupList(workspace_id, out);
}
