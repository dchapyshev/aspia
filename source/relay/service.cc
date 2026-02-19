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

#include "relay/service.h"

#include "base/logging.h"
#include "base/net/tcp_server.h"
#include "base/peer/client_authenticator.h"
#include "relay/migration_utils.h"
#include "relay/service_constants.h"
#include "relay/settings.h"
#include "proto/router.h"

namespace relay {

namespace {

const std::chrono::seconds kReconnectTimeout{ 15 };

class KeyDeleter
{
public:
    KeyDeleter(std::shared_ptr<KeyPool> pool, quint32 key_id)
        : pool_(std::move(pool)),
        key_id_(key_id)
    {
        DCHECK(pool_);
    }

    ~KeyDeleter() = default;

    void deleteKey()
    {
        pool_->setKeyExpired(key_id_);
    }

private:
    std::shared_ptr<KeyPool> pool_;
    const quint32 key_id_;

    Q_DISABLE_COPY(KeyDeleter)
};

} // namespace

//--------------------------------------------------------------------------------------------------
Service::Service(QObject* parent)
    : base::Service(kServiceName, parent),
      reconnect_timer_(new QTimer(this)),
      key_factory_(new KeyFactory(this))
{
    LOG(INFO) << "Ctor";

    connect(key_factory_, &KeyFactory::sig_keyExpired,
            this, &Service::onPoolKeyExpired,
            Qt::QueuedConnection);

    reconnect_timer_->setSingleShot(true);
    connect(reconnect_timer_, &QTimer::timeout, this, &Service::connectToRouter);

    Settings settings;

    // Router settings.
    router_address_ = settings.routerAddress();
    router_port_ = settings.routerPort();
    router_public_key_ = settings.routerPublicKey();

    LOG(INFO) << "Router address:" << router_address_;
    LOG(INFO) << "Router port:" << router_port_;
    LOG(INFO) << "Router public key:" << router_public_key_.toHex().toStdString();

    // Peers settings.
    listen_interface_ = settings.listenInterface();
    peer_address_ = settings.peerAddress();
    peer_port_ = settings.peerPort();
    peer_idle_timeout_ = settings.peerIdleTimeout();
    max_peer_count_ = settings.maxPeerCount();
    statistics_enabled_ = settings.isStatisticsEnabled();
    statistics_interval_ = settings.statisticsInterval();

    LOG(INFO) << "Listen interface:" << listen_interface_;
    LOG(INFO) << "Peer address:" << peer_address_;
    LOG(INFO) << "Peer port:" << peer_port_;
    LOG(INFO) << "Peer idle timeout:" << peer_idle_timeout_.count();
    LOG(INFO) << "Max peer count:" << max_peer_count_;
    LOG(INFO) << "Statistics enabled:" << statistics_enabled_;
    LOG(INFO) << "Statistics interval:" << statistics_interval_.count();
}

//--------------------------------------------------------------------------------------------------
Service::~Service()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void Service::onStart()
{
    LOG(INFO) << "Starting service...";

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
    LOG(INFO) << "Service stoping";
}

//--------------------------------------------------------------------------------------------------
void Service::onTcpReady()
{
    LOG(INFO) << "Connection to the router is established (session count:" << session_count_ << ")";

    // Now the session will receive incoming messages.
    tcp_channel_->resume();
    sendKeyPool(max_peer_count_ - static_cast<quint32>(session_count_));
}

//--------------------------------------------------------------------------------------------------
void Service::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(INFO) << "The connection to the router has been lost:" << error_code;

    // Clearing the key pool.
    key_factory_->clear();

    // Retrying a connection at a time interval.
    delayedConnectToRouter();
}

//--------------------------------------------------------------------------------------------------
void Service::onTcpMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    if (!incoming_message_.parse(buffer))
    {
        LOG(ERROR) << "Invalid message from router";
        return;
    }

    if (incoming_message_->has_key_used())
    {
        std::shared_ptr<KeyDeleter> key_deleter =
            std::make_shared<KeyDeleter>(key_factory_->sharedKeyPool(), incoming_message_->key_used().key_id());

        // The router gave the key to the peers. They are required to use it within 30 seconds.
        // If it is not used during this time, then it will be removed from the pool.
        QTimer::singleShot(std::chrono::seconds(30), this, [key_deleter]()
        {
            key_deleter->deleteKey();
        });
    }
    else if (incoming_message_->has_peer_connection_request())
    {
        const proto::router::PeerConnectionRequest& request = incoming_message_->peer_connection_request();

        switch (request.type())
        {
            case proto::router::PEER_CONNECTION_REQUEST_DISCONNECT:
                emit sessions_worker_->sig_disconnectSession(request.peer_session_id());
                break;

            default:
                LOG(ERROR) << "Unsupported request type:" << request.type();
                break;
        }
    }
    else
    {
        LOG(ERROR) << "Unhandled message from router";
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onSessionStarted()
{
    ++session_count_;
}

//--------------------------------------------------------------------------------------------------
void Service::onSessionStatistics(const proto::router::RelayStat& relay_stat)
{
    outgoing_message_.newMessage().mutable_relay_stat()->CopyFrom(relay_stat);

    // Send a message to the router.
    tcp_channel_->send(proto::router::CHANNEL_ID_SESSION, outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void Service::onSessionFinished()
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
    sendKeyPool(1);
}

//--------------------------------------------------------------------------------------------------
void Service::onPoolKeyExpired(quint32 key_id)
{
    // The key has expired and has been removed from the pool.
    // Add a new key to the pool and send it to the router.
    LOG(INFO) << "Key with id" << key_id << "has expired and removed from pool";
    sendKeyPool(1);
}

//--------------------------------------------------------------------------------------------------
bool Service::start()
{
    LOG(INFO) << "Starting controller";

    if (router_address_.isEmpty())
    {
        LOG(ERROR) << "Empty router address";
        return false;
    }

    if (router_port_ == 0)
    {
        LOG(ERROR) << "Invalid router port";
        return false;
    }

    if (router_public_key_.isEmpty())
    {
        LOG(ERROR) << "Empty router public key";
        return false;
    }

    if (!base::TcpServer::isValidListenInterface(listen_interface_))
    {
        LOG(ERROR) << "Invalid listen interface";
        return false;
    }

    if (peer_address_.isEmpty())
    {
        LOG(ERROR) << "Empty peer address";
        return false;
    }

    if (peer_port_ == 0)
    {
        LOG(ERROR) << "Invalid peer port";
        return false;
    }

    if (peer_idle_timeout_ < std::chrono::minutes(1) || peer_idle_timeout_ > std::chrono::minutes(60))
    {
        LOG(ERROR) << "Invalid peer idle specified";
        return false;
    }

    if (statistics_interval_ < std::chrono::seconds(1) || statistics_interval_ > std::chrono::minutes(60))
    {
        LOG(ERROR) << "Invalid statistics interval";
        return false;
    }

    sessions_worker_ = new SessionsWorker(
        listen_interface_, peer_port_, peer_idle_timeout_, statistics_enabled_, statistics_interval_,
        key_factory_->sharedKeyPool(), this);

    connect(sessions_worker_, &SessionsWorker::sig_sessionStarted,
            this, &Service::onSessionStarted);
    connect(sessions_worker_, &SessionsWorker::sig_sessionFinished,
            this, &Service::onSessionFinished);
    connect(sessions_worker_, &SessionsWorker::sig_sessionStatistics,
            this, &Service::onSessionStatistics);

    sessions_worker_->start();

    connectToRouter();
    return true;
}

//--------------------------------------------------------------------------------------------------
void Service::connectToRouter()
{
    LOG(INFO) << "Connecting to router...";

    base::ClientAuthenticator* authenticator = new base::ClientAuthenticator();
    authenticator->setIdentify(proto::key_exchange::IDENTIFY_ANONYMOUS);
    authenticator->setPeerPublicKey(router_public_key_);
    authenticator->setSessionType(proto::router::SESSION_TYPE_RELAY);

    // Create channel.
    tcp_channel_ = new base::TcpChannel(authenticator, this);

    connect(tcp_channel_, &base::TcpChannel::sig_authenticated,
            this, &Service::onTcpReady);
    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred,
            this, &Service::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived,
            this, &Service::onTcpMessageReceived);

    // Connect to router.
    tcp_channel_->connectTo(router_address_, router_port_);
}

//--------------------------------------------------------------------------------------------------
void Service::delayedConnectToRouter()
{
    LOG(INFO) << "Reconnect after" << kReconnectTimeout.count() << "seconds";
    reconnect_timer_->start(kReconnectTimeout);
}

//--------------------------------------------------------------------------------------------------
void Service::sendKeyPool(quint32 key_count)
{
    proto::router::RelayKeyPool* relay_key_pool = outgoing_message_.newMessage().mutable_key_pool();

    relay_key_pool->set_peer_host(peer_address_.toStdString());
    relay_key_pool->set_peer_port(peer_port_);

    // Add the requested number of keys to the pool.
    for (quint32 i = 0; i < key_count; ++i)
    {
        SessionKey session_key = SessionKey::create();
        if (!session_key.isValid())
        {
            LOG(ERROR) << "Unable to create session key (i=" << i << ")";
            return;
        }

        // Add the key to the outgoing message.
        proto::router::RelayKey* key = relay_key_pool->add_key();

        key->set_type(proto::router::RelayKey::TYPE_X25519);
        key->set_encryption(proto::router::RelayKey::ENCRYPTION_CHACHA20_POLY1305);
        key->set_public_key(session_key.publicKey().toStdString());
        key->set_iv(session_key.iv().toStdString());

        // Add the key to the pool.
        key->set_key_id(key_factory_->addKey(std::move(session_key)));
    }

    // Send a message to the router.
    tcp_channel_->send(proto::router::CHANNEL_ID_SESSION, outgoing_message_.serialize());
}

} // namespace relay
