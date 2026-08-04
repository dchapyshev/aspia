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

#include "router/client_admin.h"

#include "base/core_application.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "router/database.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "proto/router_host.h"
#include "router/client.h"
#include "router/user_request_handler.h"
#include "router/workspace_request_handler.h"
#include "router/workers/client_worker.h"
#include "router/workers/host_worker.h"
#include "router/workers/relay_worker.h"

//--------------------------------------------------------------------------------------------------
ClientAdmin::ClientAdmin(TcpChannel* channel, QObject* parent)
    : ClientManager(channel, parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientAdmin::~ClientAdmin()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::onSessionMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::router::CHANNEL_ID_ADMIN)
    {
        ClientManager::onSessionMessage(channel_id, buffer);
        return;
    }

    if (!isTwoFactorCompleted())
    {
        CLOG(ERROR) << "Admin message before 2FA completion";
        return;
    }

    proto::router::AdminToRouter message;
    if (!parse(buffer, &message))
    {
        CLOG(ERROR) << "Could not read message from manager";
        return;
    }

    if (message.has_relay_list_request())
        doRelayListRequest(message.relay_list_request());
    else if (message.has_host_request())
        doHostRequest(message.host_request());
    else if (message.has_relay_request())
        doRelayRequest(message.relay_request());
    else if (message.has_client_list_request())
        emit sig_clientListRequest(message.client_list_request());
    else if (message.has_client_request())
        emit sig_clientRequest(message.client_request());
    else if (message.has_user_list_request())
        doUserListRequest(message.user_list_request());
    else if (message.has_user_request())
        doUserRequest(message.user_request());
    else if (message.has_peer_request())
        doPeerRequest(message.peer_request());
    else if (message.has_workspace_request())
        doWorkspaceRequest(message.workspace_request());
    else
        CLOG(ERROR) << "Unhandled message from manager";
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::doRelayListRequest(const proto::router::RelayListRequest& request)
{
    RelayWorker* relay_worker = CoreApplication::findWorker<RelayWorker>();
    CHECK(relay_worker);

    const auto request_id = request.request_id();

    relay_worker->requestRelayList(this, [this, request_id](proto::router::RelayList&& relay_list)
    {
        relay_list.set_request_id(request_id);
        relay_list.set_error_code(proto::router::kErrorOk);

        proto::router::RouterToAdmin message;
        message.mutable_relay_list()->Swap(&relay_list);

        sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
    });
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::doUserListRequest(const proto::router::UserListRequest& request)
{
    proto::router::RouterToAdmin message;
    proto::router::UserList* list = message.mutable_user_list();
    list->set_request_id(request.request_id());

    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        list->set_error_code(proto::router::kErrorInternalError);
    }
    else
    {
        QList<RouterUser> users;
        if (!database.userList(&users))
        {
            list->set_error_code(proto::router::kErrorInternalError);
        }
        else
        {
            list->set_error_code(proto::router::kErrorOk);

            for (const auto& user : std::as_const(users))
            {
                proto::router::User* item = list->add_user();
                item->CopyFrom(user.serialize());

                // |otp_active| is a presentation-only flag derived from whether the user has a
                // confirmed TOTP secret on file.
                item->set_otp_active(!user.otp_secret.isEmpty());

                // Attach the user's active device tokens. The router only ever exposes the opaque
                // numeric id and timestamp metadata - never the token hash or any other material
                // that could identify the token outside of the router.
                std::vector<DeviceToken> tokens;
                if (!database.listClientDeviceTokens(user.entry_id, &tokens))
                {
                    // A partially built reply must not pass for a complete one.
                    list->clear_user();
                    list->set_error_code(proto::router::kErrorInternalError);
                    break;
                }

                for (DeviceToken& src : tokens)
                {
                    proto::router::User::Token* token = item->add_token();
                    token->set_token_id(src.token_id);
                    token->set_created_at(src.created_at);
                    token->set_last_used_at(src.last_used_at);
                    token->set_address(std::move(src.address));
                }
            }
        }
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::doUserRequest(const proto::router::UserRequest& request)
{
    const UserRequestHandler::Result handled =
        UserRequestHandler::handle(Database::instance(), requestCaller(), request);

    proto::router::RouterToAdmin message;
    proto::router::UserResult* result = message.mutable_user_result();
    result->set_request_id(request.request_id());
    result->set_command_name(request.command_name());
    result->set_error_code(handled.error_code);

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));

    if (handled.notify_flags)
        emit sig_notifyChanged(handled.notify_flags);

    // After the reply: the sessions being stopped can include the one that sent the request (an
    // administrator disabling its own account), and it must still see the result of its command.
    if (handled.stop_user_id > 0)
        emit sig_stopClients(handled.stop_user_id, handled.stop_token_ids, 0);
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::doHostRequest(const proto::router::HostRequest& request)
{
    HostWorker* host_worker = CoreApplication::findWorker<HostWorker>();
    CHECK(host_worker);

    const auto request_id = request.request_id();
    const std::string command_name = request.command_name();
    const HostId host_id = request.host().host_id();

    auto send_result = [this, request_id, command_name](std::string_view error_code)
    {
        proto::router::RouterToAdmin message;
        proto::router::HostResult* host_result = message.mutable_host_result();
        host_result->set_request_id(request_id);
        host_result->set_command_name(command_name);
        host_result->set_error_code(error_code);
        sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
    };

    if (command_name == proto::router::kCommandHostDisconnect)
    {
        host_worker->disconnectHost(host_id, this, [this, host_id, send_result](bool result)
        {
            if (host_id == kAllHostsId)
            {
                CLOG(INFO) << "All host sessions disconnected by" << userName();
                send_result(proto::router::kErrorOk);
            }
            else if (!result)
            {
                CLOG(ERROR) << "No live session for host_id:" << host_id;
                send_result(proto::router::kErrorInvalidEntryId);
            }
            else
            {
                CLOG(INFO) << "Host" << host_id << "disconnected by" << userName();
                send_result(proto::router::kErrorOk);
            }
        });
    }
    else if (command_name == proto::router::kCommandHostRemove)
    {
        host_worker->removeHost(host_id, this,
            [this, host_id, send_result](HostWorker::RemoveHostResult&& result)
        {
            if (result.scheduled)
            {
                CLOG(INFO) << "Host" << host_id << "removal scheduled by" << userName()
                           << "(online:" << result.online << ")";
            }

            send_result(result.error_code);
        });
    }
    else if (command_name == proto::router::kCommandHostUpdate)
    {
        host_worker->updateHost(host_id, this, [this, host_id, send_result](bool result)
        {
            if (!result)
            {
                CLOG(ERROR) << "No live session for host_id:" << host_id;
                send_result(proto::router::kErrorInvalidEntryId);
            }
            else
            {
                CLOG(INFO) << "Host" << host_id << "update check requested by" << userName();
                send_result(proto::router::kErrorOk);
            }
        });
    }
    else if (command_name == proto::router::kCommandHostApprove)
    {
        if (!isTempHostId(host_id))
        {
            CLOG(ERROR) << "No live temporary session for host_id:" << host_id;
            send_result(proto::router::kErrorInvalidEntryId);
            return;
        }

        host_worker->approveHost(host_id, this, [this, host_id, send_result](std::string_view error_code)
        {
            if (error_code == proto::router::kErrorInvalidEntryId)
                CLOG(ERROR) << "No live temporary session for host_id:" << host_id;
            else if (error_code == proto::router::kErrorOk)
                CLOG(INFO) << "Temporary host" << host_id << "approved by" << userName();

            send_result(error_code);
        });
    }
    else
    {
        CLOG(ERROR) << "Unknown host request command:" << command_name;
        send_result(proto::router::kErrorInvalidRequest);
    }
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::doRelayRequest(const proto::router::RelayRequest& request)
{
    const auto request_id = request.request_id();
    const std::string command_name = request.command_name();

    if (command_name != proto::router::kCommandRelayDisconnect)
    {
        CLOG(ERROR) << "Unknown relay request command:" << command_name;

        proto::router::RouterToAdmin message;
        proto::router::RelayResult* relay_result = message.mutable_relay_result();
        relay_result->set_request_id(request_id);
        relay_result->set_command_name(command_name);
        relay_result->set_error_code(proto::router::kErrorInvalidRequest);
        sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
        return;
    }

    RelayWorker* relay_worker = CoreApplication::findWorker<RelayWorker>();
    CHECK(relay_worker);

    const qint64 entry_id = request.entry_id();

    relay_worker->stopRelay(entry_id, this, [this, request_id, command_name, entry_id](bool result)
    {
        proto::router::RouterToAdmin message;
        proto::router::RelayResult* relay_result = message.mutable_relay_result();
        relay_result->set_request_id(request_id);
        relay_result->set_command_name(command_name);

        if (entry_id == -1)
        {
            if (result)
            {
                CLOG(INFO) << "All relay sessions disconnected by" << userName();
                relay_result->set_error_code(proto::router::kErrorOk);
            }
            else
            {
                relay_result->set_error_code(proto::router::kErrorInternalError);
            }
        }
        else if (!result)
        {
            CLOG(ERROR) << "Session not found:" << entry_id;
            relay_result->set_error_code(proto::router::kErrorInvalidEntryId);
        }
        else
        {
            CLOG(INFO) << "Relay session '" << entry_id << "' disconnected by" << userName();
            relay_result->set_error_code(proto::router::kErrorOk);
        }

        sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
    });
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::doPeerRequest(const proto::router::PeerRequest& request)
{
    RelayWorker* relay_worker = CoreApplication::findWorker<RelayWorker>();
    CHECK(relay_worker);

    const auto request_id = request.request_id();
    const std::string command_name = request.command_name();
    const qint64 relay_id = request.relay_id();

    relay_worker->disconnectPeerSession(relay_id, request, this,
        [this, request_id, command_name, relay_id](bool result)
    {
        proto::router::RouterToAdmin message;
        proto::router::PeerResult* peer_result = message.mutable_peer_result();
        peer_result->set_request_id(request_id);
        peer_result->set_command_name(command_name);

        if (!result)
        {
            CLOG(ERROR) << "Relay with id" << relay_id << "is not found";
            peer_result->set_error_code(proto::router::kErrorNotFound);
        }
        else
        {
            peer_result->set_error_code(proto::router::kErrorOk);
        }

        sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
    });
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::doWorkspaceRequest(const proto::router::WorkspaceRequest& request)
{
    const WorkspaceRequestHandler::Result handled =
        WorkspaceRequestHandler::handle(Database::instance(), requestCaller(), request);

    proto::router::RouterToAdmin message;
    proto::router::WorkspaceResult* result = message.mutable_workspace_result();
    result->set_request_id(request.request_id());
    result->set_command_name(request.command_name());
    result->set_error_code(handled.error_code);
    if (handled.entry_id > 0)
        result->set_entry_id(handled.entry_id);

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));

    if (handled.notify_flags)
        emit sig_notifyChanged(handled.notify_flags);
}
