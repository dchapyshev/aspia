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

#include "router/server.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "base/net/network_channel.h"
#include "proto/router.pb.h"
#include "router/database_sqlite.h"
#include "router/session_manager.h"
#include "router/session_peer.h"
#include "router/settings.h"

namespace router {

namespace {

const char* sessionTypeToString(proto::RouterSession session_type)
{
    switch (session_type)
    {
        case proto::ROUTER_SESSION_AUTHORIZED_PEER:
            return "ROUTER_SESSION_AUTHORIZED_PEER";

        case proto::ROUTER_SESSION_ANONIMOUS_PEER:
            return "ROUTER_SESSION_ANONIMOUS_PEER";

        case proto::ROUTER_SESSION_MANAGER:
            return "ROUTER_SESSION_MANAGER";

        default:
            return "ROUTER_SESSION_UNKNOWN";
    }
}

} // namespace

Server::Server(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner))
{
    DCHECK(task_runner_);
}

Server::~Server() = default;

bool Server::start()
{
    if (server_)
        return false;

    database_ = DatabaseSqlite::open();
    if (!database_)
    {
        LOG(LS_ERROR) << "Failed to open the database";
        return false;
    }

    Settings settings;

    base::ByteArray private_key = settings.privateKey();
    if (private_key.empty())
    {
        LOG(LS_ERROR) << "The private key is not specified in the configuration file";
        return false;
    }

    uint16_t port = settings.port();
    if (!port)
    {
        LOG(LS_ERROR) << "Invalid port specified in configuration file";
        return false;
    }

    authenticator_manager_ =
        std::make_unique<peer::ServerAuthenticatorManager>(task_runner_, this);
    authenticator_manager_->setPrivateKey(private_key);
    authenticator_manager_->setUserList(std::make_shared<peer::UserList>(database_->userList()));

    server_ = std::make_unique<base::NetworkServer>();
    server_->start(port, this);

    return true;
}

void Server::onNewConnection(std::unique_ptr<base::NetworkChannel> channel)
{
    LOG(LS_INFO) << "New connection: " << channel->peerAddress();

    if (authenticator_manager_)
        authenticator_manager_->addNewChannel(std::move(channel));
}

void Server::onNewSession(peer::ServerAuthenticatorManager::SessionInfo&& session_info)
{
    proto::RouterSession session_type =
        static_cast<proto::RouterSession>(session_info.session_type);

    LOG(LS_INFO) << "New session: " << sessionTypeToString(session_type)
                 << " (" << session_info.channel->peerAddress() << ")";

    std::unique_ptr<Session> session;

    switch (session_info.session_type)
    {
        case proto::ROUTER_SESSION_ANONIMOUS_PEER:
        case proto::ROUTER_SESSION_AUTHORIZED_PEER:
        {
            session = std::make_unique<SessionPeer>(
                session_type, std::move(session_info.channel), database_);
        }
        break;

        case proto::ROUTER_SESSION_MANAGER:
        {
            session = std::make_unique<SessionManager>(
                std::move(session_info.channel), database_);
        }
        break;

        default:
            break;
    }

    if (!session)
    {
        LOG(LS_ERROR) << "Unsupported session type: "
                      << static_cast<int>(session_info.session_type);
        return;
    }

    sessions_.emplace_back(std::move(session));
    sessions_.back()->start(this);
}

void Server::onSessionFinished()
{
    for (auto it = sessions_.begin(); it != sessions_.end();)
    {
        if (it->get()->isFinished())
        {
            // Session will be destroyed after completion of the current call.
            task_runner_->deleteSoon(std::move(*it));

            // Delete a session from the list.
            it = sessions_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace router
