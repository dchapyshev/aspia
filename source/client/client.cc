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

#include "base/version_constants.h"
#include "base/serialization.h"
#include "base/net/tcp_channel_ng.h"
#include "base/net/tcp_channel_legacy.h"
#include "base/net/udp_channel.h"
#include "base/peer/client_authenticator.h"
#include "base/peer/client_authenticator_legacy.h"
#include "base/peer/relay_peer.h"
#include "client/session_keeper.h"
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
static const int kReceiveRateIntervalMs = 1000;
static const size_t kMaxUdpAttempts = 16;

} // namespace

//--------------------------------------------------------------------------------------------------
Client::Client(QObject* parent)
    : QObject(parent),
      udp_methods_(Settings().udpMethods())
{
    CLOG(INFO) << "Ctor";

    session_keeper_ = SessionKeeper::create(this);

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

    if (session_keeper_)
        session_keeper_->release();

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
UdpMethod Client::udpMethod() const
{
    return udp_channel_ ? udp_channel_->method() : UdpMethod::DISABLED;
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
    clearAttempts();

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

    if (session_keeper_)
        session_keeper_->release();

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
        else if (message.has_gateway_udp_request())
        {
            readGatewayUdpRequest(message.gateway_udp_request());
        }
        else if (message.has_bandwidth_probe())
        {
            readBandwidthProbe(message.bandwidth_probe(), /* via_udp= */ false);
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

    if (findAttempt(attempt->requestId()) || attempts_.size() >= kMaxUdpAttempts)
    {
        CLOG(WARNING) << "Ignoring UDP attempt" << attempt->requestId()
                      << "(duplicate request id or attempt limit reached)";
        attempt->deleteLater();
        return;
    }

    connect(attempt, &UdpAttempt::sig_connected, this, &Client::onAttemptConnected);
    connect(attempt, &UdpAttempt::sig_failed, this, &Client::onAttemptError);
    connect(attempt, &UdpAttempt::sig_message, this, [this](const QByteArray& buffer)
    {
        if (tcp_channel_)
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
        readBandwidthProbe(message.bandwidth_probe(), /* via_udp= */ true);
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
void Client::onRelayConnectionError(std::optional<TcpChannel::ErrorCode> error_code)
{
    LOG(INFO) << "Relay connection error:" << error_code.has_value();
    CHECK(relay_peer_);

    relay_peer_->disconnect();
    relay_peer_.reset();

    if (error_code.has_value())
    {
        // The relay transport was established, but the peer authentication failed. This is a
        // host-level error (e.g. wrong user name or password), so report it the same way as the
        // direct connection path instead of a misleading "failed to connect to the relay".
        emit sig_statusChanged(Status::HOST_DISCONNECTED, QVariant::fromValue(*error_code));
    }
    else
    {
        // RelayPeer could not reach the relay server itself.
        emit sig_statusChanged(Status::ROUTER_ERROR, tr("Failed to connect to the relay server"));
    }
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

    if (session_keeper_)
        session_keeper_->acquire();

    // Signal that everything is ready to start the session (connection established,
    // authentication passed).
    onStarted();

    emit sig_showSessionWindow();

    // Now the session will receive incoming messages.
    tcp_channel_->setReadBufferSize(kReadBufferSize);
    tcp_channel_->setWriteBufferSize(kWriteBufferSize);
    tcp_channel_->setPaused(false);

    // Report the actual arrival rate of session traffic to the host; it drives the host's bandwidth
    // estimation under real load (probes only run while the link is idle).
    if (!receive_rate_timer_)
    {
        receive_rate_timer_ = new QTimer(this);
        connect(receive_rate_timer_, &QTimer::timeout, this, &Client::onReceiveRateReport);
    }
    receive_rate_last_total_ = totalTcpRx() + totalUdpRx();
    receive_rate_interval_.start();
    receive_rate_timer_->start(kReceiveRateIntervalMs);
}

//--------------------------------------------------------------------------------------------------
void Client::readBandwidthProbe(const proto::peer::BandwidthProbe& probe, bool via_udp)
{
    // First probe of a new train (an unfinished previous train - lost probes on UDP - is simply
    // superseded).
    if (!probe_train_.active || probe_train_.id != probe.train_id())
    {
        probe_train_.id = probe.train_id();
        probe_train_.active = true;
        probe_train_.bytes = 0;
        probe_train_.first_arrival.start();
        return;
    }

    probe_train_.bytes += probe.payload().size();

    // The last probe of the train completes the measurement: report how long the train took to
    // arrive and how much of it arrived after the first probe. The probes are sent back to back, so
    // this reflects the bottleneck rate of the path, not the RTT.
    if (probe.index() + 1 >= probe.count())
    {
        proto::peer::ClientToHost message;
        proto::peer::BandwidthProbeAck* ack = message.mutable_bandwidth_probe_ack();
        ack->set_train_id(probe_train_.id);
        ack->set_delta_us(static_cast<quint64>(probe_train_.first_arrival.nsecsElapsed() / 1000));
        ack->set_bytes(probe_train_.bytes);

        probe_train_.active = false;

        if (via_udp)
            udp_channel_->send(proto::peer::CHANNEL_ID_CONTROL, serialize(message), true);
        else
            tcp_channel_->send(proto::peer::CHANNEL_ID_CONTROL, serialize(message));
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onReceiveRateReport()
{
    const qint64 total = totalTcpRx() + totalUdpRx();
    const qint64 bytes = total - receive_rate_last_total_;
    const qint64 interval_ms = receive_rate_interval_.restart();

    receive_rate_last_total_ = total;

    // An idle interval carries no information about the path capacity.
    if (bytes <= 0 || interval_ms <= 0 || !tcp_channel_)
        return;

    proto::peer::ClientToHost message;
    proto::peer::ReceiveRate* rate = message.mutable_receive_rate();
    rate->set_bytes(static_cast<quint64>(bytes));
    rate->set_interval_ms(static_cast<quint32>(interval_ms));

    tcp_channel_->send(proto::peer::CHANNEL_ID_CONTROL, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Client::readDirectUdpRequest(const proto::peer::DirectUdpRequest& request)
{
    // Ignore once a UDP channel is up (late requests from losing host attempts).
    if (udp_channel_)
        return;

    // A host-gateway endpoint (gateway=true) reaches us as a direct connection, but is allowed only
    // when gateway mapping is enabled; a plain direct connection is gated by UDP_METHOD_DIRECT.
    const quint32 required = request.gateway() ?
        (UDP_METHOD_PCP | UDP_METHOD_NAT_PMP | UDP_METHOD_UPNP) : UDP_METHOD_DIRECT;
    if (!(udp_methods_ & required))
        return;

    CLOG(INFO) << "Direct UDP request" << request.request_id();
    addAndStart(new DirectUdpAttempt(request, this));
}

//--------------------------------------------------------------------------------------------------
void Client::readStunUdpRequest(const proto::peer::StunUdpRequest& request)
{
    // Ignore once a UDP channel is up (late requests from losing host attempts).
    if (udp_channel_ || !(udp_methods_ & UDP_METHOD_HOLE_PUNCHING))
        return;

    CLOG(INFO) << "STUN UDP request" << request.request_id();
    addAndStart(new StunUdpAttempt(request, this));
}

//--------------------------------------------------------------------------------------------------
void Client::readGatewayUdpRequest(const proto::peer::GatewayUdpRequest& request)
{
    // Ignore once a UDP channel is up (late requests from losing host attempts).
    if (udp_channel_)
        return;

    const quint32 gateway_methods =
        udp_methods_ & (UDP_METHOD_PCP | UDP_METHOD_NAT_PMP | UDP_METHOD_UPNP);
    if (!gateway_methods)
        return;

    CLOG(INFO) << "Gateway UDP request" << request.request_id();
    addAndStart(new GatewayUdpAttempt(request, gateway_methods, this));
}
