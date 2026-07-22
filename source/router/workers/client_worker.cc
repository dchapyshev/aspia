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

#include "router/workers/client_worker.h"

#include <QUuid>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/random.h"
#include "base/net/tcp_channel.h"
#include "base/net/tcp_server.h"
#include "base/threading/worker.h"
#include "proto/router.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "router/client.h"
#include "router/client_admin.h"
#include "router/client_manager.h"
#include "router/router_user_list.h"
#include "router/settings.h"
#include "router/workers/host_worker.h"
#include "router/workers/relay_worker.h"

namespace {

// Accumulated NOTIFY_* bits are flushed to the connected sessions at this interval.
const Worker::Seconds kNotifyInterval{ 5 };

} // namespace

//--------------------------------------------------------------------------------------------------
ClientWorker::ClientWorker()
    : Worker(Thread::AsioDispatcher, Seconds(1))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientWorker::~ClientWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientWorker::onStart()
{
    HostWorker* host_worker = findWorker<HostWorker>();
    CHECK(host_worker);
    connect(host_worker, &HostWorker::sig_notify, this,
            &ClientWorker::onNotifyChanged, Qt::QueuedConnection);

    RelayWorker* relay_worker = findWorker<RelayWorker>();
    CHECK(relay_worker);
    connect(relay_worker, &RelayWorker::sig_relaysChanged, this,
            [this]() { onNotifyChanged(NOTIFY_RELAYS); }, Qt::QueuedConnection);

    Settings settings;

    QString listen_interface = settings.listenInterface();
    if (!TcpServer::isValidListenInterface(listen_interface))
    {
        LOG(ERROR) << "Invalid listen interface address";
        return;
    }

    quint16 port = settings.clientPort();
    if (!port)
    {
        LOG(ERROR) << "Invalid client port specified in configuration file";
        return;
    }

    QStringList white_list = settings.clientWhiteList();
    if (white_list.isEmpty())
        LOG(INFO) << "Connections from all clients will be allowed";
    else
        LOG(INFO) << "Allowed clients:" << white_list;

    QByteArray seed_key = settings.seedKey();
    QString router_guid = settings.routerGuid();

    if (seed_key.isEmpty() || router_guid.isEmpty())
    {
        if (seed_key.isEmpty())
        {
            LOG(INFO) << "Seed key is not set; generating a new one";
            settings.setSeedKey(Random::byteArray(64));
        }

        if (router_guid.isEmpty())
        {
            LOG(INFO) << "Router GUID is not set; generating a new one";
            settings.setRouterGuid(QUuid::createUuid().toString(QUuid::WithoutBraces));
        }

        settings.sync();

        // Re-read from disk to confirm the values were actually persisted.
        Settings written;
        seed_key = written.seedKey();
        router_guid = written.routerGuid();

        if (seed_key.isEmpty() || router_guid.isEmpty())
        {
            LOG(ERROR) << "Unable to write the seed key / router GUID to the configuration";
            return;
        }
    }

    router_guid_ = router_guid;

    SharedPointer<UserList> user_list = RouterUserList::open();
    if (!user_list)
    {
        LOG(ERROR) << "Unable to open user list";
        return;
    }

    user_list->setSeedKey(seed_key);

    if (settings.isStunEnabled())
        stun_port_ = settings.stunPort();

    // Clients (admins/managers/clients) are interactive operators: bursty but human-bounded, and
    // each connection runs the heavier SRP + 2FA handshake, so the caps sit in between the host
    // and relay listeners of the sibling workers.
    static constexpr int kMaxPendingConnections = 30;
    static constexpr int kMaxConnectionsPerMinute = 60;

    // Client listener accepts admin/manager/client sessions only. These session types always
    // authenticate, so anonymous access stays disabled here.
    server_ = new TcpServer();
    connect(server_, &TcpServer::sig_newConnection, this, &ClientWorker::onNewConnection);

    server_->setUserList(user_list);
    server_->setMaxPendingConnections(kMaxPendingConnections);
    server_->setMaxConnectionsPerMinute(kMaxConnectionsPerMinute);
    server_->setWhiteList(white_list);
    if (!server_->start(port, listen_interface))
    {
        LOG(ERROR) << "Unable to start client listener";
        server_.reset();
    }
}

//--------------------------------------------------------------------------------------------------
void ClientWorker::onStop()
{
    server_.reset();

    for (auto* client : std::as_const(clients_))
    {
        client->disconnect();
        delete client;
    }

    clients_.clear();
}

//--------------------------------------------------------------------------------------------------
void ClientWorker::onTimer(const TimePoint& now)
{
    if (now < next_notify_time_)
        return;
    next_notify_time_ = now + kNotifyInterval;

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
void ClientWorker::onNewConnection()
{
    CHECK(server_);
    while (server_->hasReadyConnections())
    {
        TcpChannel* channel = server_->nextReadyConnection();

        proto::router::SessionType session_type =
            static_cast<proto::router::SessionType>(channel->peerSessionType());

        LOG(INFO) << "New client session:" << session_type << "(" << channel->peerAddress() << ")";

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
            {
                ClientAdmin* admin = new ClientAdmin(channel, this);
                connect(admin, &ClientAdmin::sig_clientListRequest,
                        this, &ClientWorker::onClientListRequest);
                connect(admin, &ClientAdmin::sig_clientRequest,
                        this, &ClientWorker::onClientRequest);
                client = admin;
                break;
            }

            default:
                LOG(ERROR) << "Unsupported client session type:" << session_type;
                break;
        }

        if (!client)
        {
            LOG(ERROR) << "Connection is rejected for" << channel->peerAddress();
            channel->deleteLater();
            continue;
        }

        if (stun_port_)
            client->setStunInfo(stun_port_);
        client->setRouterGuid(router_guid_.toStdString());

        clients_.emplace_back(client);
        connect(client, &Client::sig_finished, this, &ClientWorker::onSessionFinished);
        connect(client, &Client::sig_notifyChanged, this, &ClientWorker::onNotifyChanged);
        connect(client, &Client::sig_stopClients, this, &ClientWorker::onStopClients);
        client->start();
    }
}

//--------------------------------------------------------------------------------------------------
void ClientWorker::onSessionFinished()
{
    Client* client = static_cast<Client*>(sender());
    CHECK(client);
    client->disconnect();
    client->deleteLater();
    clients_.removeOne(client);

    onNotifyChanged(NOTIFY_CLIENTS);
}

//--------------------------------------------------------------------------------------------------
void ClientWorker::onNotifyChanged(quint32 flags)
{
    // The accumulated mask is flushed to the connected sessions by the periodic worker timer.
    dirty_mask_ |= flags;
}

//--------------------------------------------------------------------------------------------------
void ClientWorker::onStopClients(qint64 user_id, const QList<qint64>& token_ids, qint64 except_client_id)
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

    for (qint64 id : std::as_const(client_ids))
    {
        if (stopClient(id))
            LOG(INFO) << "Stopped session" << id << "of user" << user_id;
    }
}

//--------------------------------------------------------------------------------------------------
void ClientWorker::onClientListRequest(const proto::router::ClientListRequest& request)
{
    Client* session = static_cast<Client*>(sender());
    CHECK(session);

    proto::router::RouterToAdmin message;
    proto::router::ClientList* result = message.mutable_client_list();
    result->set_request_id(request.request_id());
    result->set_error_code(proto::router::kErrorOk);

    for (const auto& client : std::as_const(clients_))
    {
        proto::router::ClientInfo* item = result->add_client();

        item->set_entry_id(client->sessionId());
        item->set_timepoint(client->startTime());
        item->set_ip_address(client->address());
        item->mutable_version()->CopyFrom(serialize(client->version()));
        item->set_os_name(client->osName());
        item->set_computer_name(client->computerName());
        item->set_architecture(client->architecture());
    }

    session->sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientWorker::onClientRequest(const proto::router::ClientRequest& request)
{
    Client* session = static_cast<Client*>(sender());
    CHECK(session);

    proto::router::RouterToAdmin message;
    proto::router::ClientResult* client_result = message.mutable_client_result();
    client_result->set_request_id(request.request_id());
    client_result->set_command_name(request.command_name());

    if (request.command_name() == proto::router::kCommandClientDisconnect)
    {
        const qint64 entry_id = request.entry_id();

        if (entry_id == -1)
        {
            QList<qint64> client_ids;
            for (const auto& client : std::as_const(clients_))
                client_ids.append(client->sessionId());

            bool all_ok = true;
            for (qint64 id : std::as_const(client_ids))
            {
                if (!stopClient(id))
                {
                    LOG(ERROR) << "Failed to stop client session:" << id;
                    all_ok = false;
                }
            }

            if (all_ok)
            {
                LOG(INFO) << "All client sessions disconnected by" << session->userName();
                client_result->set_error_code(proto::router::kErrorOk);
            }
            else
            {
                client_result->set_error_code(proto::router::kErrorInternalError);
            }
        }
        else if (!stopClient(entry_id))
        {
            LOG(ERROR) << "Session not found:" << entry_id;
            client_result->set_error_code(proto::router::kErrorInvalidEntryId);
        }
        else
        {
            LOG(INFO) << "Client session" << entry_id << "disconnected by" << session->userName();
            client_result->set_error_code(proto::router::kErrorOk);
        }
    }
    else
    {
        LOG(ERROR) << "Unknown client request command:" << request.command_name();
        client_result->set_error_code(proto::router::kErrorInvalidRequest);
    }

    session->sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
bool ClientWorker::stopClient(qint64 client_id)
{
    for (auto it = clients_.begin(), it_end = clients_.end(); it != it_end; ++it)
    {
        Client* client = *it;

        if (client->sessionId() == client_id)
        {
            client->disconnect();
            client->deleteLater();
            clients_.erase(it);

            onNotifyChanged(NOTIFY_CLIENTS);
            return true;
        }
    }

    return false;
}
