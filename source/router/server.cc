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
#include "base/strings/unicode.h"
#include "proto/router.pb.h"
#include "router/database_factory_sqlite.h"
#include "router/database_sqlite.h"
#include "router/server_proxy.h"
#include "router/session_admin.h"
#include "router/session_peer.h"
#include "router/session_relay.h"
#include "router/settings.h"

#if defined(OS_WIN)
#include "base/files/base_paths.h"
#include "base/net/firewall_manager.h"
#endif // defined(OS_WIN)

namespace router {

namespace {

#if defined(OS_WIN)
const wchar_t kFirewallRuleName[] = L"Aspia Router Service";
const wchar_t kFirewallRuleDecription[] = L"Allow incoming TCP connections";
#endif // defined(OS_WIN)

const char* sessionTypeToString(proto::RouterSession session_type)
{
    switch (session_type)
    {
        case proto::ROUTER_SESSION_AUTHORIZED_PEER:
            return "ROUTER_SESSION_AUTHORIZED_PEER";

        case proto::ROUTER_SESSION_ANONIMOUS_PEER:
            return "ROUTER_SESSION_ANONIMOUS_PEER";

        case proto::ROUTER_SESSION_ADMIN:
            return "ROUTER_SESSION_ADMIN";

        case proto::ROUTER_SESSION_RELAY:
            return "ROUTER_SESSION_RELAY";

        default:
            return "ROUTER_SESSION_UNKNOWN";
    }
}

} // namespace

Server::Server(std::shared_ptr<base::TaskRunner> task_runner)
    : server_proxy_(new ServerProxy(this)),
      task_runner_(std::move(task_runner)),
      database_factory_(std::make_shared<DatabaseFactorySqlite>())
{
    DCHECK(task_runner_);
}

Server::~Server()
{
#if defined(OS_WIN)
    deleteFirewallRules();
#endif // defined(OS_WIN)

    server_proxy_->willDestroyCurrentServer();
}

bool Server::start()
{
    if (server_)
        return false;

    std::unique_ptr<Database> database = database_factory_->openDatabase();
    if (!database)
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

#if defined(OS_WIN)
    addFirewallRules(port);
#endif // defined(OS_WIN)

    authenticator_manager_ =
        std::make_unique<peer::ServerAuthenticatorManager>(task_runner_, this);
    authenticator_manager_->setPrivateKey(private_key);
    authenticator_manager_->setUserList(std::make_shared<peer::UserList>(database->userList()));
    authenticator_manager_->setAnonymousAccess(
        peer::ServerAuthenticator::AnonymousAccess::ENABLE,
        proto::ROUTER_SESSION_ANONIMOUS_PEER | proto::ROUTER_SESSION_RELAY);

    server_ = std::make_unique<base::NetworkServer>();
    server_->start(port, this);

    return true;
}

std::unique_ptr<proto::RelayList> Server::relayList() const
{
    std::unique_ptr<proto::RelayList> result = std::make_unique<proto::RelayList>();

    for (const auto& session : sessions_)
    {
        if (session->sessionType() != proto::ROUTER_SESSION_RELAY)
            continue;

        SessionRelay* session_relay = static_cast<SessionRelay*>(session.get());

        proto::Relay* relay = result->add_relay();
        relay->set_address(base::utf8FromUtf16(session_relay->address()));
        relay->set_pool_size(session_relay->poolSize());
    }

    result->set_error_code(proto::RelayList::SUCCESS);
    return result;
}

std::unique_ptr<proto::PeerList> Server::peerList() const
{
    std::unique_ptr<proto::PeerList> result = std::make_unique<proto::PeerList>();

    for (const auto& session : sessions_)
    {
        if (session->sessionType() != proto::ROUTER_SESSION_ANONIMOUS_PEER)
            continue;

        SessionPeer* session_peer = static_cast<SessionPeer*>(session.get());

        proto::Peer* peer = result->add_peer();
        peer->set_ip_address(base::utf8FromUtf16(session_peer->address()));
        peer->set_user_name(base::utf8FromUtf16(session_peer->userName()));
        peer->set_peer_id(session_peer->peerId());
    }

    result->set_error_code(proto::PeerList::SUCCESS);
    return result;
}

void Server::onPeerSessionWithId(SessionPeer* session)
{
    peer::PeerId peer_id = session->peerId();

    for (auto it = sessions_.begin(); it != sessions_.end();)
    {
        Session* entry = it->get();

        if (entry != session &&
            entry->sessionType() == proto::ROUTER_SESSION_ANONIMOUS_PEER &&
            static_cast<SessionPeer*>(entry)->peerId() == peer_id)
        {
            LOG(LS_INFO) << "Detected previous connection with ID " << peer_id
                         << ". It will be completed";

            it = sessions_.erase(it);
            continue;
        }

        ++it;
    }
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
                session_type, std::move(session_info.channel), database_factory_, server_proxy_);
        }
        break;

        case proto::ROUTER_SESSION_ADMIN:
        {
            session = std::make_unique<SessionAdmin>(
                std::move(session_info.channel), database_factory_, server_proxy_);
        }
        break;

        case proto::ROUTER_SESSION_RELAY:
        {
            session = std::make_unique<SessionRelay>(
                std::move(session_info.channel), database_factory_);
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
        if (it->get()->state() == Session::State::FINISHED)
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

#if defined(OS_WIN)
void Server::addFirewallRules(uint16_t port)
{
    std::filesystem::path file_path;
    if (!base::BasePaths::currentExecFile(&file_path))
        return;

    base::FirewallManager firewall(file_path);
    if (!firewall.isValid())
        return;

    if (!firewall.addTcpRule(kFirewallRuleName, kFirewallRuleDecription, port))
        return;

    LOG(LS_INFO) << "Rule is added to the firewall";
}

void Server::deleteFirewallRules()
{
    std::filesystem::path file_path;
    if (!base::BasePaths::currentExecFile(&file_path))
        return;

    base::FirewallManager firewall(file_path);
    if (!firewall.isValid())
        return;

    firewall.deleteRuleByName(kFirewallRuleName);
}
#endif // defined(OS_WIN)

} // namespace router
