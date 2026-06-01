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

#include "router/client_manager.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "proto/router.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"
#include "router/database.h"
#include "router/service.h"

//--------------------------------------------------------------------------------------------------
ClientManager::ClientManager(TcpChannel* channel, QObject* parent)
    : Client(channel, parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientManager::~ClientManager()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientManager::onSessionMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::router::CHANNEL_ID_CLIENT)
    {
        Client::onSessionMessage(channel_id, buffer);
        return;
    }

    if (channel_id != proto::router::CHANNEL_ID_MANAGER)
        return;

    if (!isTwoFactorCompleted())
    {
        CLOG(ERROR) << "Manager message before 2FA completion";
        return;
    }

    proto::router::ManagerToRouter message;
    if (!parse(buffer, &message))
    {
        CLOG(ERROR) << "Could not read message from manager";
        return;
    }

    if (message.has_host_request())
        doHostRequest(message.host_request());
    else if (message.has_group_request())
        doGroupRequest(message.group_request());
    else
        CLOG(ERROR) << "Unhandled message from manager";
}

//--------------------------------------------------------------------------------------------------
void ClientManager::doHostRequest(const proto::router::HostRequest& request)
{
    proto::router::RouterToManager response;
    proto::router::HostResult* result = response.mutable_host_result();
    result->set_request_id(request.request_id());
    result->set_command_name(request.command_name());

    auto reply = [&](const char* error_code)
    {
        result->set_error_code(error_code);
        sendMessage(proto::router::CHANNEL_ID_MANAGER, serialize(response));
    };

    if (request.command_name() != proto::router::kCommandHostModify)
    {
        CLOG(ERROR) << "Unknown host edit command:" << request.command_name();
        reply(proto::router::kErrorInvalidRequest);
        return;
    }

    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        reply(proto::router::kErrorInternalError);
        return;
    }

    const proto::router::Host& host = request.host();
    const HostId host_id = host.host_id();
    const qint64 workspace_id = database.hostWorkspaceId(host_id);
    if (workspace_id < 0)
    {
        CLOG(ERROR) << "Host not found:" << host_id;
        reply(proto::router::kErrorNotFound);
        return;
    }

    // Hosts that are not assigned to a workspace cannot be edited from manager/admin clients.
    // Editor must also be a member of the host's workspace; admins are auto-included by design.
    if (workspace_id == 0 || !database.hasWorkspaceAccess(userId(), workspace_id))
    {
        CLOG(ERROR) << "User" << userId() << "cannot edit host" << host_id
                    << "(workspace_id=" << workspace_id << ")";
        reply(proto::router::kErrorAccessDenied);
        return;
    }

    // group_id == 0 keeps the host at the workspace root; > 0 must reference a group in the
    // host's current workspace. Cross-workspace moves are not allowed.
    const qint64 group_id = host.group_id();
    if (group_id > 0 && database.findGroup(workspace_id, group_id).entry_id == 0)
    {
        CLOG(ERROR) << "Group" << group_id << "not found in workspace" << workspace_id;
        reply(proto::router::kErrorInvalidData);
        return;
    }

    const bool ok = database.modifyHost(
        host_id,
        group_id,
        QString::fromStdString(host.display_name()),
        QByteArray::fromStdString(host.comment()),
        QByteArray::fromStdString(host.user_name()),
        QByteArray::fromStdString(host.password()));
    if (!ok)
    {
        reply(proto::router::kErrorInternalError);
        return;
    }

    reply(proto::router::kErrorOk);
    Service::instance()->notifyChanged(Service::NOTIFY_HOSTS);
}

//--------------------------------------------------------------------------------------------------
void ClientManager::doGroupRequest(const proto::router::GroupRequest& request)
{
    proto::router::RouterToManager message;
    proto::router::GroupResult* result = message.mutable_group_result();
    result->set_request_id(request.request_id());
    result->set_command_name(request.command_name());

    const qint64 workspace_id = request.workspace_id();
    const proto::router::Group& group = request.group();
    const qint64 entry_id = group.entry_id();
    const qint64 parent_id = group.parent_id();
    const QString name = QString::fromStdString(group.name()).trimmed();
    const QByteArray comment = QByteArray::fromStdString(group.comment());

    if (workspace_id <= 0)
    {
        CLOG(ERROR) << "Invalid workspace id in group request:" << workspace_id;
        result->set_error_code(proto::router::kErrorInvalidRequest);
        sendMessage(proto::router::CHANNEL_ID_MANAGER, serialize(message));
        return;
    }

    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        result->set_error_code(proto::router::kErrorInternalError);
        sendMessage(proto::router::CHANNEL_ID_MANAGER, serialize(message));
        return;
    }

    // Caller must be a member of the target workspace to manage its groups. Matches the
    // workspace-access semantics used elsewhere; non-members do not see the workspace's
    // wrapped_gk and cannot meaningfully add or edit AEAD-encrypted group fields anyway.
    if (!database.hasWorkspaceAccess(userId(), workspace_id))
    {
        CLOG(ERROR) << "User" << userId() << "has no access to workspace" << workspace_id;
        result->set_error_code(proto::router::kErrorAccessDenied);
        sendMessage(proto::router::CHANNEL_ID_MANAGER, serialize(message));
        return;
    }

    if (request.command_name() == proto::router::kCommandGroupAdd)
    {
        CLOG(INFO) << "Group add request: workspace_id=" << workspace_id
                   << "parent_id=" << parent_id << "name=" << name;

        qint64 new_id = -1;
        const std::string_view error_code =
            database.addGroup(workspace_id, parent_id, name, comment, &new_id);
        result->set_error_code(error_code);
        if (error_code == proto::router::kErrorOk)
        {
            result->set_entry_id(new_id);
            Service::instance()->notifyChanged(Service::NOTIFY_GROUPS);
        }
    }
    else if (request.command_name() == proto::router::kCommandGroupModify)
    {
        CLOG(INFO) << "Group modify request: workspace_id=" << workspace_id
                   << "entry_id=" << entry_id << "new_parent_id=" << parent_id
                   << "name=" << name;

        const std::string_view error_code =
            database.modifyGroup(workspace_id, entry_id, parent_id, name, comment);
        result->set_error_code(error_code);
        if (error_code == proto::router::kErrorOk)
            Service::instance()->notifyChanged(Service::NOTIFY_GROUPS);
    }
    else if (request.command_name() == proto::router::kCommandGroupDelete)
    {
        CLOG(INFO) << "Group delete request: workspace_id=" << workspace_id
                   << "entry_id=" << entry_id;

        const std::string_view error_code = database.removeGroup(workspace_id, entry_id);
        result->set_error_code(error_code);
        if (error_code == proto::router::kErrorOk)
        {
            // Deleting a group detaches hosts that pointed into its subtree to the workspace
            // root, so signal both lists to refresh.
            Service::instance()->notifyChanged(Service::NOTIFY_GROUPS | Service::NOTIFY_HOSTS);
        }
    }
    else
    {
        CLOG(ERROR) << "Unknown group request command:" << request.command_name();
        result->set_error_code(proto::router::kErrorInvalidRequest);
    }

    sendMessage(proto::router::CHANNEL_ID_MANAGER, serialize(message));
}
