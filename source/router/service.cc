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
#include <QTimer>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/net/tcp_channel.h"
#include "base/peer/stun_server.h"
#include "proto/router_client.h"
#include "router/migration_utils.h"
#include "router/session_admin.h"
#include "router/session_client.h"
#include "router/session_host.h"
#include "router/session_legacy_host.h"
#include "router/session_manager.h"
#include "router/session_relay.h"
#include "router/settings.h"
#include "router/router_user_list.h"

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
    : CoreService(Service::kName, parent),
      notification_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";
    instance_ = this;

    notification_timer_->setSingleShot(true);
    notification_timer_->setInterval(5000);
    connect(notification_timer_, &QTimer::timeout, this, &Service::onNotificationFlush);
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
const QList<Session*>& Service::sessions()
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
int Service::stopUserSessions(qint64 user_id, const QList<qint64>& token_ids, qint64 except_session_id)
{
    QList<qint64> session_ids;
    for (Session* session : std::as_const(sessions_))
    {
        SessionClient* client_session = dynamic_cast<SessionClient*>(session);
        if (!client_session || !client_session->isTwoFactorCompleted())
            continue;
        if (client_session->userId() != user_id || client_session->sessionId() == except_session_id)
            continue;
        if (!token_ids.isEmpty() && !token_ids.contains(client_session->tokenId()))
            continue;

        session_ids.append(client_session->sessionId());
    }

    int stopped = 0;
    for (qint64 id : std::as_const(session_ids))
    {
        if (stopSession(id))
        {
            ++stopped;
            LOG(INFO) << "Stopped session" << id << "of user" << user_id;
        }
    }

    return stopped;
}

//--------------------------------------------------------------------------------------------------
void Service::notifyChanged(quint32 flags)
{
    dirty_mask_ |= flags;
    if (!notification_timer_->isActive())
        notification_timer_->start();
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
        TcpChannel* channel = tcp_server_->nextReadyConnection();
        LOG(INFO) << "New connection:" << channel->peerAddress();
        addSession(channel, false);
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onNewLegacyConnection()
{
    CHECK(tcp_server_legacy_);
    while (tcp_server_legacy_->hasReadyConnections())
    {
        TcpChannel* channel = tcp_server_legacy_->nextReadyConnection();
        LOG(INFO) << "New legacy connection:" << channel->peerAddress();
        addSession(channel, true);
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
void Service::onHostIdAssigned(HostId host_id)
{
    QList<Session*> matched_sessions;

    for (Session* session : std::as_const(sessions_))
    {
        SessionHost* host_session = dynamic_cast<SessionHost*>(session);
        if (host_session)
        {
            if (host_session->hostId() == host_id)
                matched_sessions.append(session);
            continue;
        }

        SessionLegacyHost* legacy_host_session = dynamic_cast<SessionLegacyHost*>(session);
        if (legacy_host_session)
        {
            if (legacy_host_session->hasHostId(host_id))
                matched_sessions.append(session);
        }
    }

    if (matched_sessions.size() <= 1)
        return;

    Session* oldest_session = matched_sessions.first();
    for (int i = 1; i < matched_sessions.size(); ++i)
    {
        if (matched_sessions[i]->startTime() < oldest_session->startTime())
            oldest_session = matched_sessions[i];
    }

    LOG(INFO) << "Duplicate host ID" << host_id << "detected. Disconnecting older session (id"
              << oldest_session->sessionId() << ")";

    oldest_session->disconnect();
    oldest_session->deleteLater();
    sessions_.removeOne(oldest_session);
}

//--------------------------------------------------------------------------------------------------
void Service::onNotificationFlush()
{
    if (dirty_mask_ == 0)
        return;

    const bool hosts      = (dirty_mask_ & NOTIFY_HOSTS)      != 0;
    const bool relays     = (dirty_mask_ & NOTIFY_RELAYS)     != 0;
    const bool clients    = (dirty_mask_ & NOTIFY_CLIENTS)    != 0;
    const bool users      = (dirty_mask_ & NOTIFY_USERS)      != 0;
    const bool workspaces = (dirty_mask_ & NOTIFY_WORKSPACES) != 0;
    const bool groups     = (dirty_mask_ & NOTIFY_GROUPS)     != 0;
    dirty_mask_ = 0;

    // Admin gets the full bitmask (relays/clients/users/hosts/workspaces/groups).
    QByteArray admin_payload;
    {
        proto::router::RouterToClient message;
        proto::router::Notification* notification = message.mutable_notification();
        notification->set_hosts_dirty(hosts);
        notification->set_relays_dirty(relays);
        notification->set_clients_dirty(clients);
        notification->set_users_dirty(users);
        notification->set_workspaces_dirty(workspaces);
        notification->set_groups_dirty(groups);
        admin_payload = serialize(message);
    }

    // Manager and regular client only care about hosts/workspaces/groups (the others are
    // admin-only).
    QByteArray client_payload;
    if (hosts || workspaces || groups)
    {
        proto::router::RouterToClient message;
        proto::router::Notification* notification = message.mutable_notification();
        notification->set_hosts_dirty(hosts);
        notification->set_workspaces_dirty(workspaces);
        notification->set_groups_dirty(groups);
        client_payload = serialize(message);
    }

    for (Session* session : std::as_const(sessions_))
    {
        switch (session->sessionType())
        {
            case proto::router::SESSION_TYPE_ADMIN:
                session->sendMessage(proto::router::CHANNEL_ID_CLIENT, admin_payload);
                break;

            case proto::router::SESSION_TYPE_MANAGER:
            case proto::router::SESSION_TYPE_CLIENT:
                if (!client_payload.isEmpty())
                    session->sendMessage(proto::router::CHANNEL_ID_CLIENT, client_payload);
                break;

            default:
                break;
        }
    }
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

    SecureByteArray private_key(settings.privateKey());
    if (private_key.isEmpty())
    {
        LOG(INFO) << "The private key is not specified in the configuration file";
        return false;
    }

    QString listen_interface = settings.listenInterface();
    if (!TcpServer::isValidListenInterface(listen_interface))
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
        LOG(INFO) << "Connections from all clients will be allowed";
    else
        LOG(INFO) << "Allowed clients:" << client_white_list_;

    host_white_list_ = settings.hostWhiteList();
    if (host_white_list_.isEmpty())
        LOG(INFO) << "Connections from all hosts will be allowed";
    else
        LOG(INFO) << "Allowed hosts:" << host_white_list_;

    admin_white_list_ = settings.adminWhiteList();
    if (admin_white_list_.isEmpty())
        LOG(INFO) << "Connections from all admins will be allowed";
    else
        LOG(INFO) << "Allowed admins:" << admin_white_list_;

    manager_white_list_ = settings.managerWhiteList();
    if (manager_white_list_.isEmpty())
        LOG(INFO) << "Connections from all managers will be allowed";
    else
        LOG(INFO) << "Allowed managers:" << manager_white_list_;

    relay_white_list_ = settings.relayWhiteList();
    if (relay_white_list_.isEmpty())
        LOG(INFO) << "Connections from all relays will be allowed";
    else
        LOG(INFO) << "Allowed relays:" << relay_white_list_;

    QByteArray seed_key = settings.seedKey();
    if (seed_key.isEmpty())
    {
        LOG(INFO) << "Empty seed key. New key is generated";
        seed_key = Random::byteArray(64);
        settings.setSeedKey(seed_key);
    }

    SharedPointer<UserList> user_list = RouterUserList::open();
    if (!user_list)
    {
        LOG(ERROR) << "Unable to open user list";
        return false;
    }

    user_list->setSeedKey(seed_key);

    // Caps for the router role. The pending cap is intentionally modest - latecomers retry in
    // a moment, and the smaller cap prevents a flood from pinning router resources. The
    // per-address rate is high enough to absorb a thundering herd from one corporate NAT
    // (e.g. hundreds of hosts reconnecting after a network blip) without false positives.
    static constexpr int kRouterMaxPendingConnections = 100;
    static constexpr int kRouterMaxConnectionsPerMinute = 300;

    tcp_server_ = new TcpServer(this);
    connect(tcp_server_, &TcpServer::sig_newConnection, this, &Service::onNewConnection);

    tcp_server_->setPrivateKey(private_key);
    tcp_server_->setUserList(user_list);
    tcp_server_->setAnonymousAccess(
        ServerAuthenticator::AnonymousAccess::ENABLE,
        proto::router::SESSION_TYPE_HOST | proto::router::SESSION_TYPE_RELAY);
    tcp_server_->setMaxPendingConnections(kRouterMaxPendingConnections);
    tcp_server_->setMaxConnectionsPerMinute(kRouterMaxConnectionsPerMinute);
    tcp_server_->start(port, listen_interface);

    tcp_server_legacy_ = new TcpServerLegacy(this);
    connect(tcp_server_legacy_, &TcpServerLegacy::sig_newConnection, this, &Service::onNewLegacyConnection);

    tcp_server_legacy_->setPrivateKey(private_key);
    tcp_server_legacy_->setUserList(user_list);
    tcp_server_legacy_->setAnonymousAccess(
        ServerAuthenticatorLegacy::AnonymousAccess::ENABLE,
        proto::router::SESSION_TYPE_HOST | proto::router::SESSION_TYPE_RELAY);
    tcp_server_legacy_->setMaxPendingConnections(kRouterMaxPendingConnections);
    tcp_server_legacy_->setMaxConnectionsPerMinute(kRouterMaxConnectionsPerMinute);
    tcp_server_legacy_->start(legacy_port, listen_interface);

    if (settings.isStunEnabled())
    {
        stun_server_ = new StunServer(this);
        stun_server_->start(settings.stunPort());
    }

    LOG(INFO) << "Server started";
    return true;
}

//--------------------------------------------------------------------------------------------------
void Service::addSession(TcpChannel* channel, bool is_legacy)
{
    QString address = channel->peerAddress();
    proto::router::SessionType session_type =
        static_cast<proto::router::SessionType>(channel->peerSessionType());

    if (is_legacy && session_type != proto::router::SESSION_TYPE_HOST)
    {
        LOG(ERROR) << "Connection is rejected for" << address;
        channel->deleteLater();
        return;
    }

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

            if (!is_legacy)
            {
                SessionHost* host_session = new SessionHost(channel, this);
                connect(host_session, &SessionHost::sig_hostIdAssigned,
                        this, &Service::onHostIdAssigned);
                session = host_session;
            }
            else
            {
                SessionLegacyHost* legacy_host_session = new SessionLegacyHost(channel, this);
                connect(legacy_host_session, &SessionLegacyHost::sig_hostIdAssigned,
                        this, &Service::onHostIdAssigned);
                session = legacy_host_session;
            }
        }
        break;

        case proto::router::SESSION_TYPE_ADMIN:
        {
            if (!isAddressAllowed(admin_white_list_, address))
                break;
            session = new SessionAdmin(channel, this);
        }
        break;

        case proto::router::SESSION_TYPE_MANAGER:
        {
            if (!isAddressAllowed(manager_white_list_, address))
                break;
            session = new SessionManager(channel, this);
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
