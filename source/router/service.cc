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

#include "base/core_application.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/random.h"
#include "base/net/tcp_channel.h"
#include "proto/router.h"
#include "proto/router_client.h"
#include "router/client.h"
#include "router/client_admin.h"
#include "router/client_manager.h"
#include "router/migration_utils.h"
#include "router/settings.h"
#include "router/router_user_list.h"
#include "router/workers/host_worker.h"
#include "router/workers/relay_worker.h"

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
    for (auto* client : std::as_const(clients_))
        delete client;

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
void Service::onClientSessionFinished()
{
    Client* client = static_cast<Client*>(sender());
    CHECK(client);
    client->disconnect();
    client->deleteLater();
    clients_.removeOne(client);
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
    if (client_server_)
    {
        LOG(ERROR) << "Server already is started";
        return false;
    }

    Settings settings;

    QString listen_interface = settings.listenInterface();
    if (!TcpServer::isValidListenInterface(listen_interface))
    {
        LOG(ERROR) << "Invalid listen interface address";
        return false;
    }

    quint16 client_port = settings.clientPort();
    if (!client_port)
    {
        LOG(ERROR) << "Invalid client port specified in configuration file";
        return false;
    }

    client_white_list_ = settings.clientWhiteList();
    if (client_white_list_.isEmpty())
        LOG(INFO) << "Connections from all clients will be allowed";
    else
        LOG(INFO) << "Allowed clients:" << client_white_list_;

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

    // Clients (admins/managers/clients) are interactive operators: bursty but human-bounded, and
    // each connection runs the heavier SRP + 2FA handshake, so the caps sit in between the host
    // and relay listeners of the workers.
    static constexpr int kClientMaxPendingConnections = 30;
    static constexpr int kClientMaxConnectionsPerMinute = 60;

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

    HostWorker* host_worker = CoreApplication::findWorker<HostWorker>();
    CHECK(host_worker);
    connect(host_worker, &HostWorker::sig_notify, this,
            [](quint32 flags) { notifyChanged(flags); }, Qt::QueuedConnection);

    RelayWorker* relay_worker = CoreApplication::findWorker<RelayWorker>();
    CHECK(relay_worker);
    connect(relay_worker, &RelayWorker::sig_relaysChanged, this, []()
    {
        notifyChanged(NOTIFY_RELAYS);
    }, Qt::QueuedConnection);

    LOG(INFO) << "Server started";
    return true;
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
// static
Service* Service::instance_ = nullptr;
