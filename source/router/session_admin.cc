//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "router/session_host.h"
#include "router/session_relay.h"
#include "router/session_manager.h"

namespace router {

//--------------------------------------------------------------------------------------------------
SessionAdmin::SessionAdmin(QObject* parent)
    : Session(proto::ROUTER_SESSION_ADMIN, parent)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
SessionAdmin::~SessionAdmin()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::onSessionReady()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::onSessionMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    proto::AdminToRouter message;

    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Could not read message from manager";
        return;
    }

    if (message.has_session_list_request())
    {
        doSessionListRequest(message.session_list_request());
    }
    else if (message.has_session_request())
    {
        doSessionRequest(message.session_request());
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
        LOG(LS_ERROR) << "Unhandled message from manager";
    }
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::onSessionMessageWritten(quint8 /* channel_id */, size_t /* pending */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doUserListRequest()
{
    std::unique_ptr<Database> database = openDatabase();
    if (!database)
    {
        LOG(LS_ERROR) << "Failed to connect to database";
        return;
    }

    proto::RouterToAdmin message;
    proto::UserList* list = message.mutable_user_list();

    QVector<base::User> users = database->userList();
    for (const auto& user : std::as_const(users))
        list->add_user()->CopyFrom(user.serialize());

    sendMessage(proto::ROUTER_CHANNEL_ID_SESSION, message);
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doUserRequest(const proto::UserRequest& request)
{
    proto::RouterToAdmin message;
    proto::UserResult* result = message.mutable_user_result();
    result->set_type(request.type());

    switch (request.type())
    {
        case proto::USER_REQUEST_ADD:
            result->set_error_code(addUser(request.user()));
            break;

        case proto::USER_REQUEST_MODIFY:
            result->set_error_code(modifyUser(request.user()));
            break;

        case proto::USER_REQUEST_DELETE:
            result->set_error_code(deleteUser(request.user()));
            break;

        default:
            LOG(LS_ERROR) << "Unknown request type: " << request.type();
            return;
    }

    sendMessage(proto::ROUTER_CHANNEL_ID_SESSION, message);
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doSessionListRequest(const proto::SessionListRequest& /* request */)
{
    QList<Session*> session_list = sessions();

    proto::RouterToAdmin message;
    proto::SessionList* result = message.mutable_session_list();

    for (const auto& session : std::as_const(session_list))
    {
        proto::Session* item = result->add_session();

        item->set_session_id(session->sessionId());
        item->set_session_type(session->sessionType());
        item->set_timepoint(static_cast<quint64>(session->startTime()));
        item->set_ip_address(session->address().toStdString());
        item->mutable_version()->CopyFrom(base::serialize(session->version()));
        item->set_os_name(session->osName().toStdString());
        item->set_computer_name(session->computerName().toStdString());
        item->set_architecture(session->architecture().toStdString());

        switch (session->sessionType())
        {
            case proto::ROUTER_SESSION_HOST:
            {
                proto::HostSessionData session_data;

                for (const auto& host_id : static_cast<SessionHost*>(session)->hostIdList())
                    session_data.add_host_id(host_id);

                item->set_session_data(session_data.SerializeAsString());
            }
            break;

        case proto::ROUTER_SESSION_RELAY:
        {
            proto::RelaySessionData session_data;
            session_data.set_pool_size(relayKeyPool().countForRelay(session->sessionId()));

            const std::optional<proto::RelayStat>& in_relay_stat =
                static_cast<SessionRelay*>(session)->relayStat();
            if (in_relay_stat.has_value())
            {
                proto::RelaySessionData::RelayStat* out_relay_stat =
                    session_data.mutable_relay_stat();

                out_relay_stat->set_uptime(in_relay_stat->uptime());
                out_relay_stat->mutable_peer_connection()->CopyFrom(
                    in_relay_stat->peer_connection());
            }

            item->set_session_data(session_data.SerializeAsString());
        }
        break;

        default:
            break;
        }
    }

    result->set_error_code(proto::SessionList::SUCCESS);

    sendMessage(proto::ROUTER_CHANNEL_ID_SESSION, message);
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doSessionRequest(const proto::SessionRequest& request)
{
    proto::RouterToAdmin message;
    proto::SessionResult* session_result = message.mutable_session_result();
    session_result->set_type(request.type());

    if (request.type() == proto::SESSION_REQUEST_DISCONNECT)
    {
        Session::SessionId session_id = request.session_id();

        if (!sessionManager()->stopSession(session_id))
        {
            LOG(LS_ERROR) << "Session not found: " << session_id;
            session_result->set_error_code(proto::SessionResult::INVALID_SESSION_ID);
        }
        else
        {
            LOG(LS_INFO) << "Session '" << session_id << "' disconnected by " << userName();
            session_result->set_error_code(proto::SessionResult::SUCCESS);
        }
    }
    else
    {
        LOG(LS_ERROR) << "Unknown session request: " << request.type();
        session_result->set_error_code(proto::SessionResult::INVALID_REQUEST);
    }

    sendMessage(proto::ROUTER_CHANNEL_ID_SESSION, message);
}

//--------------------------------------------------------------------------------------------------
void SessionAdmin::doPeerConnectionRequest(const proto::PeerConnectionRequest& request)
{
    SessionRelay* relay_session =
        dynamic_cast<SessionRelay*>(sessionManager()->sessionById(request.relay_session_id()));
    if (!relay_session)
    {
        LOG(LS_ERROR) << "Relay with id " << request.relay_session_id() << " not found";
        return;
    }

    relay_session->disconnectPeerSession(request);
}

//--------------------------------------------------------------------------------------------------
proto::UserResult::ErrorCode SessionAdmin::addUser(const proto::User& user)
{
    LOG(LS_INFO) << "User add request: " << user.name();

    base::User new_user = base::User::parseFrom(user);
    if (!new_user.isValid())
    {
        LOG(LS_ERROR) << "Failed to create user";
        return proto::UserResult::INTERNAL_ERROR;
    }

    if (!base::User::isValidUserName(new_user.name))
    {
        LOG(LS_ERROR) << "Invalid user name: " << new_user.name;
        return proto::UserResult::INVALID_DATA;
    }

    std::unique_ptr<Database> database = openDatabase();
    if (!database)
    {
        LOG(LS_ERROR) << "Failed to connect to database";
        return proto::UserResult::INTERNAL_ERROR;
    }

    if (!database->addUser(new_user))
        return proto::UserResult::INTERNAL_ERROR;

    return proto::UserResult::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
proto::UserResult::ErrorCode SessionAdmin::modifyUser(const proto::User& user)
{
    LOG(LS_INFO) << "User modify request: " << user.name();

    if (user.entry_id() <= 0)
    {
        LOG(LS_ERROR) << "Invalid user ID: " << user.entry_id();
        return proto::UserResult::INVALID_DATA;
    }

    base::User new_user = base::User::parseFrom(user);
    if (!new_user.isValid())
    {
        LOG(LS_ERROR) << "Failed to create user";
        return proto::UserResult::INTERNAL_ERROR;
    }

    if (!base::User::isValidUserName(new_user.name))
    {
        LOG(LS_ERROR) << "Invalid user name: " << new_user.name;
        return proto::UserResult::INVALID_DATA;
    }

    std::unique_ptr<Database> database = openDatabase();
    if (!database)
    {
        LOG(LS_ERROR) << "Failed to connect to database";
        return proto::UserResult::INTERNAL_ERROR;
    }

    if (!database->modifyUser(new_user))
    {
        LOG(LS_ERROR) << "modifyUser failed";
        return proto::UserResult::INTERNAL_ERROR;
    }

    return proto::UserResult::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
proto::UserResult::ErrorCode SessionAdmin::deleteUser(const proto::User& user)
{
    std::unique_ptr<Database> database = openDatabase();
    if (!database)
    {
        LOG(LS_ERROR) << "Failed to connect to database";
        return proto::UserResult::INTERNAL_ERROR;
    }

    qint64 entry_id = user.entry_id();

    LOG(LS_INFO) << "User remove request: " << entry_id;

    if (!database->removeUser(entry_id))
    {
        LOG(LS_ERROR) << "removeUser failed";
        return proto::UserResult::INTERNAL_ERROR;
    }

    return proto::UserResult::SUCCESS;
}

} // namespace router
