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

#include "client/client.h"

#include <QTimer>

#include "base/logging.h"
#include "base/version_constants.h"
#include "base/serialization.h"
#include "base/crypto/message_decryptor.h"
#include "base/crypto/message_encryptor.h"
#include "base/crypto/random.h"
#include "base/net/net_utils.h"
#include "base/net/tcp_channel_ng.h"
#include "base/net/tcp_channel_legacy.h"
#include "base/net/udp_channel.h"
#include "base/peer/client_authenticator.h"
#include "base/peer/stun_peer.h"

#if defined(Q_OS_MACOS)
#include "base/mac/app_nap_blocker.h"
#endif // defined(Q_OS_MACOS)

namespace client {

namespace {

auto g_statusType = qRegisterMetaType<client::Client::Status>();
static const int kReadBufferSize = 2 * 1024 * 1024; // 2 Mb.
static const int kWriteBufferSize = 2 * 1024 * 1024; // 2 Mb.

} // namespace

//--------------------------------------------------------------------------------------------------
Client::Client(QObject* parent)
    : QObject(parent),
      timeout_timer_(new QTimer(this)),
      reconnect_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    timeout_timer_->setSingleShot(true);
    connect(timeout_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(INFO) << "Reconnect timeout";

        if (session_state_->isConnectionByHostId() && !is_connected_to_router_)
            emit sig_statusChanged(Status::WAIT_FOR_ROUTER_TIMEOUT);
        else
            emit sig_statusChanged(Status::WAIT_FOR_HOST_TIMEOUT);

        session_state_->setReconnecting(false);
        session_state_->setAutoReconnect(false);

        if (tcp_channel_)
        {
            LOG(INFO) << "Destroy channel";
            tcp_channel_->disconnect();
            tcp_channel_->deleteLater();
            tcp_channel_ = nullptr;
        }

        if (router_controller_)
        {
            LOG(INFO) << "Destroy router controller";
            router_controller_->disconnect();
            router_controller_->deleteLater();
            router_controller_ = nullptr;
        }
    });

    reconnect_timer_->setSingleShot(true);
    connect(reconnect_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(INFO) << "Reconnecting...";
        state_ = State::CREATED;
        start();
    });

#if defined(Q_OS_MACOS)
    base::addAppNapBlock();
#endif // defined(Q_OS_MACOS)
}

//--------------------------------------------------------------------------------------------------
Client::~Client()
{
    LOG(INFO) << "Dtor";

    state_ = State::STOPPPED;
    emit sig_statusChanged(Status::STOPPED);

#if defined(Q_OS_MACOS)
    base::releaseAppNapBlock();
#endif // defined(Q_OS_MACOS)
}

//--------------------------------------------------------------------------------------------------
void Client::start()
{
    if (state_ != State::CREATED)
    {
        LOG(ERROR) << "Client already started before";
        return;
    }

    if (!session_state_)
    {
        LOG(ERROR) << "Session state not installed";
        return;
    }

    state_ = State::STARTED;
    is_connected_to_router_ = false;

    Config config = session_state_->config();

    std::unique_ptr<base::ClientAuthenticator> authenticator =
        std::make_unique<base::ClientAuthenticator>();

    authenticator->setIdentify(proto::key_exchange::IDENTIFY_SRP);
    authenticator->setUserName(session_state_->hostUserName());
    authenticator->setPassword(session_state_->hostPassword());
    authenticator->setSessionType(static_cast<quint32>(session_state_->sessionType()));
    authenticator->setDisplayName(session_state_->displayName());

    if (base::isHostId(config.address_or_id))
    {
        LOG(INFO) << "Starting RELAY connection";

        if (!config.router_config.has_value())
        {
            LOG(FATAL) << "No router config. Continuation is impossible";
            return;
        }

        router_controller_ = new RouterManager(*config.router_config, this);

        connect(router_controller_, &RouterManager::sig_routerConnected,
                this, &Client::onRouterConnected);
        connect(router_controller_, &RouterManager::sig_hostAwaiting,
                this, &Client::onHostAwaiting);
        connect(router_controller_, &RouterManager::sig_hostConnected,
                this, &Client::onHostConnected);
        connect(router_controller_, &RouterManager::sig_errorOccurred,
                this, &Client::onRouterErrorOccurred);

        bool reconnecting = session_state_->isReconnecting();
        if (!reconnecting)
        {
            // Show the status window.
            emit sig_statusChanged(Status::STARTED);
        }

        emit sig_statusChanged(Status::ROUTER_CONNECTING);
        router_controller_->connectTo(
            base::stringToHostId(config.address_or_id), authenticator.release(), reconnecting);
    }
    else
    {
        LOG(INFO) << "Starting DIRECT connection";

        if (!session_state_->isReconnecting())
        {
            // Show the status window.
            emit sig_statusChanged(Status::STARTED);
        }

        // Remove this after support for versions below 3.0.0 ends.
        if (base::kMinimumSupportedVersion < base::kVersion_3_0_0)
        {
            if (is_legacy_mode_)
                tcp_channel_ = new base::TcpChannelLegacy(authenticator.release(), this);
            else
                tcp_channel_ = new base::TcpChannelNG(authenticator.release(), this);
        }
        else
        {
            tcp_channel_ = new base::TcpChannelNG(authenticator.release(), this);
        }

        connect(tcp_channel_, &base::TcpChannel::sig_authenticated, this, &Client::onTcpConnected);
        connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred, this, &Client::onTcpErrorOccurred);
        connect(tcp_channel_, &base::TcpChannel::sig_messageReceived, this, &Client::onTcpMessageReceived);

        // Now connect to the host.
        emit sig_statusChanged(Status::HOST_CONNECTING);
        tcp_channel_->connectTo(config.address_or_id, config.port);
    }
}

//--------------------------------------------------------------------------------------------------
void Client::setSessionState(std::shared_ptr<SessionState> session_state)
{
    LOG(INFO) << "Session state installed";
    session_state_ = session_state;
}

//--------------------------------------------------------------------------------------------------
void Client::sendSessionMessage(const QByteArray& message, bool udp)
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "sendMessage called but channel not initialized";
        return;
    }

    if (udp && udp_ready_)
    {
        udp_channel_->send(proto::peer::CHANNEL_ID_0, message);
        return;
    }

    tcp_channel_->send(proto::peer::CHANNEL_ID_0, message);
}

//--------------------------------------------------------------------------------------------------
void Client::sendServiceMessage(const QByteArray& message, bool udp)
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "sendMessage called but channel not initialized";
        return;
    }

    if (udp && udp_ready_)
    {
        udp_channel_->send(proto::peer::CHANNEL_ID_1, message);
        return;
    }

    tcp_channel_->send(proto::peer::CHANNEL_ID_1, message);
}

//--------------------------------------------------------------------------------------------------
qint64 Client::totalTcpRx() const
{
    if (!tcp_channel_)
        return 0;
    return tcp_channel_->totalRx();
}

//--------------------------------------------------------------------------------------------------
qint64 Client::totalTcpTx() const
{
    if (!tcp_channel_)
        return 0;
    return tcp_channel_->totalTx();
}

//--------------------------------------------------------------------------------------------------
int Client::speedTcpRx()
{
    if (!tcp_channel_)
        return 0;
    return tcp_channel_->speedRx();
}

//--------------------------------------------------------------------------------------------------
int Client::speedTcpTx()
{
    if (!tcp_channel_)
        return 0;
    return tcp_channel_->speedTx();
}

//--------------------------------------------------------------------------------------------------
qint64 Client::totalUdpRx() const
{
    if (!udp_channel_)
        return 0;
    return udp_channel_->totalRx();
}

//--------------------------------------------------------------------------------------------------
qint64 Client::totalUdpTx() const
{
    if (!udp_channel_)
        return 0;
    return udp_channel_->totalTx();
}

//--------------------------------------------------------------------------------------------------
int Client::speedUdpRx()
{
    if (!udp_channel_)
        return 0;
    return udp_channel_->speedRx();
}

//--------------------------------------------------------------------------------------------------
int Client::speedUdpTx()
{
    if (!udp_channel_)
        return 0;
    return udp_channel_->speedTx();
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpConnected()
{
    LOG(INFO) << "Connection established";
    channelReady();
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    // Remove this after support for versions below 3.0.0 ends.
    if (base::kMinimumSupportedVersion < base::kVersion_3_0_0)
    {
        if (error_code == base::TcpChannel::ErrorCode::REMOTE_HOST_CLOSED && !is_legacy_mode_ &&
            !base::isHostId(session_state_->config().address_or_id) && !tcp_channel_->isAuthenticated())
        {
            LOG(INFO) << "Host may be out of date. Trying to connect in legacy mode";
            emit sig_statusChanged(Status::LEGACY_HOST);

            tcp_channel_->disconnect();
            tcp_channel_->deleteLater();
            tcp_channel_ = nullptr;

            is_legacy_mode_ = true;
            state_ = State::CREATED;
            start();
            return;
        }
    }

    LOG(INFO) << "Connection terminated:" << error_code;

    // Show an error to the user.
    emit sig_statusChanged(Status::HOST_DISCONNECTED, QVariant::fromValue(error_code));

    if (tcp_channel_)
    {
        tcp_channel_->disconnect();
        tcp_channel_->deleteLater();
        tcp_channel_ = nullptr;
    }

    if (!session_state_->isAutoReconnect())
        return;

    LOG(INFO) << "Reconnect to host is enabled";
    session_state_->setReconnecting(true);

    timeout_timer_->start(std::chrono::minutes(5));

    if (session_state_->isConnectionByHostId())
    {
        // If you are using an ID connection, then start the connection immediately. The Router
        // will notify you when the Host comes online again.
        state_ = State::CREATED;
        start();
    }
    else
    {
        emit sig_statusChanged(Status::WAIT_FOR_HOST);
        delayedReconnect();
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::peer::CHANNEL_ID_0)
    {
        onSessionMessageReceived(buffer);
    }
    else if (channel_id == proto::peer::CHANNEL_ID_1)
    {
        onServiceMessageReceived(buffer);
    }
    else if (channel_id == proto::peer::CHANNEL_ID_CONTROL)
    {
        proto::peer::HostToClient message;
        if (!base::parse(buffer, &message))
        {
            LOG(ERROR) << "Unable to parse control message";
            return;
        }

        if (message.has_direct_udp_reply())
        {
            const proto::peer::DirectUdpReply& udp_reply = message.direct_udp_reply();

            if (!udp_channel_ || !udp_key_pair_.isValid())
            {
                LOG(ERROR) << "UDP reply received but no UDP channel or key pair";
                return;
            }

            QByteArray host_public_key =
                QByteArray::fromStdString(udp_reply.public_key());
            QByteArray host_iv =
                QByteArray::fromStdString(udp_reply.iv());
            quint32 encryption = udp_reply.encryption();

            QByteArray session_key = udp_key_pair_.sessionKey(host_public_key);
            if (session_key.isEmpty())
            {
                LOG(ERROR) << "Failed to derive UDP session key";
                return;
            }

            std::unique_ptr<base::MessageEncryptor> encryptor;
            std::unique_ptr<base::MessageDecryptor> decryptor;

            if (encryption == proto::key_exchange::ENCRYPTION_AES256_GCM)
            {
                encryptor = base::MessageEncryptor::createForAes256Gcm(
                    session_key, udp_iv_);
                decryptor = base::MessageDecryptor::createForAes256Gcm(
                    session_key, host_iv);
            }
            else if (encryption == proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305)
            {
                encryptor = base::MessageEncryptor::createForChaCha20Poly1305(
                    session_key, udp_iv_);
                decryptor = base::MessageDecryptor::createForChaCha20Poly1305(
                    session_key, host_iv);
            }
            else
            {
                LOG(ERROR) << "Unknown UDP encryption type:" << encryption;
                return;
            }

            if (!encryptor || !decryptor)
            {
                LOG(ERROR) << "Failed to create UDP encryptor/decryptor";
                return;
            }

            udp_channel_->setEncryptor(std::move(encryptor));
            udp_channel_->setDecryptor(std::move(decryptor));
            udp_channel_->setPaused(false);

            // Key pair is no longer needed.
            udp_key_pair_ = base::KeyPair();
            udp_iv_.clear();
        }
    }
    else
    {
        LOG(ERROR) << "Unhandled incoming message from channel:" << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpErrorOccurred()
{
    udp_channel_->disconnect();
    udp_channel_->deleteLater();
    udp_channel_ = nullptr;
    udp_ready_ = false;
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (!udp_ready_)
    {
        LOG(INFO) << "UDP test data received:" << buffer;
        udp_channel_->send(proto::peer::CHANNEL_ID_0, buffer);
        udp_ready_ = true;
        return;
    }

    if (channel_id == proto::peer::CHANNEL_ID_0)
    {
        onSessionMessageReceived(buffer);
    }
    else if (channel_id == proto::peer::CHANNEL_ID_1)
    {
        onServiceMessageReceived(buffer);
    }
    else
    {
        LOG(WARNING) << "Unhandled incoming message from channel" << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onRouterConnected(const QVersionNumber& router_version)
{
    LOG(INFO) << "Router connected";
    session_state_->setRouterVersion(router_version);
    emit sig_statusChanged(Status::ROUTER_CONNECTED);
    is_connected_to_router_ = true;
}

//--------------------------------------------------------------------------------------------------
void Client::onHostAwaiting()
{
    LOG(INFO) << "Host awaiting";
    emit sig_statusChanged(Status::WAIT_FOR_HOST);
}

//--------------------------------------------------------------------------------------------------
void Client::onHostConnected()
{
    LOG(INFO) << "Host connected";

    if (!router_controller_)
    {
        LOG(ERROR) << "No router controller instance";
        return;
    }

    tcp_channel_ = router_controller_->takeChannel();
    tcp_channel_->setParent(this);

    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred, this, &Client::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived, this, &Client::onTcpMessageReceived);

    channelReady();

    QString stun_host = router_controller_->stunHost();
    quint16 stun_port = router_controller_->stunPort();

    if (!stun_host.isEmpty() && stun_port)
    {
        LOG(INFO) << "Stun server data received:" << stun_host << ':' << stun_port;

        stun_peer_ = new base::StunPeer(this);

        connect(stun_peer_, &base::StunPeer::sig_channelReady, this, &Client::onStunChannelReady);
        connect(stun_peer_, &base::StunPeer::sig_errorOccurred, this, [this]()
        {
            LOG(ERROR) << "Error in stun peer";
            stun_peer_->disconnect();
            stun_peer_->deleteLater();
            stun_peer_ = nullptr;
        });

        stun_peer_->start(stun_host, stun_port);
    }

    // Router controller is no longer needed.
    router_controller_->disconnect();
    router_controller_->deleteLater();
    router_controller_ = nullptr;
    is_connected_to_router_ = false;
}

//--------------------------------------------------------------------------------------------------
void Client::onRouterErrorOccurred(const RouterManager::Error& error)
{
    emit sig_statusChanged(Status::ROUTER_ERROR, QVariant::fromValue(error));

    LOG(INFO) << "Post task to destroy router controller";
    router_controller_->disconnect();
    router_controller_->deleteLater();
    router_controller_ = nullptr;
    is_connected_to_router_ = false;

    if (error.type == RouterManager::ErrorType::NETWORK && session_state_->isReconnecting())
    {
        emit sig_statusChanged(Status::WAIT_FOR_ROUTER);
        delayedReconnect();
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onStunChannelReady(const QString& external_address, quint16 external_port)
{
    CHECK(stun_peer_);

    LOG(INFO) << "External endpoint received:" << external_address << ':' << external_port;

    udp_channel_ = stun_peer_->takeChannel();
    udp_channel_->setParent(this);

    stun_peer_->disconnect();
    stun_peer_->deleteLater();
    stun_peer_ = nullptr;

    connect(udp_channel_, &base::UdpChannel::sig_errorOccurred, this, &Client::onUdpErrorOccurred);
    connect(udp_channel_, &base::UdpChannel::sig_messageReceived, this, &Client::onUdpMessageReceived);

    udp_key_pair_ = base::KeyPair::create(base::KeyPair::Type::X25519);
    if (!udp_key_pair_.isValid())
    {
        LOG(ERROR) << "Failed to create UDP key pair";
        return;
    }

    udp_iv_ = base::Random::byteArray(12);

    proto::peer::ClientToHost message;
    proto::peer::DirectUdpRequest* request = message.mutable_direct_udp_request();

    proto::peer::DirectUdpRequest::Endpoint* external_endpoint = request->mutable_external_endpoint();
    external_endpoint->set_ip_address(external_address.toStdString());
    external_endpoint->set_port(external_port);

    QStringList local_ips = base::NetUtils::localIpList();
    for (const auto& local_ip : std::as_const(local_ips))
    {
        proto::peer::DirectUdpRequest::Endpoint* local_endpoint = request->add_local_endpoints();
        local_endpoint->set_ip_address(local_ip.toStdString());
        local_endpoint->set_port(udp_channel_->port());
    }

    static const quint32 kSupportedEncryptios =
        proto::key_exchange::ENCRYPTION_AES256_GCM | proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305;

    proto::peer::DirectUdpRequest::Credentials* credentials = request->mutable_credentials();
    credentials->set_encryptions(kSupportedEncryptios);
    credentials->set_public_key(udp_key_pair_.publicKey().toStdString());
    credentials->set_iv(udp_iv_.toStdString());

    tcp_channel_->send(proto::peer::CHANNEL_ID_CONTROL, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Client::delayedReconnect()
{
    reconnect_timer_->start(std::chrono::seconds(5));
}

//--------------------------------------------------------------------------------------------------
void Client::channelReady()
{
    session_state_->setReconnecting(false);
    reconnect_timer_->stop();
    timeout_timer_->stop();

    const QVersionNumber& host_version = tcp_channel_->peerVersion();
    session_state_->setHostVersion(host_version);

    const QVersionNumber& client_version = base::kCurrentVersion;
    if (host_version > client_version)
    {
        LOG(ERROR) << "Version mismatch. Host:" << host_version << "Client:" << client_version;
        emit sig_statusChanged(Status::VERSION_MISMATCH);
        return;
    }

    emit sig_statusChanged(Status::HOST_CONNECTED);

    // Signal that everything is ready to start the session (connection established,
    // authentication passed).
    onSessionStarted();

    emit sig_showSessionWindow();

    // Now the session will receive incoming messages.
    tcp_channel_->setReadBufferSize(kReadBufferSize);
    tcp_channel_->setWriteBufferSize(kWriteBufferSize);
    tcp_channel_->setPaused(false);
}

} // namespace client
