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

#include "relay/workers/router_worker.h"

#include <QTimer>

#include "base/logging.h"
#include "base/net/tcp_channel_ng.h"
#include "base/peer/client_authenticator.h"
#include "proto/key_exchange.h"
#include "proto/router.h"
#include "relay/session_key.h"
#include "relay/settings.h"
#include "relay/shared_key_pool.h"
#include "relay/workers/relay_worker.h"

namespace {

const std::chrono::seconds kReconnectTimeout{ 15 };

// The router gave the key to the peers. They are required to use it within this time; otherwise
// the key is removed from the pool.
constexpr std::chrono::seconds kKeyUseTimeout{ 30 };

constexpr quint32 kMaxPeerCount = 1000;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterWorker::RouterWorker()
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
RouterWorker::~RouterWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onStart()
{
    Settings settings;

    // Router settings.
    QString router_address = settings.routerAddress();
    quint16 router_port = settings.routerPort();
    QByteArray router_public_key = settings.routerPublicKey();

    LOG(INFO) << "Router address:" << router_address;
    LOG(INFO) << "Router port:" << router_port;
    LOG(INFO) << "Router public key:" << router_public_key.toHex().toStdString();

    // Peers settings.
    QString peer_address = settings.peerAddress();
    quint16 peer_port = settings.peerPort();
    quint32 max_peer_count = settings.maxPeerCount();

    LOG(INFO) << "Peer address:" << peer_address;
    LOG(INFO) << "Peer port:" << peer_port;
    LOG(INFO) << "Max peer count:" << max_peer_count;

    if (router_address.isEmpty())
    {
        LOG(ERROR) << "Empty router address";
        return;
    }

    if (router_port == 0)
    {
        LOG(ERROR) << "Invalid router port";
        return;
    }

    if (router_public_key.isEmpty())
    {
        LOG(ERROR) << "Empty router public key";
        return;
    }

    if (peer_address.isEmpty())
    {
        LOG(ERROR) << "Empty peer address";
        return;
    }

    if (peer_port == 0)
    {
        LOG(ERROR) << "Invalid peer port";
        return;
    }

    if (!max_peer_count)
    {
        LOG(ERROR) << "Invalid max peer count";
        return;
    }

    if (max_peer_count > kMaxPeerCount)
    {
        LOG(WARNING) << "Max peer count" << max_peer_count
                     << "exceeds limit" << kMaxPeerCount << ". Clamping";
        max_peer_count = kMaxPeerCount;
    }

    router_address_ = router_address;
    router_port_ = router_port;
    router_public_key_ = router_public_key;
    peer_address_ = peer_address;
    peer_port_ = peer_port;
    max_peer_count_ = max_peer_count;

    RelayWorker* relay_worker = findWorker<RelayWorker>();
    CHECK(relay_worker);

    connect(relay_worker, &RelayWorker::sig_sessionStarted,
            this, &RouterWorker::onSessionStarted, Qt::QueuedConnection);
    connect(relay_worker, &RelayWorker::sig_sessionFinished,
            this, &RouterWorker::onSessionFinished, Qt::QueuedConnection);
    connect(relay_worker, &RelayWorker::sig_statistics,
            this, &RouterWorker::onSessionStatistics, Qt::QueuedConnection);
    connect(this, &RouterWorker::sig_disconnectSession,
            relay_worker, &RelayWorker::onDisconnectSession, Qt::QueuedConnection);

    connectToRouter();
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onStop()
{
    tcp_channel_.reset();
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onTcpConnected()
{
    LOG(INFO) << "Connection to the router is established (session count:" << session_count_ << ")";

    // Now the session will receive incoming messages.
    tcp_channel_->setPaused(false);
    refreshKeyPool();
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onTcpErrorOccurred(TcpChannel::ErrorCode error_code)
{
    LOG(INFO) << "Connection to the router has been lost:" << error_code;

    // Clearing the key pool.
    SharedKeyPool::instance().clear();

    if (tcp_channel_)
    {
        tcp_channel_->disconnect();
        tcp_channel_.reset();
    }

    // Retrying a connection at a time interval.
    delayedConnectToRouter();
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onTcpMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    proto::router::RouterToRelay* message =
        incoming_message_.parse<proto::router::RouterToRelay>(buffer);
    if (!message)
    {
        LOG(ERROR) << "Invalid message from router";
        return;
    }

    if (message->has_key_used())
    {
        const quint32 key_id = message->key_used().key_id();
        QTimer::singleShot(kKeyUseTimeout, this, [this, key_id]() { expireKey(key_id); });
    }
    else if (message->has_peer_request())
    {
        const proto::router::PeerRequest& request = message->peer_request();

        if (request.command_name() == "disconnect")
        {
            emit sig_disconnectSession(request.peer_id());
        }
        else
        {
            LOG(ERROR) << "Unsupported peer connection request command:" << request.command_name();
        }
    }
    else
    {
        LOG(ERROR) << "Unhandled message from router";
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onSessionStarted()
{
    ++session_count_;
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onSessionFinished()
{
    --session_count_;

    if (session_count_ < 0)
    {
        LOG(ERROR) << "Invalid value for session count:" << session_count_;
        session_count_ = 0;
    }

    // After disconnecting the peer, one key is released.
    // Add a new key to the pool and send it to the router.
    LOG(INFO) << "Session finished. Add new key to pool";
    refreshKeyPool();
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::onSessionStatistics(const proto::router::RelayStatistics& statistics)
{
    if (!tcp_channel_ || !tcp_channel_->isAuthenticated())
        return;

    outgoing_message_.newMessage<proto::router::RelayToRouter>().mutable_statistics()->CopyFrom(statistics);

    // Send a message to the router.
    tcp_channel_->send(0, outgoing_message_.serialize<proto::router::RelayToRouter>());
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::connectToRouter()
{
    ClientAuthenticator* authenticator = new ClientAuthenticator();
    authenticator->setIdentify(proto::key_exchange::IDENTIFY_ANONYMOUS);
    authenticator->setPeerPublicKey(router_public_key_);
    authenticator->setSessionType(proto::router::SESSION_TYPE_RELAY);

    tcp_channel_ = new TcpChannelNG(authenticator, this);

    connect(tcp_channel_, &TcpChannel::sig_authenticated, this, &RouterWorker::onTcpConnected);
    connect(tcp_channel_, &TcpChannel::sig_errorOccurred, this, &RouterWorker::onTcpErrorOccurred);
    connect(tcp_channel_, &TcpChannel::sig_messageReceived, this, &RouterWorker::onTcpMessageReceived);

    LOG(INFO) << "Connecting to router...";
    tcp_channel_->connectTo(router_address_, router_port_);
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::delayedConnectToRouter()
{
    LOG(INFO) << "Reconnect after" << kReconnectTimeout.count() << "seconds";
    QTimer::singleShot(kReconnectTimeout, this, [this]() { connectToRouter(); });
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::expireKey(quint32 key_id)
{
    // The key was consumed by the peers in time or the pool has been cleared since.
    if (!SharedKeyPool::instance().remove(key_id))
        return;

    // The key has expired and has been removed from the pool.
    // Add a new key to the pool and send it to the router.
    LOG(INFO) << "Key with id" << key_id << "has expired and removed from pool";
    refreshKeyPool();
}

//--------------------------------------------------------------------------------------------------
void RouterWorker::refreshKeyPool()
{
    if (!tcp_channel_ || !tcp_channel_->isAuthenticated())
    {
        LOG(INFO) << "No router connection; skip sending key pool";
        return;
    }

    // Every active session occupies one slot of the configured capacity; unused keys already in
    // the pool occupy their slots too. Top up the pool with the remaining capacity.
    const size_t pool_size = SharedKeyPool::instance().count();
    const size_t busy = static_cast<size_t>(session_count_) + pool_size;
    if (busy >= max_peer_count_)
    {
        LOG(INFO) << "No free capacity for new keys (active:" << session_count_
                  << "pool:" << pool_size << "max:" << max_peer_count_ << ")";
        return;
    }

    const quint32 key_count = max_peer_count_ - static_cast<quint32>(busy);

    proto::router::RelayKeyPool* relay_key_pool =
        outgoing_message_.newMessage<proto::router::RelayToRouter>().mutable_key_pool();

    relay_key_pool->set_peer_host(peer_address_.toStdString());
    relay_key_pool->set_peer_port(peer_port_);

    // Add the requested number of keys to the pool.
    for (quint32 i = 0; i < key_count; ++i)
    {
        SessionKey session_key = SessionKey::create();
        if (!session_key.isValid())
        {
            LOG(ERROR) << "Unable to create session key (i=" << i << ")";
            break;
        }

        // Add the key to the outgoing message.
        proto::router::RelayKey* key = relay_key_pool->add_key();

        // The encryption algorithm is algorithm-agnostic for the key material and is selected per
        // connection by the router, so it is not set here.
        key->set_type(proto::router::RelayKey::TYPE_X25519);
        key->set_public_key(session_key.publicKey().toStdString());
        key->set_iv(session_key.iv().toStdString());

        // Add the key to the pool.
        key->set_key_id(SharedKeyPool::instance().add(std::move(session_key)));
    }

    if (!relay_key_pool->key_size())
        return;

    // Send a message to the router.
    tcp_channel_->send(0, outgoing_message_.serialize<proto::router::RelayToRouter>());
}
