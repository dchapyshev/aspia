//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/strings/unicode.h"
#include "base/net/network_channel.h"
#include "peer/user.h"
#include "router/database.h"
#include "router/server_proxy.h"

namespace router {

SessionManager::SessionManager(std::unique_ptr<base::NetworkChannel> channel,
                               std::shared_ptr<DatabaseFactory> database_factory,
                               std::shared_ptr<ServerProxy> server_proxy)
    : Session(proto::ROUTER_SESSION_MANAGER, std::move(channel), std::move(database_factory)),
      server_proxy_(std::move(server_proxy))
{
    DCHECK(server_proxy_);
}

SessionManager::~SessionManager() = default;

void SessionManager::onSessionReady()
{
    // Nothing
}

void SessionManager::onMessageReceived(const base::ByteArray& buffer)
{
    proto::ManagerToRouter message;

    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Could not read message from manager";
        return;
    }

    if (message.has_peer_list_request())
    {
        doPeerListRequest();
    }
    else if (message.has_peer_request())
    {
        LOG(LS_INFO) << "PEER REQUEST";
    }
    else if (message.has_relay_list_request())
    {
        doRelayListRequest();
    }
    else if (message.has_user_list_request())
    {
        doUserListRequest();
    }
    else if (message.has_user_request())
    {
        doUserRequest(message.user_request());
    }
    else
    {
        LOG(LS_WARNING) << "Unhandled message from manager";
    }
}

void SessionManager::onMessageWritten(size_t pending)
{
    // Nothing
}

void SessionManager::doUserListRequest()
{
    std::unique_ptr<Database> database = openDatabase();
    if (!database)
    {
        LOG(LS_ERROR) << "Failed to connect to database";
        return;
    }

    proto::RouterToManager message;
    proto::UserList* list = message.mutable_user_list();

    peer::UserList users = database->userList();
    for (peer::UserList::Iterator it(users); !it.isAtEnd(); it.advance())
    {
        const peer::User& user = it.user();
        proto::User* item = list->add_user();

        item->set_entry_id(user.entry_id);
        item->set_name(base::utf8FromUtf16(user.name));
        item->set_sessions(user.sessions);
        item->set_flags(user.flags);
    }

    sendMessage(message);
}

void SessionManager::doUserRequest(const proto::UserRequest& request)
{
    std::unique_ptr<Database> database = openDatabase();
    if (!database)
    {
        LOG(LS_ERROR) << "Failed to connect to database";
        return;
    }

    proto::RouterToManager message;
    proto::UserResult* result = message.mutable_user_result();
    result->set_type(request.type());

    switch (request.type())
    {
        case proto::USER_REQUEST_ADD:
        {
            std::u16string username = base::utf16FromUtf8(request.user().name());
            std::u16string password = base::utf16FromUtf8(request.user().password());

            LOG(LS_INFO) << "User add request: " << username;

            if (!peer::User::isValidUserName(username) || !peer::User::isValidPassword(password))
            {
                LOG(LS_ERROR) << "Invalid user name or password";
                result->set_error_code(proto::UserResult::INVALID_DATA);
                break;
            }

            peer::User user = peer::User::create(username, password);
            if (!user.isValid())
            {
                LOG(LS_ERROR) << "Failed to create user";
                result->set_error_code(proto::UserResult::INTERNAL_ERROR);
                break;
            }

            user.sessions = request.user().sessions();
            user.flags = request.user().flags();

            if (!database->addUser(user))
            {
                result->set_error_code(proto::UserResult::INTERNAL_ERROR);
                break;
            }

            result->set_error_code(proto::UserResult::SUCCESS);
        }
        break;

        case proto::USER_REQUEST_MODIFY:
        {
            NOTIMPLEMENTED();
        }
        break;

        case proto::USER_REQUEST_DELETE:
        {
            uint64_t entry_id = request.user().entry_id();

            LOG(LS_INFO) << "User remove request: " << entry_id;

            if (!database->removeUser(entry_id))
            {
                result->set_error_code(proto::UserResult::INTERNAL_ERROR);
                break;
            }

            result->set_error_code(proto::UserResult::SUCCESS);
        }
        break;
    }

    sendMessage(message);
}

void SessionManager::doRelayListRequest()
{
    proto::RouterToManager message;

    message.set_allocated_relay_list(server_proxy_->relayList().release());
    if (!message.has_relay_list())
        message.mutable_relay_list()->set_error_code(proto::RelayList::UNKNOWN_ERROR);

    sendMessage(message);
}

void SessionManager::doPeerListRequest()
{
    proto::RouterToManager message;

    message.set_allocated_peer_list(server_proxy_->peerList().release());
    if (!message.has_peer_list())
        message.mutable_peer_list()->set_error_code(proto::PeerList::UNKNOWN_ERROR);

    sendMessage(message);
}

} // namespace router
