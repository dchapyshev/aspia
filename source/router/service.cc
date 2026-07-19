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

#include <QTimer>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/net/tcp_channel.h"
#include "proto/router_client.h"
#include "router/client.h"
#include "router/client_admin.h"
#include "router/client_manager.h"
#include "router/migration_utils.h"
#include "router/relay.h"
#include "router/host_ng.h"
#include "router/host_legacy.h"
#include "router/settings.h"
#include "router/router_user_list.h"

namespace {

constexpr qsizetype kMaxRelayKeysPerSession = 1000;
constexpr qsizetype kMaxRelayKeysTotal = 10000;

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

    // Sessions can access |instance_|, so we delete the tracked ones before zeroing it.
    for (auto* host : std::as_const(hosts_))
        delete host;

    for (auto* client : std::as_const(clients_))
        delete client;

    for (auto* relay : std::as_const(relays_))
        delete relay;

    instance_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
Service* Service::instance()
{
    return instance_;
}

//--------------------------------------------------------------------------------------------------
// static
void Service::notifyChanged(quint32 flags)
{
    if (!instance_)
        return;

    instance_->dirty_mask_ |= flags;
    if (!instance_->notification_timer_->isActive())
        instance_->notification_timer_->start();
}

//--------------------------------------------------------------------------------------------------
// static
void Service::removeKeysForRelay(qint64 session_id)
{
    if (!instance_)
        return;

    if (!instance_->key_pool_.contains(session_id))
        return;

    LOG(INFO) << "All keys for relay" << session_id << "removed";
    instance_->key_pool_.remove(session_id);
}

//--------------------------------------------------------------------------------------------------
const QList<Host*>& Service::hosts()
{
    return hosts_;
}

//--------------------------------------------------------------------------------------------------
bool Service::stopHost(qint64 session_id)
{
    for (auto it = hosts_.begin(), it_end = hosts_.end(); it != it_end; ++it)
    {
        Host* session = *it;

        if (session->sessionId() == session_id)
        {
            session->disconnect();
            session->deleteLater();
            hosts_.erase(it);
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
const QList<Client*>& Service::clients()
{
    return clients_;
}

//--------------------------------------------------------------------------------------------------
bool Service::stopClient(qint64 client_id)
{
    for (auto it = clients_.begin(), it_end = clients_.end(); it != it_end; ++it)
    {
        Client* client = *it;

        if (client->sessionId() == client_id)
        {
            client->disconnect();
            client->deleteLater();
            clients_.erase(it);
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
int Service::stopClients(qint64 user_id, const QList<qint64>& token_ids, qint64 except_client_id)
{
    QList<qint64> client_ids;
    for (Client* client : std::as_const(clients_))
    {
        if (!client->isTwoFactorCompleted())
            continue;
        if (client->userId() != user_id || client->sessionId() == except_client_id)
            continue;
        if (!token_ids.isEmpty() && !token_ids.contains(client->tokenId()))
            continue;

        client_ids.append(client->sessionId());
    }

    int stopped = 0;
    for (qint64 id : std::as_const(client_ids))
    {
        if (stopClient(id))
        {
            ++stopped;
            LOG(INFO) << "Stopped session" << id << "of user" << user_id;
        }
    }

    return stopped;
}

//--------------------------------------------------------------------------------------------------
const QList<Relay*>& Service::relays()
{
    return relays_;
}

//--------------------------------------------------------------------------------------------------
Relay* Service::relay(qint64 relay_id)
{
    for (auto* relay : std::as_const(relays_))
    {
        if (relay->sessionId() == relay_id)
            return relay;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
bool Service::stopRelay(qint64 relay_id)
{
    for (auto it = relays_.begin(), it_end = relays_.end(); it != it_end; ++it)
    {
        Relay* relay = *it;

        if (relay->sessionId() == relay_id)
        {
            removeKeysForRelay(relay_id);
            relay->disconnect();
            relay->deleteLater();
            relays_.erase(it);
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void Service::addKey(qint64 session_id, const proto::router::RelayKey& key)
{
    auto relay = key_pool_.find(session_id);

    if (relay != key_pool_.end() && relay.value().size() >= kMaxRelayKeysPerSession)
    {
        LOG(WARNING) << "Relay key limit reached for session" << session_id
                     << "key id" << key.key_id() << "will be dropped";
        return;
    }

    qsizetype key_count = 0;
    for (const auto& keys : std::as_const(key_pool_))
        key_count += keys.size();

    if (key_count >= kMaxRelayKeysTotal)
    {
        LOG(WARNING) << "Global relay key limit reached. Key id" << key.key_id()
                     << "from session" << session_id << "will be dropped";
        return;
    }

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

    // Resolve the relay before consuming a key: a one-time key must not leave the pool unless we
    // can actually build an offer from it. On the single-threaded event loop the relay stays valid
    // until this returns, so the caller never faces a missing relay after the key was taken.
    Relay* relay_session = relay(preffered_relay.key());
    if (!relay_session || !relay_session->peerData().has_value())
    {
        LOG(ERROR) << "Preferred relay" << preffered_relay.key() << "is not usable";
        return std::nullopt;
    }

    Credentials credentials;
    credentials.session_id = preffered_relay.key();
    credentials.peer_host = relay_session->peerData()->first;
    credentials.peer_port = relay_session->peerData()->second;
    credentials.key = std::move(keys.back());

    // Removing the key from the pool.
    keys.pop_back();

    if (keys.isEmpty())
    {
        LOG(INFO) << "Last key in the pool for relay. The relay will be removed from the pool";
        key_pool_.remove(preffered_relay.key());
    }

    relay_session->sendKeyUsed(credentials.key.key_id());

    return std::move(credentials);
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
void Service::onNewHostConnection()
{
    CHECK(host_server_);
    while (host_server_->hasReadyConnections())
    {
        TcpChannel* channel = host_server_->nextReadyConnection();
        LOG(INFO) << "New connection:" << channel->peerAddress();
        addHost(channel, false);
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onNewLegacyHostConnection()
{
    CHECK(host_legacy_server_);
    while (host_legacy_server_->hasReadyConnections())
    {
        TcpChannel* channel = host_legacy_server_->nextReadyConnection();
        LOG(INFO) << "New legacy connection:" << channel->peerAddress();
        addHost(channel, true);
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onNewClientConnection()
{
    CHECK(client_server_);
    while (client_server_->hasReadyConnections())
    {
        TcpChannel* channel = client_server_->nextReadyConnection();
        LOG(INFO) << "New client connection:" << channel->peerAddress();
        addClient(channel);
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onNewRelayConnection()
{
    CHECK(relay_server_);
    while (relay_server_->hasReadyConnections())
    {
        TcpChannel* channel = relay_server_->nextReadyConnection();
        LOG(INFO) << "New relay connection:" << channel->peerAddress();
        addRelay(channel);
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onHostFinished()
{
    Host* host = dynamic_cast<Host*>(sender());
    CHECK(host);
    host->disconnect();
    host->deleteLater();
    hosts_.removeOne(host);
}

//--------------------------------------------------------------------------------------------------
void Service::onClientSessionFinished()
{
    Client* client = static_cast<Client*>(sender());
    CHECK(client);
    client->disconnect();
    client->deleteLater();
    clients_.removeOne(client);
}

//--------------------------------------------------------------------------------------------------
void Service::onRelayFinished()
{
    Relay* relay = static_cast<Relay*>(sender());
    CHECK(relay);
    removeKeysForRelay(relay->sessionId());
    relay->disconnect();
    relay->deleteLater();
    relays_.removeOne(relay);
}

//--------------------------------------------------------------------------------------------------
void Service::onHostIdAssigned(HostId host_id)
{
    QList<Host*> matched_hosts;

    for (Host* host : std::as_const(hosts_))
    {
        HostNG* host_ng = dynamic_cast<HostNG*>(host);
        if (host_ng)
        {
            if (host_ng->hostId() == host_id)
                matched_hosts.append(host);
            continue;
        }

        HostLegacy* host_legacy = dynamic_cast<HostLegacy*>(host);
        if (host_legacy)
        {
            if (host_legacy->hasHostId(host_id))
                matched_hosts.append(host);
        }
    }

    if (matched_hosts.size() <= 1)
        return;

    Host* oldest_host = matched_hosts.first();
    for (int i = 1; i < matched_hosts.size(); ++i)
    {
        if (matched_hosts[i]->startTime() < oldest_host->startTime())
            oldest_host = matched_hosts[i];
    }

    LOG(INFO) << "Duplicate host ID" << host_id << "detected. Disconnecting older session (id"
              << oldest_host->sessionId() << ")";

    oldest_host->disconnect();
    oldest_host->deleteLater();
    hosts_.removeOne(oldest_host);
}

//--------------------------------------------------------------------------------------------------
void Service::onNotificationFlush()
{
    if (dirty_mask_ == 0)
        return;

    const bool temp_hosts = (dirty_mask_ & NOTIFY_TEMP_HOSTS) != 0;
    const bool hosts      = (dirty_mask_ & NOTIFY_HOSTS)      != 0;
    const bool relays     = (dirty_mask_ & NOTIFY_RELAYS)     != 0;
    const bool clients    = (dirty_mask_ & NOTIFY_CLIENTS)    != 0;
    const bool users      = (dirty_mask_ & NOTIFY_USERS)      != 0;
    const bool workspaces = (dirty_mask_ & NOTIFY_WORKSPACES) != 0;
    const bool groups     = (dirty_mask_ & NOTIFY_GROUPS)     != 0;
    dirty_mask_ = 0;

    // Admin gets the full bitmask (temp-hosts/hosts/relays/clients/users/workspaces/groups).
    QByteArray admin_payload;
    {
        proto::router::RouterToClient message;
        proto::router::Notification* notification = message.mutable_notification();
        notification->set_temp_hosts_dirty(temp_hosts);
        notification->set_hosts_dirty(hosts);
        notification->set_relays_dirty(relays);
        notification->set_clients_dirty(clients);
        notification->set_users_dirty(users);
        notification->set_workspaces_dirty(workspaces);
        notification->set_groups_dirty(groups);
        admin_payload = serialize(message);
    }

    // Manager and regular client only care about temp-hosts/hosts/workspaces/groups (the others
    // are admin-only).
    QByteArray client_payload;
    if (temp_hosts || hosts || workspaces || groups)
    {
        proto::router::RouterToClient message;
        proto::router::Notification* notification = message.mutable_notification();
        notification->set_temp_hosts_dirty(temp_hosts);
        notification->set_hosts_dirty(hosts);
        notification->set_workspaces_dirty(workspaces);
        notification->set_groups_dirty(groups);
        client_payload = serialize(message);
    }

    // A snapshot: sendMessage() can synchronously finish a failed session, which removes it from
    // |clients_| and would invalidate the iterator.
    const QList<Client*> client_sessions = clients_;
    for (Client* client : client_sessions)
    {
        if (!client->isTwoFactorCompleted())
            continue;

        switch (client->sessionType())
        {
            case proto::router::SESSION_TYPE_ADMIN:
                client->sendMessage(proto::router::CHANNEL_ID_CLIENT, admin_payload);
                break;

            case proto::router::SESSION_TYPE_MANAGER:
            case proto::router::SESSION_TYPE_CLIENT:
                if (!client_payload.isEmpty())
                    client->sendMessage(proto::router::CHANNEL_ID_CLIENT, client_payload);
                break;

            default:
                break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
bool Service::start()
{
    if (host_server_)
    {
        LOG(ERROR) << "Server already is started";
        return false;
    }

    Settings settings;

    SecureByteArray host_private_key(settings.hostPrivateKey());
    if (host_private_key.isEmpty())
    {
        LOG(INFO) << "The host private key is not specified in the configuration file";
        return false;
    }

    SecureByteArray relay_private_key(settings.relayPrivateKey());
    if (relay_private_key.isEmpty())
    {
        LOG(INFO) << "The relay private key is not specified in the configuration file";
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

    quint16 client_port = settings.clientPort();
    if (!client_port)
    {
        LOG(ERROR) << "Invalid client port specified in configuration file";
        return false;
    }

    quint16 relay_port = settings.relayPort();
    if (!relay_port)
    {
        LOG(ERROR) << "Invalid relay port specified in configuration file";
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

    // Hosts are the largest population and reconnect in storms after a network blip (e.g. hundreds
    // of hosts behind one corporate NAT coming back at once), so they get the most generous caps.
    //
    // Relays are a small, stable set of infrastructure servers - a handful at most - so a few
    // slots are plenty and tight caps make a misbehaving or spoofed relay obvious.
    //
    // Clients (admins/managers/clients) are interactive operators: bursty but human-bounded, and
    // each connection runs the heavier SRP + 2FA handshake, so the caps sit in between.
    static constexpr int kHostMaxPendingConnections = 100;
    static constexpr int kHostMaxConnectionsPerMinute = 300;
    static constexpr int kRelayMaxPendingConnections = 10;
    static constexpr int kRelayMaxConnectionsPerMinute = 30;
    static constexpr int kClientMaxPendingConnections = 30;
    static constexpr int kClientMaxConnectionsPerMinute = 60;

    host_server_ = new TcpServer(this);
    connect(host_server_, &TcpServer::sig_newConnection, this, &Service::onNewHostConnection);

    host_server_->setPrivateKey(host_private_key);
    host_server_->setAnonymousAccess(
        ServerAuthenticator::AnonymousAccess::ENABLE, proto::router::SESSION_TYPE_HOST);
    host_server_->setMaxPendingConnections(kHostMaxPendingConnections);
    host_server_->setMaxConnectionsPerMinute(kHostMaxConnectionsPerMinute);
    host_server_->setWhiteList(host_white_list_);
    if (!host_server_->start(port, listen_interface))
    {
        LOG(ERROR) << "Unable to start host listener";
        return false;
    }

    host_legacy_server_ = new TcpServerLegacy(this);
    connect(host_legacy_server_, &TcpServerLegacy::sig_newConnection, this, &Service::onNewLegacyHostConnection);

    host_legacy_server_->setPrivateKey(host_private_key);
    host_legacy_server_->setAnonymousAccess(
        ServerAuthenticatorLegacy::AnonymousAccess::ENABLE, proto::router::SESSION_TYPE_HOST);
    host_legacy_server_->setMaxPendingConnections(kHostMaxPendingConnections);
    host_legacy_server_->setMaxConnectionsPerMinute(kHostMaxConnectionsPerMinute);
    host_legacy_server_->setWhiteList(host_white_list_);
    if (!host_legacy_server_->start(legacy_port, listen_interface))
    {
        LOG(ERROR) << "Unable to start legacy host listener";
        return false;
    }

    // Relay listener accepts relay sessions only (anonymous access).
    relay_server_ = new TcpServer(this);
    connect(relay_server_, &TcpServer::sig_newConnection, this, &Service::onNewRelayConnection);

    relay_server_->setPrivateKey(relay_private_key);
    relay_server_->setAnonymousAccess(
        ServerAuthenticator::AnonymousAccess::ENABLE, proto::router::SESSION_TYPE_RELAY);
    relay_server_->setMaxPendingConnections(kRelayMaxPendingConnections);
    relay_server_->setMaxConnectionsPerMinute(kRelayMaxConnectionsPerMinute);
    relay_server_->setWhiteList(relay_white_list_);
    if (!relay_server_->start(relay_port, listen_interface))
    {
        LOG(ERROR) << "Unable to start relay listener";
        return false;
    }

    // Client listener accepts admin/manager/client sessions only. These session types always
    // authenticate, so anonymous access stays disabled here.
    client_server_ = new TcpServer(this);
    connect(client_server_, &TcpServer::sig_newConnection, this, &Service::onNewClientConnection);

    client_server_->setUserList(user_list);
    client_server_->setMaxPendingConnections(kClientMaxPendingConnections);
    client_server_->setMaxConnectionsPerMinute(kClientMaxConnectionsPerMinute);
    client_server_->setWhiteList(client_white_list_);
    if (!client_server_->start(client_port, listen_interface))
    {
        LOG(ERROR) << "Unable to start client listener";
        return false;
    }

    if (settings.isStunEnabled())
        stun_port_ = settings.stunPort();

    LOG(INFO) << "Server started";
    return true;
}

//--------------------------------------------------------------------------------------------------
void Service::addHost(TcpChannel* channel, bool is_legacy)
{
    QString address = QString::fromStdString(channel->peerAddress());

    LOG(INFO) << "New session:" << address;

    Host* host = nullptr;
    if (!is_legacy)
    {
        HostNG* host_ng = new HostNG(channel, this);
        connect(host_ng, &HostNG::sig_hostIdAssigned, this, &Service::onHostIdAssigned);
        host = host_ng;
    }
    else
    {
        HostLegacy* host_legacy = new HostLegacy(channel, this);
        connect(host_legacy, &HostLegacy::sig_hostIdAssigned, this, &Service::onHostIdAssigned);
        host = host_legacy;
    }

    hosts_.emplace_back(host);
    connect(host, &Host::sig_finished, this, &Service::onHostFinished);
    host->start();
}

//--------------------------------------------------------------------------------------------------
void Service::addClient(TcpChannel* channel)
{
    QString address = QString::fromStdString(channel->peerAddress());
    proto::router::SessionType session_type =
        static_cast<proto::router::SessionType>(channel->peerSessionType());

    LOG(INFO) << "New client session:" << session_type << "(" << address << ")";

    Client* client = nullptr;
    switch (session_type)
    {
        case proto::router::SESSION_TYPE_CLIENT:
            client = new Client(channel, this);
            break;

        case proto::router::SESSION_TYPE_MANAGER:
            client = new ClientManager(channel, this);
            break;

        case proto::router::SESSION_TYPE_ADMIN:
            client = new ClientAdmin(channel, this);
            break;

        default:
            LOG(ERROR) << "Unsupported client session type:" << session_type;
            break;
    }

    if (!client)
    {
        LOG(ERROR) << "Connection is rejected for" << address;
        channel->deleteLater();
        return;
    }

    if (stun_port_)
        client->setStunInfo(stun_port_);

    clients_.emplace_back(client);
    connect(client, &Client::sig_finished, this, &Service::onClientSessionFinished);
    client->start();
}

//--------------------------------------------------------------------------------------------------
void Service::addRelay(TcpChannel* channel)
{
    QString address = QString::fromStdString(channel->peerAddress());

    LOG(INFO) << "New relay session:" << address;

    Relay* relay = new Relay(channel, this);
    relays_.emplace_back(relay);
    connect(relay, &Relay::sig_finished, this, &Service::onRelayFinished);
    relay->start();
}

//--------------------------------------------------------------------------------------------------
// static
Service* Service::instance_ = nullptr;
