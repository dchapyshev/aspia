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

#include "router/session_manager.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "proto/router.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"
#include "router/database.h"
#include "router/service.h"

//--------------------------------------------------------------------------------------------------
SessionManager::SessionManager(TcpChannel* channel, QObject* parent)
    : SessionClient(channel, parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
SessionManager::~SessionManager()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SessionManager::onSessionMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::router::CHANNEL_ID_CLIENT)
    {
        SessionClient::onSessionMessage(channel_id, buffer);
        return;
    }

    if (channel_id != proto::router::CHANNEL_ID_MANAGER)
        return;

    proto::router::ManagerToRouter message;
    if (!parse(buffer, &message))
    {
        CLOG(ERROR) << "Could not read message from manager";
        return;
    }

    if (message.has_host_edit_request())
        doHostEditRequest(message.host_edit_request());
    else
        CLOG(ERROR) << "Unhandled message from manager";
}

//--------------------------------------------------------------------------------------------------
void SessionManager::doHostEditRequest(const proto::router::HostEditRequest& request)
{
    proto::router::RouterToManager response;
    proto::router::HostEditResult* result = response.mutable_host_edit_result();
    result->set_request_id(request.request_id());

    auto reply = [&](const char* error_code)
    {
        result->set_error_code(error_code);
        sendMessage(proto::router::CHANNEL_ID_MANAGER, serialize(response));
    };

    Database database = Database::open();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        reply(proto::router::kErrorInternalError);
        return;
    }

    const HostId host_id = request.host_id();
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

    const bool ok = database.modifyHost(
        host_id,
        QString::fromStdString(request.display_name()),
        QByteArray::fromStdString(request.comment()),
        QByteArray::fromStdString(request.user_name()),
        QByteArray::fromStdString(request.password()));
    if (!ok)
    {
        reply(proto::router::kErrorInternalError);
        return;
    }

    reply(proto::router::kErrorOk);
    Service::instance()->notifyChanged(Service::NOTIFY_HOSTS);
}
