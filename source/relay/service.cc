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

#include <QPointer>

#include "base/logging.h"
#include "base/net/tcp_channel_ng.h"
#include "base/peer/client_authenticator.h"
#include "relay/migration_utils.h"
#include "relay/service.h"
#include "relay/session_manager.h"
#include "relay/settings.h"
#include "proto/router.h"

namespace relay {

namespace {

const std::chrono::seconds kReconnectTimeout{ 15 };

class KeyDeleter
{
public:
    KeyDeleter(QPointer<SessionManager> session_manager, quint32 key_id)
        : session_manager_(session_manager),
          key_id_(key_id)
    {
        DCHECK(session_manager_);
    }

    ~KeyDeleter() = default;

    void deleteKey()
    {
        if (session_manager_)
            session_manager_->setKeyExpired(key_id_);
    }

private:
    QPointer<SessionManager> session_manager_;
    const quint32 key_id_;

    Q_DISABLE_COPY_MOVE(KeyDeleter)
};

} // namespace

//--------------------------------------------------------------------------------------------------
// static
const char Service::kFileName[] = "aspia_relay.exe";
const char Service::kName[] = "aspia-relay";
const char Service::kDisplayName[] = "Aspia Relay Service";
const char Service::kDescription[] = "Proxies user traffic to bypass NAT.";

//--------------------------------------------------------------------------------------------------
Service::Service(QObject* parent)
    : base::Service(Service::kName, parent),
      reconnect_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

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
    peer_address_ = settings.peerAddress();
    peer_port_ = settings.peerPort();
    max_peer_count_ = settings.maxPeerCount();

    LOG(INFO) << "Peer address:" << peer_address_;
    LOG(INFO) << "Peer port:" << peer_port_;
    LOG(INFO) << "Max peer count:" << max_peer_count_;
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
void Service::onTcpConnected()
{
    LOG(INFO) << "Connection to the router is established (session count:" << session_count_ << ")";

    // Now the session will receive incoming messages.
    tcp_channel_->setPaused(false);
    sendKeyPool(max_peer_count_ - static_cast<quint32>(session_count_));
}

//--------------------------------------------------------------------------------------------------
void Service::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(INFO) << "Connection to the router has been lost:" << error_code;

    // Clearing the key pool.
    session_manager_->clearKeys();

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
            std::make_shared<KeyDeleter>(session_manager_, incoming_message_->key_used().key_id());

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
                session_manager_->onDisconnectSession(request.peer_session_id());
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
    tcp_channel_->send(0, outgoing_message_.serialize());
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
    LOG(INFO) << "Starting service";

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

    if (!max_peer_count_ || max_peer_count_ > 1000)
    {
        LOG(ERROR) << "Invalid max peer count";
        return false;
    }

    session_manager_ = new SessionManager(this);

    connect(session_manager_, &SessionManager::sig_started, this, &Service::onSessionStarted);
    connect(session_manager_, &SessionManager::sig_finished, this, &Service::onSessionFinished);
    connect(session_manager_, &SessionManager::sig_statistics, this, &Service::onSessionStatistics);
    connect(session_manager_, &SessionManager::sig_keyExpired, this, &Service::onPoolKeyExpired);

    if (!session_manager_->start())
    {
        LOG(ERROR) << "Unable to start session manager";
        return false;
    }

    connectToRouter();
    return true;
}

//--------------------------------------------------------------------------------------------------
void Service::connectToRouter()
{
    base::ClientAuthenticator* authenticator = new base::ClientAuthenticator();
    authenticator->setIdentify(proto::key_exchange::IDENTIFY_ANONYMOUS);
    authenticator->setPeerPublicKey(router_public_key_);
    authenticator->setSessionType(proto::router::SESSION_TYPE_RELAY);

    tcp_channel_ = new base::TcpChannelNG(authenticator, this);

    connect(tcp_channel_, &base::TcpChannel::sig_authenticated, this, &Service::onTcpConnected);
    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred, this, &Service::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived, this, &Service::onTcpMessageReceived);

    LOG(INFO) << "Connecting to router...";
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
        key->set_key_id(session_manager_->addKey(std::move(session_key)));
    }

    // Send a message to the router.
    tcp_channel_->send(0, outgoing_message_.serialize());
}

} // namespace relay
