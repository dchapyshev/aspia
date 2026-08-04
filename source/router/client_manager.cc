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
#include "router/group_request_handler.h"
#include "router/host_request_handler.h"

//--------------------------------------------------------------------------------------------------
ClientManager::ClientManager(Database& database, TcpChannel* channel, QObject* parent)
    : Client(database, channel, parent)
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
    const HostRequestHandler::Result handled =
        HostRequestHandler::handle(database(), requestCaller(), request);

    proto::router::RouterToManager message;
    proto::router::HostResult* result = message.mutable_host_result();
    result->set_request_id(request.request_id());
    result->set_command_name(request.command_name());
    result->set_error_code(handled.error_code);

    sendMessage(proto::router::CHANNEL_ID_MANAGER, serialize(message));

    if (handled.notify_flags)
        emit sig_notifyChanged(handled.notify_flags);
}

//--------------------------------------------------------------------------------------------------
void ClientManager::doGroupRequest(const proto::router::GroupRequest& request)
{
    const GroupRequestHandler::Result handled =
        GroupRequestHandler::handle(database(), requestCaller(), request);

    proto::router::RouterToManager message;
    proto::router::GroupResult* result = message.mutable_group_result();
    result->set_request_id(request.request_id());
    result->set_command_name(request.command_name());
    result->set_error_code(handled.error_code);
    if (handled.entry_id > 0)
        result->set_entry_id(handled.entry_id);

    sendMessage(proto::router::CHANNEL_ID_MANAGER, serialize(message));

    if (handled.notify_flags)
        emit sig_notifyChanged(handled.notify_flags);
}
