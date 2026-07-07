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

#include <set>
#include <unordered_map>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/string_util.h"
#include "base/peer/router_user.h"
#include "router/database.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "proto/router_host.h"
#include "router/client.h"
#include "router/relay.h"
#include "router/service.h"
#include "router/host_legacy.h"
#include "router/host_ng.h"

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
        doClientListRequest(message.client_list_request());
    else if (message.has_client_request())
        doClientRequest(message.client_request());
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
    const QList<Relay*>& relays = Service::instance()->relays();

    proto::router::RouterToAdmin message;
    proto::router::RelayList* result = message.mutable_relay_list();
    result->set_request_id(request.request_id());
    result->set_error_code(proto::router::kErrorOk);

    for (const auto& relay : relays)
    {
        proto::router::RelayInfo* item = result->add_relay();

        // Generic session info.
        item->set_entry_id(relay->sessionId());
        item->set_timepoint(relay->startTime());
        item->set_ip_address(relay->address());
        item->mutable_version()->CopyFrom(serialize(relay->version()));
        item->set_os_name(relay->osName());
        item->set_computer_name(relay->computerName());
        item->set_architecture(relay->architecture());

        // Statistics info.
        const std::optional<proto::router::RelayStatistics>& statistics = relay->statistics();
        if (statistics.has_value())
        {
            item->mutable_statistics()->mutable_peer()->CopyFrom(statistics->peer());
            item->mutable_statistics()->set_uptime(statistics->uptime());
        }

        // Other info.
        item->set_pool_size(Service::instance()->keyCountForRelay(relay->sessionId()));
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::doClientListRequest(const proto::router::ClientListRequest& request)
{
    const QList<Client*>& clients = Service::instance()->clients();

    proto::router::RouterToAdmin message;
    proto::router::ClientList* result = message.mutable_client_list();
    result->set_request_id(request.request_id());
    result->set_error_code(proto::router::kErrorOk);

    for (const auto& client : clients)
    {
        proto::router::ClientInfo* item = result->add_client();

        item->set_entry_id(client->sessionId());
        item->set_timepoint(client->startTime());
        item->set_ip_address(client->address());
        item->mutable_version()->CopyFrom(serialize(client->version()));
        item->set_os_name(client->osName());
        item->set_computer_name(client->computerName());
        item->set_architecture(client->architecture());
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
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
        list->set_error_code(proto::router::kErrorOk);

        QList<RouterUser> users = database.userList();
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
            std::vector<DeviceToken> tokens = database.listClientDeviceTokens(user.entry_id);
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

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::doUserRequest(const proto::router::UserRequest& request)
{
    proto::router::RouterToAdmin message;
    proto::router::UserResult* result = message.mutable_user_result();
    result->set_request_id(request.request_id());
    result->set_command_name(request.command_name());
    qint64 reset_otp_user_id = 0;
    qint64 revoked_tokens_user_id = 0;
    qint64 deleted_user_id = 0;

    if (request.command_name() == proto::router::kCommandUserAdd)
    {
        result->set_error_code(addUser(request.user()));
    }
    else if (request.command_name() == proto::router::kCommandUserModify)
    {
        result->set_error_code(modifyUser(request.user()));
    }
    else if (request.command_name() == proto::router::kCommandUserDelete)
    {
        const qint64 user_id = request.user().entry_id();
        const std::string error_code = deleteUser(request.user());
        result->set_error_code(error_code);
        if (error_code == proto::router::kErrorOk)
            deleted_user_id = user_id;
    }
    else if (request.command_name() == proto::router::kCommandUserResetOtp)
    {
        const qint64 user_id = request.user().entry_id();

        if (user_id <= 0)
        {
            CLOG(ERROR) << "Invalid reset_otp request: user_id=" << user_id;
            result->set_error_code(proto::router::kErrorInvalidRequest);
        }
        else
        {
            Database& database = Database::instance();
            if (!database.isValid())
            {
                CLOG(ERROR) << "Failed to connect to database";
                result->set_error_code(proto::router::kErrorInternalError);
            }
            else if (std::string_view error_code = database.clearUserOtp(user_id);
                     error_code != proto::router::kErrorOk)
            {
                result->set_error_code(error_code);
            }
            else
            {
                reset_otp_user_id = user_id;

                // Re-enrollment implies a new device key pair; existing device tokens must die
                // with the secret they were issued against.
                const std::string_view revoke_code = database.revokeUserClientDeviceTokens(user_id);
                if (revoke_code != proto::router::kErrorOk)
                {
                    CLOG(WARNING) << "OTP cleared but failed to revoke device tokens for user"
                                  << user_id << ":" << revoke_code;
                    result->set_error_code(revoke_code);
                }
                else
                {
                    CLOG(INFO) << "OTP cleared for user" << user_id << "by" << userName();
                    result->set_error_code(proto::router::kErrorOk);
                }
                Service::instance()->notifyChanged(Service::NOTIFY_USERS);
            }
        }
    }
    else if (request.command_name() == proto::router::kCommandUserRevokeTokens)
    {
        const qint64 user_id = request.user().entry_id();

        if (user_id <= 0)
        {
            CLOG(ERROR) << "Invalid revoke_tokens request: user_id=" << user_id;
            result->set_error_code(proto::router::kErrorInvalidRequest);
        }
        else
        {
            Database& database = Database::instance();
            if (!database.isValid())
            {
                CLOG(ERROR) << "Failed to connect to database";
                result->set_error_code(proto::router::kErrorInternalError);
            }
            else if (request.user().token_size() == 0)
            {
                // Empty list - drop every token of the user atomically.
                const std::string_view error_code = database.revokeUserClientDeviceTokens(user_id);
                if (error_code != proto::router::kErrorOk)
                {
                    result->set_error_code(error_code);
                }
                else
                {
                    revoked_tokens_user_id = user_id;
                    CLOG(INFO) << "All device tokens of user" << user_id
                               << "revoked by" << userName();
                    result->set_error_code(proto::router::kErrorOk);
                    Service::instance()->notifyChanged(Service::NOTIFY_USERS);
                }
            }
            else
            {
                std::string_view code = proto::router::kErrorOk;
                QList<qint64> revoked_token_ids;

                for (int i = 0; i < request.user().token_size(); ++i)
                {
                    const qint64 token_id = request.user().token(i).token_id();
                    if (token_id <= 0)
                    {
                        CLOG(ERROR) << "Invalid token_id in revoke_tokens request";
                        code = proto::router::kErrorInvalidRequest;
                        break;
                    }
                    if (!database.revokeClientDeviceToken(user_id, token_id))
                    {
                        CLOG(WARNING) << "Device token not found: user_id=" << user_id
                                      << "token_id=" << token_id;
                        code = proto::router::kErrorNotFound;
                        break;
                    }
                    revoked_token_ids.append(token_id);
                    CLOG(INFO) << "Device token" << token_id << "of user" << user_id
                               << "revoked by" << userName();
                }

                if (!revoked_token_ids.isEmpty())
                {
                    Service::instance()->stopClients(user_id, revoked_token_ids);
                    Service::instance()->notifyChanged(Service::NOTIFY_USERS);
                }
                result->set_error_code(code);
            }
        }
    }
    else
    {
        CLOG(ERROR) << "Unknown user request command:" << request.command_name();
        result->set_error_code(proto::router::kErrorInvalidRequest);
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));

    if (reset_otp_user_id > 0)
        Service::instance()->stopClients(reset_otp_user_id);

    if (revoked_tokens_user_id > 0)
        Service::instance()->stopClients(revoked_tokens_user_id);

    if (deleted_user_id > 0)
        Service::instance()->stopClients(deleted_user_id);
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::doHostRequest(const proto::router::HostRequest& request)
{
    auto find_host = [](HostId host_id) -> Host*
    {
        const QList<Host*>& hosts = Service::instance()->hosts();
        for (Host* host : std::as_const(hosts))
        {
            HostNG* host_ng = dynamic_cast<HostNG*>(host);
            if (host_ng && host_ng->hostId() == host_id)
                return host_ng;

            HostLegacy* host_legacy = dynamic_cast<HostLegacy*>(host);
            if (host_legacy && host_legacy->hasHostId(host_id))
                return host_legacy;
        }
        return nullptr;
    };

    proto::router::RouterToAdmin message;
    proto::router::HostResult* host_result = message.mutable_host_result();
    host_result->set_request_id(request.request_id());
    host_result->set_command_name(request.command_name());

    if (request.command_name() == proto::router::kCommandHostDisconnect)
    {
        const HostId host_id = request.host().host_id();

        if (host_id == kAllHostsId)
        {
            const QList<Host*>& hosts = Service::instance()->hosts();
            QList<qint64> host_ids;

            for (const auto& host : hosts)
                host_ids.append(host->sessionId());

            bool all_ok = true;
            for (qint64 id : host_ids)
            {
                if (!Service::instance()->stopHost(id))
                {
                    CLOG(ERROR) << "Failed to stop host session:" << id;
                    all_ok = false;
                }
            }

            if (all_ok)
            {
                CLOG(INFO) << "All host sessions disconnected by" << userName();
                host_result->set_error_code(proto::router::kErrorOk);
            }
            else
            {
                host_result->set_error_code(proto::router::kErrorInternalError);
            }
        }
        else
        {
            Host* host = find_host(host_id);
            if (!host)
            {
                CLOG(ERROR) << "No live session for host_id:" << host_id;
                host_result->set_error_code(proto::router::kErrorInvalidEntryId);
            }
            else if (!Service::instance()->stopHost(host->sessionId()))
            {
                CLOG(ERROR) << "Failed to stop session for host_id:" << host_id;
                host_result->set_error_code(proto::router::kErrorInternalError);
            }
            else
            {
                CLOG(INFO) << "Host" << host_id << "disconnected by" << userName();
                host_result->set_error_code(proto::router::kErrorOk);
            }
        }
    }
    else if (request.command_name() == proto::router::kCommandHostRemove)
    {
        const HostId host_id = request.host().host_id();

        Database& database = Database::instance();
        if (!database.isValid())
        {
            CLOG(ERROR) << "Failed to connect to database";
            host_result->set_error_code(proto::router::kErrorInternalError);
        }
        else if (!database.scheduleHostRemoval(host_id))
        {
            CLOG(ERROR) << "Failed to schedule host removal for host_id:" << host_id;
            host_result->set_error_code(proto::router::kErrorInternalError);
        }
        else
        {
            // The hosts row is now in hosts_remove; if the host is online send it the remove
            // command and let HostNG finalize the hosts_remove row on disconnect. Legacy hosts
            // have no router->host remove command, so remove the id from the live legacy session
            // and finalize the pending row immediately. Offline legacy hosts are handled on the
            // next HostIdRequest.
            Host* host = find_host(host_id);
            std::string_view error_code = proto::router::kErrorOk;
            if (HostNG* host_ng = dynamic_cast<HostNG*>(host))
            {
                host_ng->sendRemoveCommand();
            }
            else if (HostLegacy* host_legacy = dynamic_cast<HostLegacy*>(host))
            {
                if (host_legacy->removeHostId(host_id))
                {
                    if (!database.finalizeHostRemoval(host_id))
                    {
                        CLOG(ERROR) << "Failed to finalize removal for legacy host_id:" << host_id;
                        error_code = proto::router::kErrorInternalError;
                    }

                    if (host_legacy->hostIdList().isEmpty())
                        Service::instance()->stopHost(host_legacy->sessionId());
                }
            }

            CLOG(INFO) << "Host" << host_id << "removal scheduled by" << userName()
                       << "(online:" << (host != nullptr) << ")";
            host_result->set_error_code(error_code);
            Service::instance()->notifyChanged(Service::NOTIFY_HOSTS);
        }
    }
    else if (request.command_name() == proto::router::kCommandHostUpdate)
    {
        const HostId host_id = request.host().host_id();

        HostNG* host = dynamic_cast<HostNG*>(find_host(host_id));
        if (!host)
        {
            CLOG(ERROR) << "No live session for host_id:" << host_id;
            host_result->set_error_code(proto::router::kErrorInvalidEntryId);
        }
        else
        {
            host->sendUpdateCommand();

            CLOG(INFO) << "Host" << host_id << "update check requested by" << userName();
            host_result->set_error_code(proto::router::kErrorOk);
        }
    }
    else
    {
        CLOG(ERROR) << "Unknown host request command:" << request.command_name();
        host_result->set_error_code(proto::router::kErrorInvalidRequest);
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::doRelayRequest(const proto::router::RelayRequest& request)
{
    proto::router::RouterToAdmin message;
    proto::router::RelayResult* relay_result = message.mutable_relay_result();
    relay_result->set_request_id(request.request_id());
    relay_result->set_command_name(request.command_name());

    if (request.command_name() == proto::router::kCommandRelayDisconnect)
    {
        qint64 entry_id = request.entry_id();

        if (entry_id == -1)
        {
            const QList<Relay*>& relays = Service::instance()->relays();
            QList<qint64> relay_ids;

            for (const auto& relay : relays)
                relay_ids.append(relay->sessionId());

            bool all_ok = true;
            for (qint64 id : relay_ids)
            {
                if (!Service::instance()->stopRelay(id))
                {
                    CLOG(ERROR) << "Failed to stop relay session:" << id;
                    all_ok = false;
                }
            }

            if (all_ok)
            {
                CLOG(INFO) << "All relay sessions disconnected by" << userName();
                relay_result->set_error_code(proto::router::kErrorOk);
            }
            else
            {
                relay_result->set_error_code(proto::router::kErrorInternalError);
            }
        }
        else
        {
            if (!Service::instance()->stopRelay(entry_id))
            {
                CLOG(ERROR) << "Session not found:" << entry_id;
                relay_result->set_error_code(proto::router::kErrorInvalidEntryId);
            }
            else
            {
                CLOG(INFO) << "Relay session '" << entry_id << "' disconnected by" << userName();
                relay_result->set_error_code(proto::router::kErrorOk);
            }
        }
    }
    else
    {
        CLOG(ERROR) << "Unknown relay request command:" << request.command_name();
        relay_result->set_error_code(proto::router::kErrorInvalidRequest);
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::doClientRequest(const proto::router::ClientRequest& request)
{
    proto::router::RouterToAdmin message;
    proto::router::ClientResult* client_result = message.mutable_client_result();
    client_result->set_request_id(request.request_id());
    client_result->set_command_name(request.command_name());

    if (request.command_name() == proto::router::kCommandClientDisconnect)
    {
        qint64 entry_id = request.entry_id();

        if (entry_id == -1)
        {
            const QList<Client*>& clients = Service::instance()->clients();
            QList<qint64> client_ids;

            for (const auto& client : clients)
                client_ids.append(client->sessionId());

            bool all_ok = true;
            for (qint64 id : client_ids)
            {
                if (!Service::instance()->stopClient(id))
                {
                    CLOG(ERROR) << "Failed to stop client session:" << id;
                    all_ok = false;
                }
            }

            if (all_ok)
            {
                CLOG(INFO) << "All client sessions disconnected by" << userName();
                client_result->set_error_code(proto::router::kErrorOk);
            }
            else
            {
                client_result->set_error_code(proto::router::kErrorInternalError);
            }
        }
        else
        {
            if (!Service::instance()->stopClient(entry_id))
            {
                CLOG(ERROR) << "Session not found:" << entry_id;
                client_result->set_error_code(proto::router::kErrorInvalidEntryId);
            }
            else
            {
                CLOG(INFO) << "Client session" << entry_id << "disconnected by" << userName();
                client_result->set_error_code(proto::router::kErrorOk);
            }
        }
    }
    else
    {
        CLOG(ERROR) << "Unknown client request command:" << request.command_name();
        client_result->set_error_code(proto::router::kErrorInvalidRequest);
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::doPeerRequest(const proto::router::PeerRequest& request)
{
    proto::router::RouterToAdmin message;
    proto::router::PeerResult* result = message.mutable_peer_result();
    result->set_request_id(request.request_id());
    result->set_command_name(request.command_name());

    Relay* relay_session = Service::instance()->relay(request.relay_id());
    if (!relay_session)
    {
        CLOG(ERROR) << "Relay with id" << request.relay_id() << "is not found";
        result->set_error_code(proto::router::kErrorNotFound);
    }
    else
    {
        relay_session->disconnectPeerSession(request);
        result->set_error_code(proto::router::kErrorOk);
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientAdmin::doWorkspaceRequest(const proto::router::WorkspaceRequest& request)
{
    proto::router::RouterToAdmin message;
    proto::router::WorkspaceResult* result = message.mutable_workspace_result();
    result->set_request_id(request.request_id());
    result->set_command_name(request.command_name());

    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        result->set_error_code(proto::router::kErrorInternalError);
        sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
        return;
    }

    const proto::router::Workspace& workspace = request.workspace();
    const std::string name(strTrimmed(workspace.name()));
    const std::string_view comment = workspace.comment();
    const qint64 entry_id = workspace.entry_id();

    std::set<HostId> desired_host_ids;
    for (int i = 0; i < workspace.host_id_size(); ++i)
        desired_host_ids.insert(workspace.host_id(i));

    if (request.command_name() == proto::router::kCommandWorkspaceAdd)
    {
        CLOG(INFO) << "Workspace add request:" << name << "with" << workspace.access_size()
                   << "access entries and" << desired_host_ids.size() << "hosts";

        bool self_present = false;
        QList<Workspace::Access> initial_access;
        initial_access.reserve(workspace.access_size());

        for (int i = 0; i < workspace.access_size(); ++i)
        {
            const proto::router::WorkspaceAccess& src = workspace.access(i);
            Workspace::Access dst;
            dst.user_id    = src.user_id();
            dst.wrapped_gk = QByteArray::fromStdString(src.wrapped_gk());
            initial_access.append(dst);

            if (dst.user_id == userId())
                self_present = true;
        }

        if (!self_present)
        {
            CLOG(ERROR) << "Admin" << userName() << "tried to create workspace without own access";
            result->set_error_code(proto::router::kErrorInvalidData);
        }
        else
        {
            qint64 new_id = -1;
            std::string_view error_code = database.addWorkspace(name, comment, initial_access, &new_id);
            result->set_error_code(error_code);
            if (error_code == proto::router::kErrorOk)
            {
                result->set_entry_id(new_id);
                Service::instance()->notifyChanged(Service::NOTIFY_WORKSPACES);
                if (!desired_host_ids.empty())
                {
                    error_code = database.setWorkspaceHosts(new_id, desired_host_ids);
                    if (error_code != proto::router::kErrorOk)
                    {
                        CLOG(ERROR) << "Workspace" << new_id
                                    << "created but host assignments failed:" << error_code;
                        result->set_error_code(error_code);
                    }
                    else
                    {
                        Service::instance()->notifyChanged(Service::NOTIFY_HOSTS);
                    }
                }
            }
        }
    }
    else if (request.command_name() == proto::router::kCommandWorkspaceModify)
    {
        CLOG(INFO) << "Workspace modify request:" << entry_id << name
                   << "with" << workspace.access_size() << "access entries and"
                   << desired_host_ids.size() << "hosts";

        bool self_present = false;
        QList<Workspace::Access> desired_access;
        desired_access.reserve(workspace.access_size());

        for (int i = 0; i < workspace.access_size(); ++i)
        {
            const proto::router::WorkspaceAccess& src = workspace.access(i);
            Workspace::Access dst;
            dst.user_id    = src.user_id();
            dst.wrapped_gk = QByteArray::fromStdString(src.wrapped_gk());
            desired_access.append(dst);

            if (dst.user_id == userId())
                self_present = true;
        }

        if (!self_present)
        {
            CLOG(ERROR) << "Admin" << userName() << "tried to revoke own access to workspace" << entry_id;
            result->set_error_code(proto::router::kErrorInvalidData);
        }
        else
        {
            std::string_view error_code = database.modifyWorkspace(entry_id, name, comment, desired_access);
            result->set_error_code(error_code);
            if (error_code == proto::router::kErrorOk)
            {
                Service::instance()->notifyChanged(Service::NOTIFY_WORKSPACES);
                error_code = database.setWorkspaceHosts(entry_id, desired_host_ids);
                if (error_code != proto::router::kErrorOk)
                {
                    CLOG(ERROR) << "Workspace" << entry_id
                                << "modified but host assignments failed:" << error_code;
                    result->set_error_code(error_code);
                }
                else
                {
                    Service::instance()->notifyChanged(Service::NOTIFY_HOSTS);
                }
            }
        }
    }
    else if (request.command_name() == proto::router::kCommandWorkspaceDelete)
    {
        CLOG(INFO) << "Workspace delete request:" << entry_id;
        const std::string_view error_code = database.removeWorkspace(entry_id);
        result->set_error_code(error_code);
        if (error_code == proto::router::kErrorOk)
        {
            // Workspace deletion also releases its hosts to workspace_id=0; signal both so
            // clients refetch both lists with updated workspace_id columns.
            Service::instance()->notifyChanged(Service::NOTIFY_WORKSPACES | Service::NOTIFY_HOSTS);
        }
    }
    else
    {
        CLOG(ERROR) << "Unknown workspace request command:" << request.command_name();
        result->set_error_code(proto::router::kErrorInvalidRequest);
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
std::string ClientAdmin::addUser(const proto::router::User& user)
{
    CLOG(INFO) << "User add request:" << user.name();

    RouterUser new_user = RouterUser::parseFrom(user);
    if (!new_user.isValid())
    {
        CLOG(ERROR) << "Failed to create user";
        return proto::router::kErrorInternalError;
    }

    if (!User::isValidUserName(new_user.name))
    {
        CLOG(ERROR) << "Invalid user name:" << new_user.name;
        return proto::router::kErrorInvalidData;
    }

    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return proto::router::kErrorInternalError;
    }

    if (!database.addUser(new_user))
        return proto::router::kErrorInternalError;

    Service::instance()->notifyChanged(Service::NOTIFY_USERS);
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string ClientAdmin::modifyUser(const proto::router::User& user)
{
    CLOG(INFO) << "User modify request:" << user.name();

    if (user.entry_id() <= 0)
    {
        CLOG(ERROR) << "Invalid user ID:" << user.entry_id();
        return proto::router::kErrorInvalidData;
    }

    RouterUser new_user = RouterUser::parseFrom(user);
    if (!new_user.isValid())
    {
        CLOG(ERROR) << "Failed to create user";
        return proto::router::kErrorInternalError;
    }

    if (!User::isValidUserName(new_user.name))
    {
        CLOG(ERROR) << "Invalid user name:" << new_user.name;
        return proto::router::kErrorInvalidData;
    }

    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return proto::router::kErrorInternalError;
    }

    // On a password rotation the stored wrapped GKs must be replaced with the keys re-sealed by
    // the admin to the new key pair. The user update, token revocation and re-wrap happen in one
    // transaction inside modifyUser; if the re-sealed set is incomplete the whole change is
    // rejected, so the user never loses workspace access. The keys are only consumed when the
    // password actually changes (decided authoritatively inside modifyUser).
    std::unordered_map<qint64, QByteArray> wrapped_keys;
    wrapped_keys.reserve(user.workspace_key_size());
    for (int i = 0; i < user.workspace_key_size(); ++i)
    {
        const proto::router::User::WorkspaceKey& wk = user.workspace_key(i);
        wrapped_keys.emplace(wk.workspace_id(), QByteArray::fromStdString(wk.wrapped_gk()));
    }

    bool password_changed = false;
    const std::string_view error_code = database.modifyUser(new_user, wrapped_keys, &password_changed);
    if (error_code != proto::router::kErrorOk)
    {
        CLOG(ERROR) << "modifyUser failed:" << error_code;
        return std::string(error_code);
    }

    // The rotation revoked every device token; drop the live sessions so the new password applies
    // immediately.
    if (password_changed)
        Service::instance()->stopClients(new_user.entry_id);

    Service::instance()->notifyChanged(Service::NOTIFY_USERS);
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string ClientAdmin::deleteUser(const proto::router::User& user)
{
    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return proto::router::kErrorInternalError;
    }

    qint64 entry_id = user.entry_id();

    CLOG(INFO) << "User remove request:" << entry_id;

    const std::string_view error_code = database.removeUser(entry_id);
    if (error_code != proto::router::kErrorOk)
    {
        CLOG(ERROR) << "removeUser failed:" << error_code;
        return std::string(error_code);
    }

    Service::instance()->notifyChanged(Service::NOTIFY_USERS);
    return proto::router::kErrorOk;
}
