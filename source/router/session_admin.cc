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

#include "router/session_admin.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/peer/router_user.h"
#include "router/database.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "proto/router_host.h"
#include "router/service.h"
#include "router/session_host.h"
#include "router/session_legacy_host.h"
#include "router/session_relay.h"

//--------------------------------------------------------------------------------------------------
SessionAdmin::SessionAdmin(TcpChannel* channel, QObject* parent)
    : SessionManager(channel, parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
SessionAdmin::~SessionAdmin()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::onSessionMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::router::CHANNEL_ID_ADMIN)
    {
        SessionManager::onSessionMessage(channel_id, buffer);
        return;
    }

    proto::router::AdminToRouter message;
    if (!parse(buffer, &message))
    {
        CLOG(ERROR) << "Could not read message from manager";
        return;
    }

    if (message.has_relay_list_request())
    {
        doRelayListRequest();
    }
    else if (message.has_host_request())
    {
        doHostRequest(message.host_request());
    }
    else if (message.has_relay_request())
    {
        doRelayRequest(message.relay_request());
    }
    else if (message.has_client_list_request())
    {
        doClientListRequest();
    }
    else if (message.has_client_request())
    {
        doClientRequest(message.client_request());
    }
    else if (message.has_user_list_request())
    {
        doUserListRequest();
    }
    else if (message.has_user_request())
    {
        doUserRequest(message.user_request());
    }
    else if (message.has_peer_request())
    {
        doPeerRequest(message.peer_request());
    }
    else if (message.has_workspace_list_request())
    {
        doWorkspaceListRequest();
    }
    else if (message.has_workspace_request())
    {
        doWorkspaceRequest(message.workspace_request());
    }
    else
    {
        CLOG(ERROR) << "Unhandled message from manager";
    }
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doRelayListRequest()
{
    const QList<Session*>& sessions = Service::instance()->sessions();

    proto::router::RouterToAdmin message;
    proto::router::RelayList* result = message.mutable_relay_list();
    result->set_error_code(proto::router::kErrorOk);

    for (const auto& session : sessions)
    {
        if (session->sessionType() != proto::router::SESSION_TYPE_RELAY)
            continue;

        proto::router::RelayInfo* item = result->add_relay();

        // Generic session info.
        item->set_entry_id(session->sessionId());
        item->set_timepoint(session->startTime());
        item->set_ip_address(session->address().toString().toStdString());
        item->mutable_version()->CopyFrom(serialize(session->version()));
        item->set_os_name(session->osName().toStdString());
        item->set_computer_name(session->computerName().toStdString());
        item->set_architecture(session->architecture().toStdString());

        // Statistics info.
        const std::optional<proto::router::RelayStatistics>& statistics =
            static_cast<SessionRelay*>(session)->statistics();
        if (statistics.has_value())
        {
            item->mutable_statistics()->mutable_peer()->CopyFrom(statistics->peer());
            item->mutable_statistics()->set_uptime(statistics->uptime());
        }

        // Other info.
        item->set_pool_size(Service::instance()->keyCountForRelay(session->sessionId()));
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doClientListRequest()
{
    const QList<Session*>& sessions = Service::instance()->sessions();

    proto::router::RouterToAdmin message;
    proto::router::ClientList* result = message.mutable_client_list();
    result->set_error_code(proto::router::kErrorOk);

    for (const auto& session : sessions)
    {
        proto::router::SessionType type = session->sessionType();
        if (type != proto::router::SESSION_TYPE_CLIENT &&
            type != proto::router::SESSION_TYPE_MANAGER &&
            type != proto::router::SESSION_TYPE_ADMIN)
        {
            continue;
        }

        proto::router::ClientInfo* item = result->add_client();

        item->set_entry_id(session->sessionId());
        item->set_timepoint(session->startTime());
        item->set_ip_address(session->address().toString().toStdString());
        item->mutable_version()->CopyFrom(serialize(session->version()));
        item->set_os_name(session->osName().toStdString());
        item->set_computer_name(session->computerName().toStdString());
        item->set_architecture(session->architecture().toStdString());
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doUserListRequest()
{
    proto::router::RouterToAdmin message;
    proto::router::UserList* list = message.mutable_user_list();

    Database database = Database::open();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        list->set_error_code(proto::router::kErrorInternalError);
    }
    else
    {
        list->set_error_code(proto::router::kErrorOk);

        QVector<RouterUser> users = database.userList();
        for (const auto& user : std::as_const(users))
            list->add_user()->CopyFrom(user.serialize());
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doUserRequest(const proto::router::UserRequest& request)
{
    proto::router::RouterToAdmin message;
    proto::router::UserResult* result = message.mutable_user_result();
    result->set_command_name(request.command_name());

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
        result->set_error_code(deleteUser(request.user()));
    }
    else
    {
        CLOG(ERROR) << "Unknown user request command:" << request.command_name();
        result->set_error_code(proto::router::kErrorInvalidRequest);
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doHostRequest(const proto::router::HostRequest& request)
{
    auto find_host_session = [](HostId host_id) -> SessionHost*
    {
        const QList<Session*>& sessions = Service::instance()->sessions();
        for (Session* session : std::as_const(sessions))
        {
            if (session->sessionType() != proto::router::SESSION_TYPE_HOST)
                continue;

            SessionHost* host_session = dynamic_cast<SessionHost*>(session);
            if (host_session && host_session->hostId() == host_id)
                return host_session;
        }
        return nullptr;
    };

    proto::router::RouterToAdmin message;
    proto::router::HostResult* host_result = message.mutable_host_result();
    host_result->set_command_name(request.command_name());

    if (request.command_name() == proto::router::kCommandHostDisconnect)
    {
        const qint64 host_id = request.host_id();

        if (host_id == -1)
        {
            const QList<Session*>& sessions = Service::instance()->sessions();
            QList<qint64> host_session_ids;
            for (const auto& session : sessions)
            {
                if (session->sessionType() == proto::router::SESSION_TYPE_HOST)
                    host_session_ids.append(session->sessionId());
            }

            bool all_ok = true;
            for (qint64 id : host_session_ids)
            {
                if (!Service::instance()->stopSession(id))
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
            SessionHost* host_session = find_host_session(static_cast<HostId>(host_id));
            if (!host_session)
            {
                CLOG(ERROR) << "No live session for host_id:" << host_id;
                host_result->set_error_code(proto::router::kErrorInvalidEntryId);
            }
            else if (!Service::instance()->stopSession(host_session->sessionId()))
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
        const qint64 host_id = request.host_id();

        Database database = Database::open();
        if (!database.isValid())
        {
            CLOG(ERROR) << "Failed to connect to database";
            host_result->set_error_code(proto::router::kErrorInternalError);
        }
        else if (!database.scheduleHostRemoval(static_cast<HostId>(host_id)))
        {
            CLOG(ERROR) << "Failed to schedule host removal for host_id:" << host_id;
            host_result->set_error_code(proto::router::kErrorInternalError);
        }
        else
        {
            // The hosts row is now in hosts_remove; if the host is online send it the remove
            // command and let SessionHost finalize the hosts_remove row on disconnect. Otherwise
            // the command is issued on reconnect.
            SessionHost* host_session = find_host_session(static_cast<HostId>(host_id));
            if (host_session)
                host_session->sendRemoveCommand();

            CLOG(INFO) << "Host" << host_id << "removal scheduled by" << userName()
                       << "(online:" << (host_session != nullptr) << ")";
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
void SessionAdmin::doRelayRequest(const proto::router::RelayRequest& request)
{
    proto::router::RouterToAdmin message;
    proto::router::RelayResult* relay_result = message.mutable_relay_result();
    relay_result->set_command_name(request.command_name());

    if (request.command_name() == proto::router::kCommandRelayDisconnect)
    {
        qint64 entry_id = request.entry_id();

        if (entry_id == -1)
        {
            const QList<Session*>& sessions = Service::instance()->sessions();
            QList<qint64> relay_session_ids;
            for (const auto& session : sessions)
            {
                if (session->sessionType() == proto::router::SESSION_TYPE_RELAY)
                    relay_session_ids.append(session->sessionId());
            }

            bool all_ok = true;
            for (qint64 id : relay_session_ids)
            {
                if (!Service::instance()->stopSession(id))
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
            if (!Service::instance()->stopSession(entry_id))
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
void SessionAdmin::doClientRequest(const proto::router::ClientRequest& request)
{
    proto::router::RouterToAdmin message;
    proto::router::ClientResult* client_result = message.mutable_client_result();
    client_result->set_command_name(request.command_name());

    if (request.command_name() == proto::router::kCommandClientDisconnect)
    {
        qint64 entry_id = request.entry_id();

        if (entry_id == -1)
        {
            const QList<Session*>& sessions = Service::instance()->sessions();
            QList<qint64> client_session_ids;
            for (const auto& session : sessions)
            {
                proto::router::SessionType type = session->sessionType();
                if (type == proto::router::SESSION_TYPE_CLIENT ||
                    type == proto::router::SESSION_TYPE_MANAGER ||
                    type == proto::router::SESSION_TYPE_ADMIN)
                {
                    client_session_ids.append(session->sessionId());
                }
            }

            bool all_ok = true;
            for (qint64 id : client_session_ids)
            {
                if (!Service::instance()->stopSession(id))
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
            if (!Service::instance()->stopSession(entry_id))
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
void SessionAdmin::doPeerRequest(const proto::router::PeerRequest& request)
{
    SessionRelay* relay_session =
        dynamic_cast<SessionRelay*>(Service::instance()->session(request.relay_session_id()));
    if (!relay_session)
    {
        CLOG(ERROR) << "Relay with id" << request.relay_session_id() << "is not found";
        return;
    }

    relay_session->disconnectPeerSession(request);
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doWorkspaceListRequest()
{
    proto::router::RouterToAdmin message;
    proto::router::WorkspaceList* list = message.mutable_workspace_list();

    Database database = Database::open();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        list->set_error_code(proto::router::kErrorInternalError);
    }
    else
    {
        list->set_error_code(proto::router::kErrorOk);

        const QVector<Workspace> workspaces = database.workspaceList();
        for (const auto& workspace : std::as_const(workspaces))
        {
            proto::router::Workspace* item = list->add_workspace();
            item->set_entry_id(workspace.entry_id);
            item->set_name(workspace.name.toStdString());
            item->set_comment(workspace.comment.toStdString());

            const QVector<Workspace::Access> accesses = database.workspaceAccessList(workspace.entry_id);
            for (const auto& access : std::as_const(accesses))
            {
                proto::router::WorkspaceAccess* access_item = item->add_access();
                access_item->set_user_id(access.user_id);
                access_item->set_wrapped_gk(access.wrapped_gk.toStdString());
            }
        }
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doWorkspaceRequest(const proto::router::WorkspaceRequest& request)
{
    proto::router::RouterToAdmin message;
    proto::router::WorkspaceResult* result = message.mutable_workspace_result();
    result->set_command_name(request.command_name());

    Database database = Database::open();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        result->set_error_code(proto::router::kErrorInternalError);
        sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
        return;
    }

    const proto::router::Workspace& workspace = request.workspace();
    const QString name = QString::fromStdString(workspace.name()).trimmed();
    const QByteArray comment = QByteArray::fromStdString(workspace.comment());
    const qint64 entry_id = workspace.entry_id();

    if (request.command_name() == proto::router::kCommandWorkspaceAdd)
    {
        CLOG(INFO) << "Workspace add request:" << name << "with" << workspace.access_size() << "access entries";

        bool self_present = false;
        QVector<Workspace::Access> initial_access;
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
                result->set_entry_id(new_id);
        }
    }
    else if (request.command_name() == proto::router::kCommandWorkspaceModify)
    {
        CLOG(INFO) << "Workspace modify request:" << entry_id << name
                   << "with" << workspace.access_size() << "access entries";

        bool self_present = false;
        QVector<Workspace::Access> desired_access;
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
            result->set_error_code(database.modifyWorkspace(entry_id, name, comment, desired_access));
        }
    }
    else if (request.command_name() == proto::router::kCommandWorkspaceDelete)
    {
        CLOG(INFO) << "Workspace delete request:" << entry_id;
        result->set_error_code(database.removeWorkspace(entry_id));
    }
    else
    {
        CLOG(ERROR) << "Unknown workspace request command:" << request.command_name();
        result->set_error_code(proto::router::kErrorInvalidRequest);
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
std::string SessionAdmin::addUser(const proto::router::User& user)
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

    Database database = Database::open();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return proto::router::kErrorInternalError;
    }

    if (!database.addUser(new_user))
        return proto::router::kErrorInternalError;

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string SessionAdmin::modifyUser(const proto::router::User& user)
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

    Database database = Database::open();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return proto::router::kErrorInternalError;
    }

    if (!database.modifyUser(new_user))
    {
        CLOG(ERROR) << "modifyUser failed";
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string SessionAdmin::deleteUser(const proto::router::User& user)
{
    Database database = Database::open();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return proto::router::kErrorInternalError;
    }

    qint64 entry_id = user.entry_id();

    CLOG(INFO) << "User remove request:" << entry_id;

    if (!database.removeUser(entry_id))
    {
        CLOG(ERROR) << "removeUser failed";
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}
