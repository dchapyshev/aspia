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

#include "router/server.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/version_constants.h"
#include "base/crypto/random.h"
#include "base/net/tcp_channel.h"
#include "router/database.h"
#include "router/database_factory_sqlite.h"
#include "router/session_admin.h"
#include "router/session_client.h"
#include "router/session_host.h"
#include "router/session_relay.h"
#include "router/settings.h"
#include "router/user_list_db.h"

namespace router {

namespace {

//--------------------------------------------------------------------------------------------------
const char* sessionTypeToString(proto::RouterSession session_type)
{
    switch (session_type)
    {
        case proto::ROUTER_SESSION_CLIENT:
            return "ROUTER_SESSION_CLIENT";

        case proto::ROUTER_SESSION_HOST:
            return "ROUTER_SESSION_HOST";

        case proto::ROUTER_SESSION_ADMIN:
            return "ROUTER_SESSION_ADMIN";

        case proto::ROUTER_SESSION_RELAY:
            return "ROUTER_SESSION_RELAY";

        default:
            return "ROUTER_SESSION_UNKNOWN";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
Server::Server(QObject* parent)
    : QObject(parent),
      database_factory_(base::make_local_shared<DatabaseFactorySqlite>())
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
Server::~Server()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool Server::start()
{
    if (server_)
    {
        LOG(LS_ERROR) << "Server already started";
        return false;
    }

    std::unique_ptr<Database> database = database_factory_->openDatabase();
    if (!database)
    {
        LOG(LS_ERROR) << "Failed to open the database";
        return false;
    }

    Settings settings;

    QByteArray private_key = settings.privateKey();
    if (private_key.isEmpty())
    {
        LOG(LS_INFO) << "The private key is not specified in the configuration file";
        return false;
    }

    QString listen_interface = settings.listenInterface();
    if (!base::TcpServer::isValidListenInterface(listen_interface))
    {
        LOG(LS_ERROR) << "Invalid listen interface address";
        return false;
    }

    quint16 port = settings.port();
    if (!port)
    {
        LOG(LS_ERROR) << "Invalid port specified in configuration file";
        return false;
    }

    client_white_list_ = settings.clientWhiteList();
    if (client_white_list_.empty())
    {
        LOG(LS_INFO) << "Empty client white list. Connections from all clients will be allowed";
    }
    else
    {
        LOG(LS_INFO) << "Client white list is not empty. Allowed clients: " << client_white_list_;
    }

    host_white_list_ = settings.hostWhiteList();
    if (host_white_list_.empty())
    {
        LOG(LS_INFO) << "Empty host white list. Connections from all hosts will be allowed";
    }
    else
    {
        LOG(LS_INFO) << "Host white list is not empty. Allowed hosts: " << host_white_list_;
    }

    admin_white_list_ = settings.adminWhiteList();
    if (admin_white_list_.empty())
    {
        LOG(LS_INFO) << "Empty admin white list. Connections from all admins will be allowed";
    }
    else
    {
        LOG(LS_INFO) << "Admin white list is not empty. Allowed admins: " << admin_white_list_;
    }

    relay_white_list_ = settings.relayWhiteList();
    if (relay_white_list_.empty())
    {
        LOG(LS_INFO) << "Empty relay white list. Connections from all relays will be allowed";
    }
    else
    {
        LOG(LS_INFO) << "Relay white list is not empty. Allowed relays: " << relay_white_list_;
    }

    QByteArray seed_key = settings.seedKey();
    if (seed_key.isEmpty())
    {
        LOG(LS_INFO) << "Empty seed key. New key generated";
        seed_key = base::Random::byteArray(64);
        settings.setSeedKey(seed_key);
    }

    std::unique_ptr<base::UserListBase> user_list = UserListDb::open(*database_factory_);
    user_list->setSeedKey(seed_key);

    authenticator_manager_ = new base::ServerAuthenticatorManager(this);

    connect(authenticator_manager_, &base::ServerAuthenticatorManager::sig_sessionReady,
            this, &Server::onSessionAuthenticated);

    authenticator_manager_->setPrivateKey(private_key);
    authenticator_manager_->setUserList(std::move(user_list));
    authenticator_manager_->setAnonymousAccess(
        base::ServerAuthenticator::AnonymousAccess::ENABLE,
        proto::ROUTER_SESSION_HOST | proto::ROUTER_SESSION_RELAY);

    relay_key_pool_ = std::make_unique<SharedKeyPool>(this);

    server_ = std::make_unique<base::TcpServer>();
    connect(server_.get(), &base::TcpServer::sig_newConnection, this, &Server::onNewConnection);
    server_->start(listen_interface, port);

    LOG(LS_INFO) << "Server started";
    return true;
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<proto::SessionList> Server::sessionList() const
{
    std::unique_ptr<proto::SessionList> result = std::make_unique<proto::SessionList>();

    for (const auto& session : sessions_)
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

                for (const auto& host_id : static_cast<SessionHost*>(session.get())->hostIdList())
                    session_data.add_host_id(host_id);

                item->set_session_data(session_data.SerializeAsString());
            }
            break;

            case proto::ROUTER_SESSION_RELAY:
            {
                proto::RelaySessionData session_data;
                session_data.set_pool_size(relay_key_pool_->countForRelay(session->sessionId()));

                const std::optional<proto::RelayStat>& in_relay_stat =
                    static_cast<SessionRelay*>(session.get())->relayStat();
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
    return result;
}

//--------------------------------------------------------------------------------------------------
bool Server::stopSession(Session::SessionId session_id)
{
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it)
    {
        if (it->get()->sessionId() == session_id)
        {
            sessions_.erase(it);
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void Server::onHostSessionWithId(SessionHost* session)
{
    if (!session)
    {
        LOG(LS_ERROR) << "Invalid session pointer";
        return;
    }

    for (auto it = sessions_.begin(); it != sessions_.end();)
    {
        Session* other_session_ptr = it->get();

        if (!other_session_ptr || other_session_ptr->sessionType() != proto::ROUTER_SESSION_HOST)
        {
            ++it;
            continue;
        }

        SessionHost* other_session = reinterpret_cast<SessionHost*>(other_session_ptr);
        if (other_session == session)
        {
            ++it;
            continue;
        }

        bool is_found = false;

        for (const auto& host_id : session->hostIdList())
        {
            if (other_session->hasHostId(host_id))
            {
                LOG(LS_INFO) << "Detected previous connection with ID " << host_id;

                is_found = true;
                break;
            }
        }

        if (is_found)
            it = sessions_.erase(it);
        else
            ++it;
    }
}

//--------------------------------------------------------------------------------------------------
SessionHost* Server::hostSessionById(base::HostId host_id)
{
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it)
    {
        Session* entry = it->get();

        if (entry->sessionType() == proto::ROUTER_SESSION_HOST &&
            static_cast<SessionHost*>(entry)->hasHostId(host_id))
        {
            return static_cast<SessionHost*>(entry);
        }
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
Session* Server::sessionById(Session::SessionId session_id)
{
    for (auto& session : sessions_)
    {
        if (session->sessionId() == session_id)
            return session.get();
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
void Server::onPoolKeyUsed(Session::SessionId session_id, quint32 key_id)
{
    for (const auto& session : sessions_)
    {
        SessionRelay* relay_session = static_cast<SessionRelay*>(session.get());
        if (relay_session->sessionId() == session_id)
            relay_session->sendKeyUsed(key_id);
    }
}

//--------------------------------------------------------------------------------------------------
void Server::onSessionFinished(Session::SessionId session_id, proto::RouterSession /* session_type */)
{
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it)
    {
        if (it->get()->sessionId() == session_id)
        {
            // Session will be destroyed after completion of the current call.
            it->release()->deleteLater();

            // Delete a session from the list.
            sessions_.erase(it);
            break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Server::onSessionAuthenticated()
{
    if (!authenticator_manager_)
    {
        LOG(LS_ERROR) << "No authenticator manager instance";
        return;
    }

    while (authenticator_manager_->hasReadySessions())
    {
        base::ServerAuthenticatorManager::SessionInfo session_info =
            authenticator_manager_->nextReadySession();

        QString address = session_info.channel->peerAddress();
        proto::RouterSession session_type =
            static_cast<proto::RouterSession>(session_info.session_type);

        LOG(LS_INFO) << "New session: " << sessionTypeToString(session_type) << " (" << address << ")";

        if (session_info.version >= base::kVersion_2_6_0)
        {
            LOG(LS_INFO) << "Using channel id support";
            session_info.channel->setChannelIdSupport(true);
        }

        std::unique_ptr<Session> session;

        switch (session_info.session_type)
        {
        case proto::ROUTER_SESSION_CLIENT:
        {
            if (!client_white_list_.empty() && !client_white_list_.contains(address))
                break;

            session = std::make_unique<SessionClient>();
        }
        break;

        case proto::ROUTER_SESSION_HOST:
        {
            if (!host_white_list_.empty() && !host_white_list_.contains(address))
                break;

            session = std::make_unique<SessionHost>();
        }
        break;

        case proto::ROUTER_SESSION_ADMIN:
        {
            if (!admin_white_list_.empty() && !admin_white_list_.contains(address))
                break;

            session = std::make_unique<SessionAdmin>();
        }
        break;

        case proto::ROUTER_SESSION_RELAY:
        {
            if (!relay_white_list_.empty() && !relay_white_list_.contains(address))
                break;

            session = std::make_unique<SessionRelay>();
        }
        break;

        default:
        {
            LOG(LS_ERROR) << "Unsupported session type: "
                          << static_cast<int>(session_info.session_type);
        }
        break;
        }

        if (!session)
        {
            LOG(LS_ERROR) << "Connection rejected for '" << address << "'";
            return;
        }

        session->setChannel(std::move(session_info.channel));
        session->setDatabaseFactory(database_factory_);
        session->setServer(this);
        session->setRelayKeyPool(relay_key_pool_->share());
        session->setVersion(session_info.version);
        session->setOsName(session_info.os_name);
        session->setComputerName(session_info.computer_name);
        session->setArchitecture(session_info.architecture);
        session->setUserName(session_info.user_name);

        sessions_.emplace_back(std::move(session));
        sessions_.back()->start(this);
    }
}

//--------------------------------------------------------------------------------------------------
void Server::onNewConnection()
{
    if (!server_)
    {
        LOG(LS_ERROR) << "No TCP server instance";
        return;
    }

    while (server_->hasPendingConnections())
    {
        std::unique_ptr<base::TcpChannel> channel(server_->nextPendingConnection());
        LOG(LS_INFO) << "New connection: " << channel->peerAddress();
        authenticator_manager_->addNewChannel(std::move(channel));
    }
}

} // namespace router
