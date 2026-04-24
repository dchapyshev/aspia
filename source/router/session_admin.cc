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
#include "base/peer/user.h"
#include "router/database.h"
#include "router/service.h"
#include "router/session_host.h"
#include "router/session_legacy_host.h"
#include "router/session_relay.h"

namespace router {

//--------------------------------------------------------------------------------------------------
SessionAdmin::SessionAdmin(base::TcpChannel* channel, QObject* parent)
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
    if (!base::parse(buffer, &message))
    {
        CLOG(ERROR) << "Could not read message from manager";
        return;
    }

    if (message.has_relay_list_request())
    {
        doRelayListRequest();
    }
    else if (message.has_host_list_request())
    {
        doHostListRequest();
    }
    else if (message.has_host_request())
    {
        doHostRequest(message.host_request());
    }
    else if (message.has_relay_request())
    {
        doRelayRequest(message.relay_request());
    }
    else if (message.has_user_list_request())
    {
        doUserListRequest();
    }
    else if (message.has_user_request())
    {
        doUserRequest(message.user_request());
    }
    else if (message.has_peer_connection_request())
    {
        doPeerConnectionRequest(message.peer_connection_request());
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
    result->set_error_code("ok");

    for (const auto& session : sessions)
    {
        if (session->sessionType() != proto::router::SESSION_TYPE_RELAY)
            continue;

        proto::router::RelayInfo* item = result->add_relay();

        // Generic session info.
        item->set_entry_id(session->sessionId());
        item->set_timepoint(session->startTime());
        item->set_ip_address(session->address().toString().toStdString());
        item->mutable_version()->CopyFrom(base::serialize(session->version()));
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

    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doHostListRequest()
{
    const QList<Session*>& sessions = Service::instance()->sessions();

    proto::router::RouterToAdmin message;
    proto::router::HostList* result = message.mutable_host_list();
    result->set_error_code("ok");

    for (const auto& session : sessions)
    {
        if (session->sessionType() != proto::router::SESSION_TYPE_HOST)
            continue;

        proto::router::HostInfo* item = result->add_host();

        // Generic session info.
        item->set_entry_id(session->sessionId());
        item->set_timepoint(session->startTime());
        item->set_ip_address(session->address().toString().toStdString());
        item->mutable_version()->CopyFrom(base::serialize(session->version()));
        item->set_os_name(session->osName().toStdString());
        item->set_computer_name(session->computerName().toStdString());
        item->set_architecture(session->architecture().toStdString());

        // Other info.
        item->set_host_id(static_cast<SessionHost*>(session)->hostId());
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doUserListRequest()
{
    Database database = Database::open();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return;
    }

    proto::router::RouterToAdmin message;
    proto::router::UserList* list = message.mutable_user_list();

    QVector<base::User> users = database.userList();
    for (const auto& user : std::as_const(users))
        list->add_user()->CopyFrom(user.serialize());

    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doUserRequest(const proto::router::UserRequest& request)
{
    proto::router::RouterToAdmin message;
    proto::router::UserResult* result = message.mutable_user_result();
    result->set_type(request.type());

    switch (request.type())
    {
        case proto::router::USER_REQUEST_ADD:
            result->set_error_code(addUser(request.user()));
            break;

        case proto::router::USER_REQUEST_MODIFY:
            result->set_error_code(modifyUser(request.user()));
            break;

        case proto::router::USER_REQUEST_DELETE:
            result->set_error_code(deleteUser(request.user()));
            break;

        default:
            CLOG(ERROR) << "Unknown request type:" << request.type();
            return;
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doHostRequest(const proto::router::HostRequest& request)
{
    proto::router::RouterToAdmin message;
    proto::router::HostResult* host_result = message.mutable_host_result();
    host_result->set_command_name(request.command_name());

    if (request.command_name() == "disconnect")
    {
        qint64 entry_id = request.entry_id();

        if (entry_id == -1)
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
                host_result->set_error_code("ok");
            }
            else
            {
                host_result->set_error_code("internal_error");
            }
        }
        else
        {
            if (!Service::instance()->stopSession(entry_id))
            {
                CLOG(ERROR) << "Session not found:" << entry_id;
                host_result->set_error_code("invalid_entry_id");
            }
            else
            {
                CLOG(INFO) << "Host session" << entry_id << "disconnected by" << userName();
                host_result->set_error_code("ok");
            }
        }
    }
    else if (request.command_name() == "remove")
    {
        qint64 entry_id = request.entry_id();
        SessionHost* host_session = dynamic_cast<SessionHost*>(Service::instance()->session(entry_id));
        if (!host_session)
        {
            CLOG(ERROR) << "Host with id" << entry_id << "is not found";
            host_result->set_error_code("invalid_entry_id");
        }
        else
        {
            proto::router::HostCommand command;
            command.set_command_name(request.command_name());
            command.set_params(request.params());
            host_session->sendHostCommand(command);

            CLOG(INFO) << "Host" << entry_id << "removed by" << userName();
            host_result->set_error_code("ok");
        }
    }
    else
    {
        CLOG(ERROR) << "Unknown host request command:" << request.command_name();
        host_result->set_error_code("invalid_request");
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doRelayRequest(const proto::router::RelayRequest& request)
{
    proto::router::RouterToAdmin message;
    proto::router::RelayResult* relay_result = message.mutable_relay_result();
    relay_result->set_command_name(request.command_name());

    if (request.command_name() == "disconnect")
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
                relay_result->set_error_code("ok");
            }
            else
            {
                relay_result->set_error_code("internal_error");
            }
        }
        else
        {
            if (!Service::instance()->stopSession(entry_id))
            {
                CLOG(ERROR) << "Session not found:" << entry_id;
                relay_result->set_error_code("invalid_entry_id");
            }
            else
            {
                CLOG(INFO) << "Relay session '" << entry_id << "' disconnected by" << userName();
                relay_result->set_error_code("ok");
            }
        }
    }
    else
    {
        CLOG(ERROR) << "Unknown relay request command:" << request.command_name();
        relay_result->set_error_code("invalid_request");
    }

    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doPeerConnectionRequest(const proto::router::PeerConnectionRequest& request)
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
proto::router::UserResult::ErrorCode SessionAdmin::addUser(const proto::router::User& user)
{
    CLOG(INFO) << "User add request:" << user.name();

    base::User new_user = base::User::parseFrom(user);
    if (!new_user.isValid())
    {
        CLOG(ERROR) << "Failed to create user";
        return proto::router::UserResult::INTERNAL_ERROR;
    }

    if (!base::User::isValidUserName(new_user.name))
    {
        CLOG(ERROR) << "Invalid user name:" << new_user.name;
        return proto::router::UserResult::INVALID_DATA;
    }

    Database database = Database::open();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return proto::router::UserResult::INTERNAL_ERROR;
    }

    if (!database.addUser(new_user))
        return proto::router::UserResult::INTERNAL_ERROR;

    return proto::router::UserResult::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
proto::router::UserResult::ErrorCode SessionAdmin::modifyUser(const proto::router::User& user)
{
    CLOG(INFO) << "User modify request:" << user.name();

    if (user.entry_id() <= 0)
    {
        CLOG(ERROR) << "Invalid user ID:" << user.entry_id();
        return proto::router::UserResult::INVALID_DATA;
    }

    base::User new_user = base::User::parseFrom(user);
    if (!new_user.isValid())
    {
        CLOG(ERROR) << "Failed to create user";
        return proto::router::UserResult::INTERNAL_ERROR;
    }

    if (!base::User::isValidUserName(new_user.name))
    {
        CLOG(ERROR) << "Invalid user name:" << new_user.name;
        return proto::router::UserResult::INVALID_DATA;
    }

    Database database = Database::open();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return proto::router::UserResult::INTERNAL_ERROR;
    }

    if (!database.modifyUser(new_user))
    {
        CLOG(ERROR) << "modifyUser failed";
        return proto::router::UserResult::INTERNAL_ERROR;
    }

    return proto::router::UserResult::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
proto::router::UserResult::ErrorCode SessionAdmin::deleteUser(const proto::router::User& user)
{
    Database database = Database::open();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return proto::router::UserResult::INTERNAL_ERROR;
    }

    qint64 entry_id = user.entry_id();

    CLOG(INFO) << "User remove request:" << entry_id;

    if (!database.removeUser(entry_id))
    {
        CLOG(ERROR) << "removeUser failed";
        return proto::router::UserResult::INTERNAL_ERROR;
    }

    return proto::router::UserResult::SUCCESS;
}

} // namespace router
