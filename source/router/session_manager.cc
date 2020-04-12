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
#include "net/channel.h"
#include "proto/router.pb.h"
#include "router/database.h"

namespace router {

SessionManager::SessionManager(std::unique_ptr<net::Channel> channel,
                               std::shared_ptr<Database> database)
    : Session(std::move(channel)),
      database_(std::move(database))
{
    // Nothing
}

SessionManager::~SessionManager() = default;

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
        LOG(LS_INFO) << "PEER LIST REQUEST";
    }
    else if (message.has_peer_request())
    {
        LOG(LS_INFO) << "PEER REQUEST";
    }
    else if (message.has_proxy_list_request())
    {
        LOG(LS_INFO) << "PROXY LIST REQUEST";
    }
    else if (message.has_user_list_request())
    {
        doUserListRequest();
    }
    else if (message.has_user_request())
    {
        LOG(LS_INFO) << "USER REQUEST";
    }
    else
    {
        LOG(LS_WARNING) << "Unhandled message from manager";
    }
}

void SessionManager::onMessageWritten()
{
    // Nothing
}

void SessionManager::doUserListRequest()
{
    proto::RouterToManager message;
    proto::UserList* list = message.mutable_user_list();

    net::ServerUserList users = database_->userList();
    for (net::ServerUserList::Iterator it(users); !it.isAtEnd(); it.advance())
    {
        const net::ServerUser& user = it.user();
        proto::User* item = list->add_user();

        item->set_name(base::utf8FromUtf16(user.name));
        item->set_sessions(user.sessions);
        item->set_flags(user.flags);
    }

    send(base::serialize(message));
}

} // namespace router
