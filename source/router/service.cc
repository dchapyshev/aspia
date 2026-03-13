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

#include <QHostAddress>

#include "base/logging.h"
#include "base/crypto/random.h"
#include "base/net/tcp_channel.h"
#include "base/peer/stun_server.h"
#include "router/migration_utils.h"
#include "router/session_admin.h"
#include "router/session_client.h"
#include "router/session_host.h"
#include "router/session_relay.h"
#include "router/settings.h"
#include "router/user_list.h"

namespace router {

namespace {

//--------------------------------------------------------------------------------------------------
bool isAddressAllowed(const QStringList& white_list, const QString& address_string)
{
    if (white_list.isEmpty())
        return true;

    QHostAddress address(address_string);
    if (address.protocol() != QAbstractSocket::IPv4Protocol &&
        address.protocol() != QAbstractSocket::IPv6Protocol)
    {
        LOG(ERROR) << "Invalid peer IP address:" << address_string;
        return false;
    }

    for (const auto& entry : white_list)
    {
        QHostAddress white_list_address(entry);
        if (white_list_address == address)
            return true;

        QPair<QHostAddress, int> subnet = QHostAddress::parseSubnet(entry);
        if (subnet.second >= 0 && address.isInSubnet(subnet))
            return true;
    }

    return false;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
const char Service::kFileName[] = "aspia_router.exe";
const char Service::kName[] = "aspia-router";
const char Service::kDisplayName[] = "Aspia Router Service";
const char Service::kDescription[] =
    "Assigns identifiers to peers and routes traffic to bypass NAT.";

//--------------------------------------------------------------------------------------------------
Service::Service(QObject* parent)
    : base::Service(Service::kName, parent)
{
    LOG(INFO) << "Ctor";
    instance_ = this;
}

//--------------------------------------------------------------------------------------------------
Service::~Service()
{
    LOG(INFO) << "Dtor";

    // Sessions can access the |instance_|, so we delete them all before zeroing the |instance_|.
    for (auto* session : std::as_const(sessions_))
        delete session;

    instance_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
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
void Service::addKey(qint64 session_id, const proto::router::RelayKey& key)
{
    auto relay = key_pool_.find(session_id);
    if (relay == key_pool_.end())
    {
        LOG(INFO) << "Host not found in key pool. It will be added";
        relay = key_pool_.insert(session_id, Keys());
    }

    LOG(INFO) << "Added key with id" << key.key_id() << "for host" << session_id;
    relay.value().append(key);
}

//--------------------------------------------------------------------------------------------------
std::optional<Service::Credentials> Service::takeCredentials()
{
    if (key_pool_.isEmpty())
    {
        LOG(ERROR) << "Empty key pool";
        return std::nullopt;
    }

    auto preffered_relay = key_pool_.end();
    int max_count = 0;

    for (auto it = key_pool_.begin(), it_end = key_pool_.end(); it != it_end; ++it)
    {
        int count = it.value().size();
        if (count > max_count)
        {
            preffered_relay = it;
            max_count = count;
        }
    }

    if (preffered_relay == key_pool_.end())
    {
        LOG(ERROR) << "Empty key pool";
        return std::nullopt;
    }

    LOG(INFO) << "Preffered relay:" << preffered_relay.key();

    QList<proto::router::RelayKey>& keys = preffered_relay.value();
    if (keys.isEmpty())
    {
        LOG(ERROR) << "Empty key pool for relay";
        return std::nullopt;
    }

    Credentials credentials;
    credentials.session_id = preffered_relay.key();
    credentials.key = std::move(keys.back());

    // Removing the key from the pool.
    keys.pop_back();

    if (keys.isEmpty())
    {
        LOG(INFO) << "Last key in the pool for relay. The relay will be removed from the pool";
        key_pool_.remove(preffered_relay.key());
    }

    for (auto* session : std::as_const(sessions_))
    {
        if (session->sessionId() == credentials.session_id)
        {
            SessionRelay* relay_session = static_cast<SessionRelay*>(session);
            relay_session->sendKeyUsed(credentials.key.key_id());
        }
    }

    return std::move(credentials);
}

//--------------------------------------------------------------------------------------------------
void Service::removeKeysForRelay(qint64 session_id)
{
    LOG(INFO) << "All keys for relay" << session_id << "removed";
    key_pool_.remove(session_id);
}

//--------------------------------------------------------------------------------------------------
void Service::clearKeyPool()
{
    LOG(INFO) << "Key pool cleared";
    key_pool_.clear();
}

//--------------------------------------------------------------------------------------------------
size_t Service::keyCountForRelay(qint64 session_id) const
{
    auto result = key_pool_.find(session_id);
    if (result == key_pool_.end())
        return 0;
    return result.value().size();
}

//--------------------------------------------------------------------------------------------------
size_t Service::keyCount() const
{
    size_t result = 0;

    for (const auto& relay : std::as_const(key_pool_))
        result += relay.size();

    return result;
}

//--------------------------------------------------------------------------------------------------
bool Service::isKeyPoolEmpty() const
{
    return key_pool_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
void Service::onStart()
{
    LOG(INFO) << "Service start...";

    if (isMigrationNeeded())
        doMigration();

    if (!start())
    {
        LOG(ERROR) << "Unable to start service";
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
void Service::onNewConnection()
{
    CHECK(tcp_server_);
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
    CHECK(tcp_server_legacy_);
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
        LOG(ERROR) << "Server already is started";
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
    if (client_white_list_.isEmpty())
        LOG(INFO) << "Empty client white list. Connections from all clients will be allowed";
    else
        LOG(INFO) << "Client white list is not empty. Allowed clients:" << client_white_list_;

    host_white_list_ = settings.hostWhiteList();
    if (host_white_list_.isEmpty())
        LOG(INFO) << "Empty host white list. Connections from all hosts will be allowed";
    else
        LOG(INFO) << "Host white list is not empty. Allowed hosts:" << host_white_list_;

    admin_white_list_ = settings.adminWhiteList();
    if (admin_white_list_.isEmpty())
        LOG(INFO) << "Empty admin white list. Connections from all admins will be allowed";
    else
        LOG(INFO) << "Admin white list is not empty. Allowed admins:" << admin_white_list_;

    relay_white_list_ = settings.relayWhiteList();
    if (relay_white_list_.isEmpty())
        LOG(INFO) << "Empty relay white list. Connections from all relays will be allowed";
    else
        LOG(INFO) << "Relay white list is not empty. Allowed relays:" << relay_white_list_;

    QByteArray seed_key = settings.seedKey();
    if (seed_key.isEmpty())
    {
        LOG(INFO) << "Empty seed key. New key is generated";
        seed_key = base::Random::byteArray(64);
        settings.setSeedKey(seed_key);
    }

    base::SharedPointer<base::UserListBase> user_list(UserList::open().release());
    user_list->setSeedKey(seed_key);

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

    if (settings.isStunEnabled())
    {
        stun_server_ = new base::StunServer(this);
        stun_server_->start(settings.stunPort());
    }

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
            if (!isAddressAllowed(client_white_list_, address))
                break;

            SessionClient* client_session = new SessionClient(channel, this);
            if (stun_server_)
                client_session->setStunInfo(stun_server_->port());
            session = client_session;
        }
        break;

        case proto::router::SESSION_TYPE_HOST:
        {
            if (!isAddressAllowed(host_white_list_, address))
                break;
            session = new SessionHost(channel, this);
        }
        break;

        case proto::router::SESSION_TYPE_ADMIN:
        {
            if (!isAddressAllowed(admin_white_list_, address))
                break;
            session = new SessionAdmin(channel, this);
        }
        break;

        case proto::router::SESSION_TYPE_RELAY:
        {
            if (!isAddressAllowed(relay_white_list_, address))
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
        LOG(ERROR) << "Connection is rejected for" << address;
        channel->deleteLater();
        return;
    }

    sessions_.emplace_back(session);
    connect(session, &Session::sig_finished, this, &Service::onSessionFinished);
    session->start();
}

//--------------------------------------------------------------------------------------------------
// static
Service* Service::instance_ = nullptr;

} // namespace router
