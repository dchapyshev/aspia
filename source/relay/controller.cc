//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "relay/controller.h"

#include "base/logging.h"
#include "base/net/tcp_server.h"
#include "base/peer/client_authenticator.h"
#include "proto/router_common.pb.h"
#include "relay/settings.h"

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

    DISALLOW_COPY_AND_ASSIGN(KeyDeleter);
};

} // namespace

//--------------------------------------------------------------------------------------------------
Controller::Controller(QObject* parent)
    : QObject(parent),
      reconnect_timer_(new QTimer(this)),
      key_factory_(new KeyFactory(this))
{
    LOG(LS_INFO) << "Ctor";

    connect(key_factory_, &KeyFactory::sig_keyExpired,
            this, &Controller::onPoolKeyExpired,
            Qt::QueuedConnection);

    reconnect_timer_->setSingleShot(true);
    connect(reconnect_timer_, &QTimer::timeout, this, &Controller::connectToRouter);

    Settings settings;

    // Router settings.
    router_address_ = settings.routerAddress();
    router_port_ = settings.routerPort();
    router_public_key_ = settings.routerPublicKey();

    LOG(LS_INFO) << "Router address: " << router_address_;
    LOG(LS_INFO) << "Router port: " << router_port_;
    LOG(LS_INFO) << "Router public key: " << router_public_key_.toHex().toStdString();

    // Peers settings.
    listen_interface_ = settings.listenInterface();
    peer_address_ = settings.peerAddress();
    peer_port_ = settings.peerPort();
    peer_idle_timeout_ = settings.peerIdleTimeout();
    max_peer_count_ = settings.maxPeerCount();
    statistics_enabled_ = settings.isStatisticsEnabled();
    statistics_interval_ = settings.statisticsInterval();

    LOG(LS_INFO) << "Listen interface: " << listen_interface_;
    LOG(LS_INFO) << "Peer address: " << peer_address_;
    LOG(LS_INFO) << "Peer port: " << peer_port_;
    LOG(LS_INFO) << "Peer idle timeout: " << peer_idle_timeout_.count();
    LOG(LS_INFO) << "Max peer count: " << max_peer_count_;
    LOG(LS_INFO) << "Statistics enabled: " << statistics_enabled_;
    LOG(LS_INFO) << "Statistics interval: " << statistics_interval_.count();
}

//--------------------------------------------------------------------------------------------------
Controller::~Controller()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool Controller::start()
{
    LOG(LS_INFO) << "Starting controller";

    if (router_address_.isEmpty())
    {
        LOG(LS_ERROR) << "Empty router address";
        return false;
    }

    if (router_port_ == 0)
    {
        LOG(LS_ERROR) << "Invalid router port";
        return false;
    }

    if (router_public_key_.isEmpty())
    {
        LOG(LS_ERROR) << "Empty router public key";
        return false;
    }

    if (!base::TcpServer::isValidListenInterface(listen_interface_))
    {
        LOG(LS_ERROR) << "Invalid listen interface";
        return false;
    }

    if (peer_address_.isEmpty())
    {
        LOG(LS_ERROR) << "Empty peer address";
        return false;
    }

    if (peer_port_ == 0)
    {
        LOG(LS_ERROR) << "Invalid peer port";
        return false;
    }

    if (peer_idle_timeout_ < std::chrono::minutes(1) || peer_idle_timeout_ > std::chrono::minutes(60))
    {
        LOG(LS_ERROR) << "Invalid peer idle specified";
        return false;
    }

    if (statistics_interval_ < std::chrono::seconds(1) || statistics_interval_ > std::chrono::minutes(60))
    {
        LOG(LS_ERROR) << "Invalid statistics interval";
        return false;
    }

    sessions_worker_ = new SessionsWorker(
        listen_interface_, peer_port_, peer_idle_timeout_, statistics_enabled_, statistics_interval_,
        key_factory_->sharedKeyPool(), this);

    connect(sessions_worker_, &SessionsWorker::sig_sessionStarted,
            this, &Controller::onSessionStarted);
    connect(sessions_worker_, &SessionsWorker::sig_sessionFinished,
            this, &Controller::onSessionFinished);
    connect(sessions_worker_, &SessionsWorker::sig_sessionStatistics,
            this, &Controller::onSessionStatistics);

    sessions_worker_->start();

    connectToRouter();
    return true;
}

//--------------------------------------------------------------------------------------------------
void Controller::onTcpConnected()
{
    LOG(LS_INFO) << "Connection to the router is established";

    authenticator_ = new base::ClientAuthenticator(this);

    authenticator_->setIdentify(proto::IDENTIFY_ANONYMOUS);
    authenticator_->setPeerPublicKey(router_public_key_);
    authenticator_->setSessionType(proto::ROUTER_SESSION_RELAY);

    connect(authenticator_, &base::Authenticator::sig_finished,
            this, [this](base::Authenticator::ErrorCode error_code)
    {
        if (error_code == base::Authenticator::ErrorCode::SUCCESS)
        {
            connect(tcp_channel_, &base::TcpChannel::sig_disconnected,
                    this, &Controller::onTcpDisconnected);
            connect(tcp_channel_, &base::TcpChannel::sig_messageReceived,
                    this, &Controller::onTcpMessageReceived);

            LOG(LS_INFO) << "Authentication complete (session count: " << session_count_ << ")";

            // Now the session will receive incoming messages.
            tcp_channel_->setChannelIdSupport(true);
            tcp_channel_->resume();

            sendKeyPool(max_peer_count_ - static_cast<quint32>(session_count_));
        }
        else
        {
            LOG(LS_ERROR) << "Authentication failed: "
                          << base::Authenticator::errorToString(error_code);
            delayedConnectToRouter();
        }

        // Authenticator is no longer needed.
        authenticator_->deleteLater();
    });

    authenticator_->start(tcp_channel_);
}

//--------------------------------------------------------------------------------------------------
void Controller::onTcpDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "The connection to the router has been lost: "
                 << base::NetworkChannel::errorToString(error_code);

    // Clearing the key pool.
    key_factory_->clear();

    // Retrying a connection at a time interval.
    delayedConnectToRouter();
}

//--------------------------------------------------------------------------------------------------
void Controller::onTcpMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    if (!incoming_message_.parse(buffer))
    {
        LOG(LS_ERROR) << "Invalid message from router";
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
        const proto::PeerConnectionRequest& request = incoming_message_->peer_connection_request();

        switch (request.type())
        {
            case proto::PEER_CONNECTION_REQUEST_DISCONNECT:
                emit sessions_worker_->sig_disconnectSession(request.peer_session_id());
                break;

            default:
                LOG(LS_ERROR) << "Unsupported request type: " << request.type();
                break;
        }
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message from router";
    }
}

//--------------------------------------------------------------------------------------------------
void Controller::onSessionStarted()
{
    ++session_count_;
}

//--------------------------------------------------------------------------------------------------
void Controller::onSessionStatistics(const proto::RelayStat& relay_stat)
{
    outgoing_message_.newMessage().mutable_relay_stat()->CopyFrom(relay_stat);

    // Send a message to the router.
    tcp_channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void Controller::onSessionFinished()
{
    --session_count_;

    if (session_count_ < 0)
    {
        LOG(LS_ERROR) << "Invalid value for session count: " << session_count_;
        session_count_ = 0;
    }

    // After disconnecting the peer, one key is released.
    // Add a new key to the pool and send it to the router.
    LOG(LS_INFO) << "Session finished. Add new key to pool";
    sendKeyPool(1);
}

//--------------------------------------------------------------------------------------------------
void Controller::onPoolKeyExpired(quint32 key_id)
{
    // The key has expired and has been removed from the pool.
    // Add a new key to the pool and send it to the router.
    LOG(LS_INFO) << "Key with id " << key_id << " has expired and removed from pool";
    sendKeyPool(1);
}

//--------------------------------------------------------------------------------------------------
void Controller::connectToRouter()
{
    LOG(LS_INFO) << "Connecting to router...";

    // Create channel.
    tcp_channel_ = new base::TcpChannel(this);

    connect(tcp_channel_, &base::TcpChannel::sig_connected, this, &Controller::onTcpConnected);

    // Connect to router.
    tcp_channel_->connectTo(router_address_, router_port_);
}

//--------------------------------------------------------------------------------------------------
void Controller::delayedConnectToRouter()
{
    LOG(LS_INFO) << "Reconnect after " << kReconnectTimeout.count() << " seconds";
    reconnect_timer_->start(kReconnectTimeout);
}

//--------------------------------------------------------------------------------------------------
void Controller::sendKeyPool(quint32 key_count)
{
    proto::RelayKeyPool* relay_key_pool = outgoing_message_.newMessage().mutable_key_pool();

    relay_key_pool->set_peer_host(peer_address_.toStdString());
    relay_key_pool->set_peer_port(peer_port_);

    // Add the requested number of keys to the pool.
    for (quint32 i = 0; i < key_count; ++i)
    {
        SessionKey session_key = SessionKey::create();
        if (!session_key.isValid())
        {
            LOG(LS_ERROR) << "Unable to create session key (i=" << i << ")";
            return;
        }

        // Add the key to the outgoing message.
        proto::RelayKey* key = relay_key_pool->add_key();

        key->set_type(proto::RelayKey::TYPE_X25519);
        key->set_encryption(proto::RelayKey::ENCRYPTION_CHACHA20_POLY1305);
        key->set_public_key(session_key.publicKey().toStdString());
        key->set_iv(session_key.iv().toStdString());

        // Add the key to the pool.
        key->set_key_id(key_factory_->addKey(std::move(session_key)));
    }

    // Send a message to the router.
    tcp_channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, outgoing_message_.serialize());
}

} // namespace relay
