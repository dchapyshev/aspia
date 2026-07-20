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

#include "router/workers/relay_worker.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/secure_byte_array.h"
#include "base/net/tcp_channel.h"
#include "base/net/tcp_server.h"
#include "router/relay.h"
#include "router/settings.h"
#include "router/shared_key_pool.h"

//--------------------------------------------------------------------------------------------------
RelayWorker::RelayWorker()
    : Worker(Thread::AsioDispatcher, Seconds(1))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
RelayWorker::~RelayWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::requestRelayList(QObject* context, RelayListCallback callback)
{
    request(context, [this]() { return doRelayList(); }, std::move(callback));
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::stopRelay(qint64 relay_id, QObject* context, ResultCallback callback)
{
    request(context, [this, relay_id]() { return doStopRelay(relay_id); }, std::move(callback));
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::disconnectPeerSession(qint64 relay_id, const proto::router::PeerRequest& request,
                                        QObject* context, ResultCallback callback)
{
    Worker::request(context, [this, relay_id, request]()
    {
        Relay* relay = relayById(relay_id);
        if (!relay)
            return false;

        relay->disconnectPeerSession(request);
        return true;
    },
    std::move(callback));
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::notifyKeyUsed(qint64 session_id, quint32 key_id)
{
    post([this, session_id, key_id]()
    {
        if (Relay* relay = relayById(session_id))
            relay->sendKeyUsed(key_id);
    });
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::onStart()
{
    Settings settings;

    SecureByteArray private_key(settings.relayPrivateKey());
    if (private_key.isEmpty())
    {
        LOG(ERROR) << "The relay private key is not specified in the configuration file";
        return;
    }

    QString listen_interface = settings.listenInterface();
    if (!TcpServer::isValidListenInterface(listen_interface))
    {
        LOG(ERROR) << "Invalid listen interface address";
        return;
    }

    quint16 port = settings.relayPort();
    if (!port)
    {
        LOG(ERROR) << "Invalid relay port specified in configuration file";
        return;
    }

    QStringList white_list = settings.relayWhiteList();
    if (white_list.isEmpty())
        LOG(INFO) << "Connections from all relays will be allowed";
    else
        LOG(INFO) << "Allowed relays:" << white_list;

    // Relays are a small, stable set of infrastructure servers - a handful at most - so a few
    // slots are plenty and tight caps make a misbehaving or spoofed relay obvious.
    static constexpr int kMaxPendingConnections = 10;
    static constexpr int kMaxConnectionsPerMinute = 30;

    // Relay listener accepts relay sessions only (anonymous access).
    server_ = new TcpServer();
    connect(server_, &TcpServer::sig_newConnection, this, &RelayWorker::onNewRelayConnection);

    server_->setPrivateKey(private_key);
    server_->setAnonymousAccess(
        ServerAuthenticator::AnonymousAccess::ENABLE, proto::router::SESSION_TYPE_RELAY);
    server_->setMaxPendingConnections(kMaxPendingConnections);
    server_->setMaxConnectionsPerMinute(kMaxConnectionsPerMinute);
    server_->setWhiteList(white_list);
    if (!server_->start(port, listen_interface))
    {
        LOG(ERROR) << "Unable to start relay listener";
        server_.reset();
    }
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::onStop()
{
    server_.reset();

    for (auto* relay : std::as_const(relays_))
    {
        relay->disconnect();
        delete relay;
    }

    relays_.clear();
    SharedKeyPool::instance().clear();
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::onNewRelayConnection()
{
    CHECK(server_);
    while (server_->hasReadyConnections())
    {
        TcpChannel* channel = server_->nextReadyConnection();
        LOG(INFO) << "New relay session:" << channel->peerAddress();

        Relay* relay = new Relay(channel, this);
        relays_.emplace_back(relay);

        connect(relay, &Relay::sig_finished, this, &RelayWorker::onRelayFinished);

        relay->start();
        emit sig_relaysChanged();
    }
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::onRelayFinished()
{
    Relay* relay = static_cast<Relay*>(sender());
    CHECK(relay);
    removeRelay(relay);
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::removeRelay(Relay* relay)
{
    const qint64 session_id = relay->sessionId();

    relay->disconnect();
    relay->deleteLater();
    relays_.removeOne(relay);

    SharedKeyPool::instance().remove(session_id);

    emit sig_relaysChanged();
}

//--------------------------------------------------------------------------------------------------
Relay* RelayWorker::relayById(qint64 session_id)
{
    for (auto* relay : std::as_const(relays_))
    {
        if (relay->sessionId() == session_id)
            return relay;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
proto::router::RelayList RelayWorker::doRelayList() const
{
    proto::router::RelayList relay_list;

    for (const auto* relay : std::as_const(relays_))
    {
        proto::router::RelayInfo* item = relay_list.add_relay();

        // Generic session info.
        item->set_entry_id(relay->sessionId());
        item->set_timepoint(relay->startTime());
        item->set_ip_address(relay->address());
        item->mutable_version()->CopyFrom(serialize(relay->version()));
        item->set_os_name(relay->osName());
        item->set_computer_name(relay->computerName());
        item->set_architecture(relay->architecture());

        // Statistics info.
        const std::optional<proto::router::RelayStatistics>& statistics = relay->statistics();
        if (statistics.has_value())
        {
            item->mutable_statistics()->mutable_peer()->CopyFrom(statistics->peer());
            item->mutable_statistics()->set_uptime(statistics->uptime());
        }

        // Other info.
        item->set_pool_size(SharedKeyPool::instance().count(relay->sessionId()));
    }

    return relay_list;
}

//--------------------------------------------------------------------------------------------------
bool RelayWorker::doStopRelay(qint64 relay_id)
{
    if (relay_id == -1)
    {
        while (!relays_.isEmpty())
            removeRelay(relays_.first());
        return true;
    }

    Relay* relay = relayById(relay_id);
    if (!relay)
        return false;

    removeRelay(relay);
    return true;
}

