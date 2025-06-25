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

//--------------------------------------------------------------------------------------------------
Server::Server(QObject* parent)
    : QObject(parent),
      database_factory_(new DatabaseFactorySqlite()),
      session_manager_(new SessionManager(this))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
Server::~Server()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool Server::start()
{
    if (tcp_server_)
    {
        LOG(ERROR) << "Server already started";
        return false;
    }

    std::unique_ptr<Database> database = database_factory_->openDatabase();
    if (!database)
    {
        LOG(ERROR) << "Failed to open the database";
        return false;
    }

    Settings settings;

    QByteArray private_key = settings.privateKey();
    if (private_key.isEmpty())
    {
        LOG(INFO) << "The private key is not specified in the configuration file";
        return false;
    }

    QString listen_interface = settings.listenInterface();
    if (!base::TcpServer::isValidListenInterface(listen_interface))
    {
        LOG(ERROR) << "Invalid listen interface address";
        return false;
    }

    quint16 port = settings.port();
    if (!port)
    {
        LOG(ERROR) << "Invalid port specified in configuration file";
        return false;
    }

    client_white_list_ = settings.clientWhiteList();
    if (client_white_list_.empty())
    {
        LOG(INFO) << "Empty client white list. Connections from all clients will be allowed";
    }
    else
    {
        LOG(INFO) << "Client white list is not empty. Allowed clients:" << client_white_list_;
    }

    host_white_list_ = settings.hostWhiteList();
    if (host_white_list_.empty())
    {
        LOG(INFO) << "Empty host white list. Connections from all hosts will be allowed";
    }
    else
    {
        LOG(INFO) << "Host white list is not empty. Allowed hosts:" << host_white_list_;
    }

    admin_white_list_ = settings.adminWhiteList();
    if (admin_white_list_.empty())
    {
        LOG(INFO) << "Empty admin white list. Connections from all admins will be allowed";
    }
    else
    {
        LOG(INFO) << "Admin white list is not empty. Allowed admins:" << admin_white_list_;
    }

    relay_white_list_ = settings.relayWhiteList();
    if (relay_white_list_.empty())
    {
        LOG(INFO) << "Empty relay white list. Connections from all relays will be allowed";
    }
    else
    {
        LOG(INFO) << "Relay white list is not empty. Allowed relays:" << relay_white_list_;
    }

    QByteArray seed_key = settings.seedKey();
    if (seed_key.isEmpty())
    {
        LOG(INFO) << "Empty seed key. New key generated";
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
        proto::router::SESSION_TYPE_HOST | proto::router::SESSION_TYPE_RELAY);

    key_factory_ = new KeyFactory(this);
    connect(key_factory_, &KeyFactory::sig_keyUsed, this, &Server::onPoolKeyUsed);

    tcp_server_ = new base::TcpServer(this);
    connect(tcp_server_, &base::TcpServer::sig_newConnection, this, &Server::onNewConnection);
    tcp_server_->start(port, listen_interface);

    LOG(INFO) << "Server started";
    return true;
}

//--------------------------------------------------------------------------------------------------
void Server::onPoolKeyUsed(Session::SessionId session_id, quint32 key_id)
{
    QList<Session*> sessions = session_manager_->sessions();

    for (const auto& session : std::as_const(sessions))
    {
        if (session->sessionId() == session_id)
        {
            SessionRelay* relay_session = static_cast<SessionRelay*>(session);
            relay_session->sendKeyUsed(key_id);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Server::onSessionAuthenticated()
{
    if (!authenticator_manager_)
    {
        LOG(ERROR) << "No authenticator manager instance";
        return;
    }

    while (authenticator_manager_->hasReadySessions())
    {
        base::ServerAuthenticatorManager::SessionInfo session_info =
            authenticator_manager_->nextReadySession();

        QString address = session_info.channel->peerAddress();
        proto::router::SessionType session_type =
            static_cast<proto::router::SessionType>(session_info.session_type);

        LOG(INFO) << "New session:" << session_type << "(" << address << ")";

        Session* session = nullptr;

        switch (session_info.session_type)
        {
            case proto::router::SESSION_TYPE_CLIENT:
            {
                if (!client_white_list_.isEmpty() && !client_white_list_.contains(address))
                    break;

                session = new SessionClient(session_manager_);
            }
            break;

            case proto::router::SESSION_TYPE_HOST:
            {
                if (!host_white_list_.isEmpty() && !host_white_list_.contains(address))
                    break;

                session = new SessionHost(session_manager_);
            }
            break;

            case proto::router::SESSION_TYPE_ADMIN:
            {
                if (!admin_white_list_.isEmpty() && !admin_white_list_.contains(address))
                    break;

                session = new SessionAdmin(session_manager_);
            }
            break;

            case proto::router::SESSION_TYPE_RELAY:
            {
                if (!relay_white_list_.isEmpty() && !relay_white_list_.contains(address))
                    break;

                session = new SessionRelay(session_manager_);
            }
            break;

            default:
            {
                LOG(ERROR) << "Unsupported session type:"
                           << static_cast<int>(session_info.session_type);
            }
            break;
        }

        if (!session)
        {
            LOG(ERROR) << "Connection rejected for" << address;
            return;
        }

        session->setChannel(session_info.channel);
        session->setDatabaseFactory(database_factory_);
        session->setRelayKeyPool(key_factory_->sharedKeyPool());
        session->setVersion(session_info.version);
        session->setOsName(session_info.os_name);
        session->setComputerName(session_info.computer_name);
        session->setArchitecture(session_info.architecture);
        session->setUserName(session_info.user_name);

        session_manager_->addSession(session);
        session->start();
    }
}

//--------------------------------------------------------------------------------------------------
void Server::onNewConnection()
{
    if (!tcp_server_)
    {
        LOG(ERROR) << "No TCP server instance";
        return;
    }

    while (tcp_server_->hasPendingConnections())
    {
        base::TcpChannel* channel = tcp_server_->nextPendingConnection();
        LOG(INFO) << "New connection:" << channel->peerAddress();
        authenticator_manager_->addNewChannel(channel);
    }
}

} // namespace router
