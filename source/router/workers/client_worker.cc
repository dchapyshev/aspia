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

#include <algorithm>

#include "base/logging.h"
#include "base/scoped_qpointer.h"
#include "base/serialization.h"
#include "base/crypto/random.h"
#include "base/net/net_utils.h"
#include "base/net/tcp_channel.h"
#include "base/net/tcp_server.h"
#include "base/threading/worker.h"
#include "proto/router.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "router/client_admin.h"
#include "router/client_manager.h"
#include "router/client_operator.h"
#include "router/database.h"
#include "router/router_user_list.h"
#include "router/settings.h"
#include "router/workers/host_worker.h"
#include "router/workers/relay_worker.h"

namespace {

// Accumulated NOTIFY_* bits are flushed to the connected sessions at this interval.
const Seconds kNotifyInterval{ 5 };

// How long a session may sit at the two-factor stage.
const Minutes kTwoFactorStageTimeout{ 2 };

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
    connect(this, &ClientWorker::sig_clientsChanged,
            relay_worker, &RelayWorker::onClientsChanged, Qt::QueuedConnection);

    Settings settings;

    QString listen_interface = settings.listenInterface();
    if (!NetUtils::isValidListenInterface(listen_interface))
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

    if (seed_key.isEmpty())
    {
        LOG(INFO) << "Seed key is not set; generating a new one";
        settings.setSeedKey(Random::byteArray(64));
        settings.sync();

        // Re-read from disk to confirm the value was actually persisted.
        seed_key = Settings().seedKey();

        if (seed_key.isEmpty())
        {
            LOG(ERROR) << "Unable to write the seed key to the configuration";
            return;
        }
    }

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

    // The listener accepts operators, managers and administrators only. These session types always
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
void ClientWorker::onTimer(TimePoint now)
{
    std::vector<qint64> expired;
    for (ClientOperator* client : std::as_const(clients_))
    {
        if (client->isTwoFactorCompleted() || now - client->startTime() < kTwoFactorStageTimeout)
            continue;

        expired.emplace_back(client->sessionId());
    }

    for (qint64 client_id : std::as_const(expired))
    {
        if (stopClient(client_id))
            LOG(INFO) << "Session" << client_id << "left at the two-factor stage. Stopped";
    }

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
    const std::vector<ClientOperator*> client_sessions = clients_;
    for (ClientOperator* client : client_sessions)
    {
        if (!client->isTwoFactorCompleted())
            continue;

        switch (client->sessionType())
        {
            case proto::router::SESSION_TYPE_ADMIN:
                client->sendMessage(proto::router::CHANNEL_ID_CLIENT, admin_payload);
                break;

            case proto::router::SESSION_TYPE_MANAGER:
            case proto::router::SESSION_TYPE_OPERATOR:
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
        ScopedQPointer<TcpChannel> channel(server_->nextReadyConnection());

        proto::router::SessionType session_type =
            static_cast<proto::router::SessionType>(channel->peerSessionType());

        LOG(INFO) << "New client session:" << session_type << "(" << channel->peerAddress() << ")";

        if (isUserLimitReached(channel->peerUserId()))
        {
            LOG(ERROR) << "Too many connections for user" << channel->peerUserName() << ". Rejected";
            continue;
        }

        ClientOperator* client = nullptr;
        switch (session_type)
        {
            case proto::router::SESSION_TYPE_OPERATOR:
                client = new ClientOperator(Database::instance(), channel.release(), this);
                break;

            case proto::router::SESSION_TYPE_MANAGER:
                client = new ClientManager(Database::instance(), channel.release(), this);
                break;

            case proto::router::SESSION_TYPE_ADMIN:
            {
                ClientAdmin* admin = new ClientAdmin(Database::instance(), channel.release(), this);
                connect(admin, &ClientAdmin::sig_clientListRequest, this, &ClientWorker::onClientListRequest);
                connect(admin, &ClientAdmin::sig_clientRequest, this, &ClientWorker::onClientRequest);
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
            continue;
        }

        if (stun_port_)
            client->setStunInfo(stun_port_);

        clients_.emplace_back(client);
        connect(client, &ClientOperator::sig_twoFactorCompleted, this, &ClientWorker::updateClientsMask);
        connect(client, &ClientOperator::sig_finished, this, &ClientWorker::onSessionFinished);
        connect(client, &ClientOperator::sig_notifyChanged, this, &ClientWorker::onNotifyChanged);
        connect(client, &ClientOperator::sig_stopClients, this, &ClientWorker::onStopClients);
        client->start();

        updateClientsMask();
    }
}

//--------------------------------------------------------------------------------------------------
void ClientWorker::onSessionFinished(qint64 session_id)
{
    stopClient(session_id);
}

//--------------------------------------------------------------------------------------------------
void ClientWorker::onNotifyChanged(quint32 flags)
{
    // The accumulated mask is flushed to the connected sessions by the periodic worker timer.
    dirty_mask_ |= flags;
}

//--------------------------------------------------------------------------------------------------
void ClientWorker::onStopClients(qint64 user_id, const std::vector<qint64>& token_ids)
{
    std::vector<qint64> client_ids;
    for (ClientOperator* client : std::as_const(clients_))
    {
        if (client->userId() != user_id)
            continue;

        // Sessions still at the 2FA stage are included. The drop is by user, not by what
        // exactly the command revoked, and a session that has not passed the stage yet loses
        // nothing but the SRP round it repeats on reconnect. A revocation of specific tokens
        // skips them for free, because their token id is still 0 and token ids are validated
        // as positive before they reach here.
        if (!token_ids.empty() && std::ranges::find(token_ids, client->tokenId()) == token_ids.end())
            continue;

        client_ids.emplace_back(client->sessionId());
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
    ClientOperator* session = static_cast<ClientOperator*>(sender());
    CHECK(session);

    proto::router::RouterToAdmin message;
    proto::router::ClientList* result = message.mutable_client_list();
    result->set_request_id(request.request_id());

    const qint64 offset = request.offset();
    const qint64 count = request.count();

    if (offset < 0 || count < 1 || count > proto::router::kMaxClientPageSize)
    {
        LOG(ERROR) << "Invalid client list page: offset" << offset << "count" << count;
        result->set_error_code(proto::router::kErrorInvalidRequest);
        session->sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
        return;
    }

    // The clients are kept in the order they arrived. The page has to name the same records
    // every time it is asked for, so the order is fixed here.
    std::vector<ClientOperator*> clients(clients_.begin(), clients_.end());
    std::sort(clients.begin(), clients.end(), [](const ClientOperator* a, const ClientOperator* b)
    {
        return a->sessionId() < b->sessionId();
    });

    result->set_error_code(proto::router::kErrorOk);
    result->set_total_count(static_cast<qint64>(clients.size()));

    const qint64 wall_now = secondsSinceEpoch();
    const TimePoint monotonic_now = Clock::now();

    const size_t begin = std::min(static_cast<size_t>(offset), clients.size());
    const size_t end = std::min(begin + static_cast<size_t>(count), clients.size());

    for (size_t i = begin; i < end; ++i)
    {
        const ClientOperator* client = clients[i];

        proto::router::ClientInfo* item = result->add_client();
        item->set_entry_id(client->sessionId());
        item->set_timepoint(
            wall_now - DurationCast<Seconds>(monotonic_now - client->startTime()).count());
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
    ClientOperator* session = static_cast<ClientOperator*>(sender());
    CHECK(session);

    proto::router::RouterToAdmin message;
    proto::router::ClientResult* client_result = message.mutable_client_result();
    client_result->set_request_id(request.request_id());
    client_result->set_command_name(request.command_name());

    if (request.command_name() == proto::router::kCommandClientDisconnect)
    {
        const qint64 entry_id = request.entry_id();

        std::vector<qint64> session_ids;
        session_ids.reserve(clients_.size());
        for (const auto& client : std::as_const(clients_))
            session_ids.emplace_back(client->sessionId());

        const std::vector<qint64> targets =
            sessionsToStop(session_ids, entry_id, session->sessionId());

        if (targets.empty() && entry_id != -1)
        {
            LOG(ERROR) << "Session not found:" << entry_id;
            client_result->set_error_code(proto::router::kErrorInvalidEntryId);
        }
        else
        {
            bool all_ok = true;
            for (qint64 id : std::as_const(targets))
            {
                if (!stopClient(id))
                {
                    LOG(ERROR) << "Failed to stop client session:" << id;
                    all_ok = false;
                }
            }

            if (!all_ok)
            {
                client_result->set_error_code(proto::router::kErrorInternalError);
            }
            else
            {
                if (entry_id == -1)
                {
                    LOG(INFO) << "All client sessions disconnected by" << session->userName();
                }
                else
                {
                    LOG(INFO) << "ClientOperator session" << entry_id << "disconnected by"
                              << session->userName();
                }

                client_result->set_error_code(proto::router::kErrorOk);
            }
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
// static
std::vector<qint64> ClientWorker::sessionsToStop(const std::vector<qint64>& session_ids,
                                                 qint64 entry_id, qint64 requesting_session_id)
{
    std::vector<qint64> targets;

    if (entry_id == -1)
    {
        targets.reserve(session_ids.size());

        for (qint64 session_id : session_ids)
        {
            if (session_id != requesting_session_id)
                targets.emplace_back(session_id);
        }

        return targets;
    }

    if (std::ranges::find(session_ids, entry_id) != session_ids.end())
        targets.emplace_back(entry_id);

    return targets;
}

//--------------------------------------------------------------------------------------------------
bool ClientWorker::isUserLimitReached(qint64 user_id) const
{
    const auto count = std::ranges::count_if(clients_, [user_id](const ClientOperator* client)
    {
        return client->userId() == user_id;
    });

    return static_cast<size_t>(count) >= kMaxClientsPerUser;
}

//--------------------------------------------------------------------------------------------------
bool ClientWorker::stopClient(qint64 client_id)
{
    for (auto it = clients_.begin(), it_end = clients_.end(); it != it_end; ++it)
    {
        ClientOperator* client = *it;

        if (client->sessionId() == client_id)
        {
            client->stop();
            client->deleteLater();
            clients_.erase(it);

            updateClientsMask();
            onNotifyChanged(NOTIFY_CLIENTS);
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void ClientWorker::updateClientsMask()
{
    quint32 clients_mask = 0;

    for (const ClientOperator* client : std::as_const(clients_))
    {
        // Sessions still at the two-factor stage do not count. The mask switches the relay
        // statistics polling, and nothing of it can be seen before the stage is passed.
        if (!client->isTwoFactorCompleted())
            continue;

        switch (client->sessionType())
        {
            case proto::router::SESSION_TYPE_OPERATOR:
                clients_mask |= CLIENT_OPERATORS;
                break;

            case proto::router::SESSION_TYPE_MANAGER:
                clients_mask |= CLIENT_MANAGERS;
                break;

            case proto::router::SESSION_TYPE_ADMIN:
                clients_mask |= CLIENT_ADMINS;
                break;

            default:
                break;
        }
    }

    if (clients_mask == clients_mask_)
        return;

    clients_mask_ = clients_mask;
    emit sig_clientsChanged(clients_mask_);
}
