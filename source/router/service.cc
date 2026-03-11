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

#include "router/service.h"

#include "base/logging.h"
#include "base/crypto/random.h"
#include "base/net/tcp_channel.h"
#include "router/database.h"
#include "router/database_factory_sqlite.h"
#include "router/key_factory.h"
#include "router/migration_utils.h"
#include "router/session_admin.h"
#include "router/session_client.h"
#include "router/session_host.h"
#include "router/session_relay.h"
#include "router/settings.h"
#include "router/user_list_db.h"

namespace router {

//--------------------------------------------------------------------------------------------------
// static
const char Service::kFileName[] = "aspia_router.exe";
const char Service::kName[] = "aspia-router";
const char Service::kDisplayName[] = "Aspia Router Service";
const char Service::kDescription[] =
    "Assigns identifiers to peers and routes traffic to bypass NAT.";

//--------------------------------------------------------------------------------------------------
Service::Service(QObject* parent)
    : base::Service(Service::kName, parent),
      database_factory_(new DatabaseFactorySqlite())
{
    LOG(INFO) << "Ctor";
    instance_ = this;
}

//--------------------------------------------------------------------------------------------------
Service::~Service()
{
    LOG(INFO) << "Dtor";
    instance_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
Service* Service::instance()
{
    return instance_;
}

//--------------------------------------------------------------------------------------------------
QList<Session*> Service::sessions()
{
    return sessions_;
}

//--------------------------------------------------------------------------------------------------
Session* Service::session(qint64 session_id)
{
    for (auto* session : std::as_const(sessions_))
    {
        if (session->sessionId() == session_id)
            return session;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
bool Service::stopSession(qint64 session_id)
{
    for (auto it = sessions_.begin(), it_end = sessions_.end(); it != it_end; ++it)
    {
        Session* session = *it;

        if (session->sessionId() == session_id)
        {
            session->disconnect();
            session->deleteLater();
            sessions_.erase(it);
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void Service::onStart()
{
    LOG(INFO) << "Service start...";

    if (isMigrationNeeded())
        doMigration();

    if (!start())
    {
        LOG(ERROR) << "Unable to start server. Service not started";
        QCoreApplication::quit();
        return;
    }

    LOG(INFO) << "Service started";
}

//--------------------------------------------------------------------------------------------------
void Service::onStop()
{
    LOG(INFO) << "Service stopping";
}

//--------------------------------------------------------------------------------------------------
void Service::onPoolKeyUsed(qint64 session_id, quint32 key_id)
{
    for (auto* session : std::as_const(sessions_))
    {
        if (session->sessionId() == session_id)
        {
            SessionRelay* relay_session = static_cast<SessionRelay*>(session);
            relay_session->sendKeyUsed(key_id);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onNewConnection()
{
    if (!tcp_server_)
    {
        LOG(ERROR) << "No TCP server instance";
        return;
    }

    while (tcp_server_->hasReadyConnections())
    {
        base::TcpChannel* channel = tcp_server_->nextReadyConnection();
        LOG(INFO) << "New connection:" << channel->peerAddress();
        addSession(channel);
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onNewLegacyConnection()
{
    if (!tcp_server_legacy_)
    {
        LOG(ERROR) << "No legacy TCP server instance";
        return;
    }

    while (tcp_server_legacy_->hasReadyConnections())
    {
        base::TcpChannel* channel = tcp_server_legacy_->nextReadyConnection();
        LOG(INFO) << "New legacy connection:" << channel->peerAddress();
        addSession(channel);
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onSessionFinished()
{
    Session* session = dynamic_cast<Session*>(sender());
    CHECK(session);
    session->disconnect();
    session->deleteLater();
    sessions_.removeOne(session);
}

//--------------------------------------------------------------------------------------------------
bool Service::start()
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

    quint16 legacy_port = settings.legacyPort();
    if (!legacy_port)
    {
        LOG(ERROR) << "Invalid legacy port specified in configuration file";
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

    base::SharedPointer<base::UserListBase> user_list(UserListDb::open(*database_factory_).release());
    user_list->setSeedKey(seed_key);

    key_factory_ = new KeyFactory(this);
    connect(key_factory_, &KeyFactory::sig_keyUsed, this, &Service::onPoolKeyUsed);

    tcp_server_ = new base::TcpServer(this);
    connect(tcp_server_, &base::TcpServer::sig_newConnection, this, &Service::onNewConnection);

    tcp_server_->setPrivateKey(private_key);
    tcp_server_->setUserList(user_list);
    tcp_server_->setAnonymousAccess(
        base::ServerAuthenticator::AnonymousAccess::ENABLE,
        proto::router::SESSION_TYPE_HOST | proto::router::SESSION_TYPE_RELAY);
    tcp_server_->start(port, listen_interface);

    tcp_server_legacy_ = new base::TcpServerLegacy(this);
    connect(tcp_server_legacy_, &base::TcpServerLegacy::sig_newConnection, this, &Service::onNewLegacyConnection);

    tcp_server_legacy_->setPrivateKey(private_key);
    tcp_server_legacy_->setUserList(user_list);
    tcp_server_legacy_->setAnonymousAccess(
        base::ServerAuthenticator::AnonymousAccess::ENABLE,
        proto::router::SESSION_TYPE_HOST | proto::router::SESSION_TYPE_RELAY);
    tcp_server_legacy_->start(legacy_port, listen_interface);

    LOG(INFO) << "Server started";
    return true;
}

//--------------------------------------------------------------------------------------------------
void Service::addSession(base::TcpChannel* channel)
{
    QString address = channel->peerAddress();
    proto::router::SessionType session_type =
        static_cast<proto::router::SessionType>(channel->peerSessionType());

    LOG(INFO) << "New session:" << session_type << "(" << address << ")";

    Session* session = nullptr;

    switch (session_type)
    {
        case proto::router::SESSION_TYPE_CLIENT:
        {
            if (!client_white_list_.isEmpty() && !client_white_list_.contains(address))
                break;
            session = new SessionClient(channel, this);
        }
        break;

        case proto::router::SESSION_TYPE_HOST:
        {
            if (!host_white_list_.isEmpty() && !host_white_list_.contains(address))
                break;
            session = new SessionHost(channel, this);
        }
        break;

        case proto::router::SESSION_TYPE_ADMIN:
        {
            if (!admin_white_list_.isEmpty() && !admin_white_list_.contains(address))
                break;
            session = new SessionAdmin(channel, this);
        }
        break;

        case proto::router::SESSION_TYPE_RELAY:
        {
            if (!relay_white_list_.isEmpty() && !relay_white_list_.contains(address))
                break;
            session = new SessionRelay(channel, this);
        }
        break;

        default:
            LOG(ERROR) << "Unsupported session type:" << session_type;
            break;
    }

    if (!session)
    {
        LOG(ERROR) << "Connection rejected for" << address;
        channel->deleteLater();
        return;
    }

    session->setDatabaseFactory(database_factory_);
    session->setRelayKeyPool(key_factory_->sharedKeyPool());

    sessions_.emplace_back(session);
    connect(session, &Session::sig_finished, this, &Service::onSessionFinished);
    session->start();
}

//--------------------------------------------------------------------------------------------------
// static
Service* Service::instance_ = nullptr;

} // namespace router
