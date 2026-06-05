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

#include "base/version_constants.h"
#include "base/serialization.h"
#include "base/net/net_utils.h"
#include "base/net/tcp_channel_ng.h"
#include "base/net/tcp_channel_legacy.h"
#include "base/net/udp_channel.h"
#include "base/peer/client_authenticator.h"
#include "base/peer/client_authenticator_legacy.h"
#include "base/peer/relay_peer.h"
#include "client/settings.h"
#include "client/udp_attempt.h"
#include "proto/key_exchange.h"
#include "proto/peer.h"
#include "proto/router_client.h"
#include "proto/router_peer.h"

#if defined(Q_OS_MACOS)
#include "base/mac/app_nap_blocker.h"
#endif // defined(Q_OS_MACOS)

namespace {

auto g_statusType = qRegisterMetaType<Client::Status>();
static const int kReadBufferSize = 2 * 1024 * 1024; // 2 Mb.
static const int kWriteBufferSize = 2 * 1024 * 1024; // 2 Mb.

//--------------------------------------------------------------------------------------------------
template <class Request>
QStringList collectAddresses(const Request& request)
{
    QStringList result;

    for (int i = 0; i < request.address_size(); ++i)
    {
        QString address = QString::fromStdString(request.address(i));
        if (!address.isEmpty() && NetUtils::isValidIpAddress(address))
            result.append(address);
    }

    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Client::Client(QObject* parent)
    : QObject(parent)
{
    CLOG(INFO) << "Ctor";

#if defined(Q_OS_MACOS)
    addAppNapBlock();
#endif // defined(Q_OS_MACOS)
}

//--------------------------------------------------------------------------------------------------
Client::~Client()
{
    CLOG(INFO) << "Dtor";

    state_ = State::STOPPPED;
    emit sig_statusChanged(Status::STOPPED);

#if defined(Q_OS_MACOS)
    releaseAppNapBlock();
#endif // defined(Q_OS_MACOS)
}

//--------------------------------------------------------------------------------------------------
void Client::start()
{
    if (state_ != State::CREATED)
    {
        CLOG(ERROR) << "Client already started before";
        return;
    }

    if (!session_state_)
    {
        CLOG(ERROR) << "Session state not installed";
        return;
    }

    state_ = State::STARTED;

    auto setupAuthenticator = [this](auto* auth)
    {
        auth->setIdentify(proto::key_exchange::IDENTIFY_SRP);
        auth->setUserName(session_state_->hostUserName());
        auth->setPassword(SecureString(session_state_->hostPassword()));
        auth->setSessionType(static_cast<quint32>(session_state_->sessionType()));
        auth->setDisplayName(session_state_->displayName());
    };

    if (session_state_->isConnectionByHostId())
    {
        CLOG(INFO) << "Starting RELAY connection";

        if (session_state_->routerId() <= 0)
        {
            CLOG(FATAL) << "No router id. Continuation is impossible";
            return;
        }

        const proto::router::ConnectionOffer offer = session_state_->connectionOffer();
        if (offer.error_code() != proto::router::ConnectionOffer::SUCCESS)
        {
            CLOG(ERROR) << "Connection offer not provided or has error";
            return;
        }

        if (!session_state_->isReconnecting())
        {
            // Show the status window.
            emit sig_statusChanged(Status::STARTED);
        }

        // For relay path the channel type (Legacy/NG) is decided later from the connection
        // offer. Authenticator implementations are currently identical, so we always use the
        // non-legacy variant here. When the non-legacy handshake diverges, this will need to
        // be revisited together with RelayPeer.
        auto* relay_authenticator = new ClientAuthenticator();
        setupAuthenticator(relay_authenticator);
        relay_peer_ = new RelayPeer(relay_authenticator, this);

        connect(relay_peer_, &RelayPeer::sig_connectionError, this, &Client::onRelayConnectionError);
        connect(relay_peer_, &RelayPeer::sig_connectionReady, this, &Client::onRelayConnectionReady);

        relay_peer_->start(offer);
    }
    else
    {
        CLOG(INFO) << "Starting DIRECT connection";

        if (!session_state_->isReconnecting())
        {
            // Show the status window.
            emit sig_statusChanged(Status::STARTED);
        }

        // Remove this after support for versions below 3.0.0 ends.
        if (kMinimumSupportedVersion < kVersion_3_0_0)
        {
            if (is_legacy_mode_)
            {
                auto* auth = new ClientAuthenticatorLegacy();
                setupAuthenticator(auth);
                tcp_channel_ = new TcpChannelLegacy(auth, this);
            }
            else
            {
                auto* auth = new ClientAuthenticator();
                setupAuthenticator(auth);
                tcp_channel_ = new TcpChannelNG(auth, this);
            }
        }
        else
        {
            auto* auth = new ClientAuthenticator();
            setupAuthenticator(auth);
            tcp_channel_ = new TcpChannelNG(auth, this);
        }

        connect(tcp_channel_, &TcpChannel::sig_authenticated, this, &Client::onTcpConnected);
        connect(tcp_channel_, &TcpChannel::sig_errorOccurred, this, &Client::onTcpErrorOccurred);
        connect(tcp_channel_, &TcpChannel::sig_messageReceived, this, &Client::onTcpMessageReceived);

        // Now connect to the host.
        emit sig_statusChanged(Status::HOST_CONNECTING);
        tcp_channel_->connectTo(session_state_->hostAddress(), session_state_->hostPort());
    }
}

//--------------------------------------------------------------------------------------------------
void Client::setSessionState(std::shared_ptr<SessionState> session_state)
{
    CLOG(INFO) << "Session state installed";
    session_state_ = session_state;
}

//--------------------------------------------------------------------------------------------------
void Client::sendMessage(quint8 channel_id, const QByteArray& message)
{
    if (channel_id == proto::peer::CHANNEL_ID_CONTROL)
    {
        LOG(ERROR) << "Attempt to send with CONTROL channel id";
        return;
    }

    if (!tcp_channel_)
    {
        CLOG(ERROR) << "sendMessage called but channel not initialized";
        return;
    }

    if (udp_ready_)
    {
        udp_channel_->send(channel_id, message, true);
        return;
    }

    tcp_channel_->send(channel_id, message);
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
bool Client::isLegacy() const
{
    return is_legacy_mode_;
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpConnected()
{
    CLOG(INFO) << "Connection established";
    tcpChannelReady();
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpErrorOccurred(TcpChannel::ErrorCode error_code)
{
    // Remove this after support for versions below 3.0.0 ends.
    if (kMinimumSupportedVersion < kVersion_3_0_0)
    {
        if (error_code == TcpChannel::ErrorCode::REMOTE_HOST_CLOSED && !is_legacy_mode_ &&
            !session_state_->isConnectionByHostId() && !tcp_channel_->isAuthenticated())
        {
            CLOG(INFO) << "Host may be out of date. Trying to connect in legacy mode";
            emit sig_statusChanged(Status::LEGACY_HOST);

            tcp_channel_->disconnect();
            tcp_channel_.reset();

            is_legacy_mode_ = true;
            state_ = State::CREATED;
            start();
            return;
        }
    }

    CLOG(INFO) << "Connection terminated:" << error_code;

    // Show an error to the user.
    emit sig_statusChanged(Status::HOST_DISCONNECTED, QVariant::fromValue(error_code));

    if (tcp_channel_)
    {
        tcp_channel_->disconnect();
        tcp_channel_.reset();
    }

    if (!session_state_->isAutoReconnect())
        return;

    CLOG(INFO) << "Reconnect to host is enabled";
    session_state_->setReconnecting(true);

    // Both relay and direct paths are handled the same way: the owner (ClientWindow)
    // observes WAIT_FOR_HOST, destroys this Client, and builds a fresh one when ready.
    emit sig_statusChanged(Status::WAIT_FOR_HOST);
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::peer::CHANNEL_ID_CONTROL)
    {
        proto::peer::HostToClient message;
        if (!parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse control message";
            return;
        }

        if (message.has_direct_udp_request())
        {
            readDirectUdpRequest(message.direct_udp_request());
        }
        else if (message.has_stun_udp_request())
        {
            readStunUdpRequest(message.stun_udp_request());
        }
        else if (message.has_upnp_udp_request())
        {
            readUpnpUdpRequest(message.upnp_udp_request());
        }
        else if (message.has_bandwidth_probe())
        {
            proto::peer::ClientToHost message;
            proto::peer::BandwidthProbeAck* ask = message.mutable_bandwidth_probe_ack();
            ask->set_dummy(1);

            tcp_channel_->send(proto::peer::CHANNEL_ID_CONTROL, serialize(message));
        }
        else
        {
            CLOG(WARNING) << "Unhandled control message";
        }
    }
    else
    {
        onMessageReceived(channel_id, buffer);
    }
}

//--------------------------------------------------------------------------------------------------
void Client::addAndStart(UdpAttempt* attempt)
{
    if (!attempt->isValid())
    {
        CLOG(ERROR) << "Failed to create UDP attempt";
        attempt->deleteLater();
        return;
    }

    connect(attempt, &UdpAttempt::sig_connected, this, &Client::onAttemptConnected);
    connect(attempt, &UdpAttempt::sig_failed, this, &Client::onAttemptError);
    connect(attempt, &UdpAttempt::sig_message, this, [this](const QByteArray& buffer)
    {
        tcp_channel_->send(proto::peer::CHANNEL_ID_CONTROL, buffer);
    });

    attempts_.emplace_back(attempt);
    attempt->start();
}

//--------------------------------------------------------------------------------------------------
UdpAttempt* Client::findAttempt(quint32 request_id)
{
    for (const auto& attempt : attempts_)
    {
        if (attempt && attempt->requestId() == request_id)
            return attempt;
    }
    return nullptr;
}

//--------------------------------------------------------------------------------------------------
void Client::eraseAttempt(quint32 request_id)
{
    for (auto it = attempts_.begin(); it != attempts_.end(); ++it)
    {
        if (!*it || (*it)->requestId() != request_id)
            continue;

        (*it)->disconnect();
        attempts_.erase(it);
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void Client::clearAttempts()
{
    for (auto& attempt : attempts_)
    {
        if (attempt)
            attempt->disconnect();
    }
    attempts_.clear();
}

//--------------------------------------------------------------------------------------------------
void Client::onAttemptError(quint32 request_id)
{
    CLOG(INFO) << "UDP attempt" << request_id << "failed";
    eraseAttempt(request_id);
}

//--------------------------------------------------------------------------------------------------
void Client::onAttemptConnected(quint32 request_id)
{
    if (udp_channel_)
        return;

    UdpAttempt* attempt = findAttempt(request_id);
    if (attempt)
        selectAttempt(attempt);
}

//--------------------------------------------------------------------------------------------------
void Client::selectAttempt(UdpAttempt* attempt)
{
    CCHECK(attempt);

    CLOG(INFO) << "UDP attempt" << attempt->requestId() << "won";

    // Take the channel from the attempt and drive the session through it directly; tear down the
    // rest. The probe that selected us was already acknowledged by the attempt.
    udp_channel_ = attempt->takeChannel();
    clearAttempts();

    if (!udp_channel_)
    {
        CLOG(ERROR) << "Winning attempt has no channel";
        return;
    }

    udp_channel_->setParent(this);
    connect(udp_channel_, &UdpChannel::sig_messageReceived, this, &Client::onUdpMessageReceived);
    connect(udp_channel_, &UdpChannel::sig_errorOccurred, this, &Client::onUdpErrorOccurred);

    udp_ready_ = true;
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::peer::CHANNEL_ID_CONTROL)
    {
        onMessageReceived(channel_id, buffer);
        return;
    }

    proto::peer::HostToClient message;
    if (!parse(buffer, &message))
    {
        CLOG(ERROR) << "Unable to parse UDP control message";
        return;
    }

    if (message.has_bandwidth_probe())
    {
        proto::peer::ClientToHost ack;
        ack.mutable_bandwidth_probe_ack()->set_dummy(1);
        udp_channel_->send(proto::peer::CHANNEL_ID_CONTROL, serialize(ack), true);
    }
    else
    {
        CLOG(WARNING) << "Unhandled UDP control message";
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpErrorOccurred()
{
    CLOG(WARNING) << "UDP channel error";

    udp_ready_ = false;
    if (udp_channel_)
    {
        udp_channel_->disconnect();
        udp_channel_.reset();
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onRelayConnectionReady()
{
    LOG(INFO) << "Relay connection ready";
    CHECK(relay_peer_);

    tcp_channel_ = relay_peer_->takeChannel();
    tcp_channel_->setParent(this);

    relay_peer_->disconnect();
    relay_peer_.reset();

    connect(tcp_channel_, &TcpChannel::sig_errorOccurred, this, &Client::onTcpErrorOccurred);
    connect(tcp_channel_, &TcpChannel::sig_messageReceived, this, &Client::onTcpMessageReceived);

    tcpChannelReady();
}

//--------------------------------------------------------------------------------------------------
void Client::onRelayConnectionError()
{
    LOG(INFO) << "Relay connection error";
    CHECK(relay_peer_);

    relay_peer_->disconnect();
    relay_peer_.reset();

    emit sig_statusChanged(Status::ROUTER_ERROR, tr("Failed to connect to the relay server"));
}

//--------------------------------------------------------------------------------------------------
void Client::tcpChannelReady()
{
    session_state_->setReconnecting(false);

    const QVersionNumber& host_version = tcp_channel_->peerVersion();
    session_state_->setHostVersion(host_version);

    const QVersionNumber& client_version = kCurrentVersion;
    if (host_version > client_version)
    {
        CLOG(ERROR) << "Version mismatch. Host:" << host_version << "Client:" << client_version;
        emit sig_statusChanged(Status::VERSION_MISMATCH);
        return;
    }

    emit sig_statusChanged(Status::HOST_CONNECTED);

    // Signal that everything is ready to start the session (connection established,
    // authentication passed).
    onStarted();

    emit sig_showSessionWindow();

    // Now the session will receive incoming messages.
    tcp_channel_->setReadBufferSize(kReadBufferSize);
    tcp_channel_->setWriteBufferSize(kWriteBufferSize);
    tcp_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
bool Client::isUdpConnectionAllowed()
{
    if (qEnvironmentVariableIsSet("ASPIA_DISABLE_UDP"))
    {
        CLOG(INFO) << "UDP is disabled by environment variable";
        return false;
    }

    if (!Settings().isUdpAllowed())
    {
        CLOG(INFO) << "UDP is disabled by user settings";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void Client::readDirectUdpRequest(const proto::peer::DirectUdpRequest& request)
{
    if (!isUdpConnectionAllowed())
        return;

    QByteArray public_key = QByteArray::fromStdString(request.public_key());
    QByteArray iv = QByteArray::fromStdString(request.iv());
    if (public_key.isEmpty() || iv.isEmpty())
    {
        CLOG(ERROR) << "Empty public key or IV";
        return;
    }

    QStringList addresses = collectAddresses(request);
    if (addresses.isEmpty())
    {
        CLOG(ERROR) << "No valid addresses";
        return;
    }

    if (!NetUtils::isValidPort(request.port()))
    {
        CLOG(ERROR) << "Invalid port:" << request.port();
        return;
    }

    CLOG(INFO) << "Direct UDP request" << request.request_id();
    addAndStart(new DirectUdpAttempt(request.request_id(), public_key, iv, request.encryptions(),
                                     addresses, static_cast<quint16>(request.port()), this));
}

//--------------------------------------------------------------------------------------------------
void Client::readStunUdpRequest(const proto::peer::StunUdpRequest& request)
{
    if (!isUdpConnectionAllowed())
        return;

    QByteArray public_key = QByteArray::fromStdString(request.public_key());
    QByteArray iv = QByteArray::fromStdString(request.iv());
    if (public_key.isEmpty() || iv.isEmpty())
    {
        CLOG(ERROR) << "Empty public key or IV";
        return;
    }

    QStringList addresses = collectAddresses(request);
    if (addresses.isEmpty())
    {
        CLOG(ERROR) << "No valid addresses";
        return;
    }

    if (!NetUtils::isValidPort(request.port()))
    {
        CLOG(ERROR) << "Invalid port:" << request.port();
        return;
    }

    QString stun_host = QString::fromStdString(request.stun_host());
    quint16 stun_port = static_cast<quint16>(request.stun_port());
    if (stun_host.isEmpty() || !stun_port)
    {
        CLOG(ERROR) << "Invalid STUN server data";
        return;
    }

    CLOG(INFO) << "STUN UDP request" << request.request_id() << "(STUN" << stun_host << ':'
               << stun_port << ")";
    addAndStart(new StunUdpAttempt(request.request_id(), public_key, iv, request.encryptions(),
                                   addresses.first(), static_cast<quint16>(request.port()),
                                   stun_host, stun_port, this));
}

//--------------------------------------------------------------------------------------------------
void Client::readUpnpUdpRequest(const proto::peer::UpnpUdpRequest& request)
{
    if (!isUdpConnectionAllowed())
        return;

    QByteArray public_key = QByteArray::fromStdString(request.public_key());
    QByteArray iv = QByteArray::fromStdString(request.iv());
    if (public_key.isEmpty() || iv.isEmpty())
    {
        CLOG(ERROR) << "Empty public key or IV";
        return;
    }

    CLOG(INFO) << "UPnP UDP request" << request.request_id();
    addAndStart(new UpnpUdpAttempt(request.request_id(), public_key, iv, request.encryptions(), this));
}
