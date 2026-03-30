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

#include "host/client.h"

#include <QHostAddress>
#include <QTimer>

#include "base/location.h"
#include "base/serialization.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/datagram_decryptor.h"
#include "base/crypto/datagram_encryptor.h"
#include "base/crypto/random.h"
#include "base/peer/stun_peer.h"
#include "base/net/net_utils.h"
#include "base/net/udp_channel.h"
#include "proto/key_exchange.h"

namespace host {

namespace {

const qint64 kProbeIntervalMs = 5000;   // Check every 5 seconds.
const qint64 kIdleThresholdMs = 5000;    // Consider idle after 5 seconds of no sends.
const qint64 kProbeDataSize = 16 * 1024; // 16 KB probe payload.
const int kMaxHolePunchingAttempts = 32;
const int kHolePunchingRetryDelayMs = 2000; // 2 seconds between retry attempts.
const int kInitialProbeTimeoutMs = 5000;   // 5 seconds to wait for initial probe ACK.
const int kUdpConnectTimeoutMs = 5000;   // 5 seconds to wait for UDP connection after setPeerAddress.
const int kUdpReconnectDelayMs = 5000;  // 5 seconds before attempting UDP reconnection.

} // namespace

//--------------------------------------------------------------------------------------------------
Client::Client(base::TcpChannel* tcp_channel, QObject* parent)
    : QObject(parent),
      tcp_channel_(tcp_channel),
      probe_timer_(new QTimer(this))
{
    CLOG(INFO) << "Ctor";
    CCHECK(tcp_channel_);

    tcp_channel_->setParent(this);

    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred, this, &Client::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived, this, &Client::onTcpMessageReceived);
    connect(this, &Client::sig_started, this, [this]()
    {
        tcp_channel_->setPaused(false);
    });

    connect(probe_timer_, &QTimer::timeout, this, [this]()
    {
        TimePoint current_time = Clock::now();

        auto idle_duration = std::chrono::duration_cast<Milliseconds>(current_time - last_send_time_);
        if (idle_duration.count() < kIdleThresholdMs)
            return;

        sendTcpBandwidthProbe(current_time);
        sendUdpBandwidthProbe(current_time);
    });
}

//--------------------------------------------------------------------------------------------------
Client::~Client()
{
    tcp_channel_->disconnect();
}

//--------------------------------------------------------------------------------------------------
void Client::start(bool direct, const QString& stun_host, quint16 stun_port, bool peer_equals)
{
    direct_ = direct;
    peer_equals_ = peer_equals;
    stun_host_ = stun_host;
    stun_port_ = stun_port;

    CLOG(INFO) << "Starting (direct:" << direct << "equals:" << peer_equals
               << "stun:" << stun_host << ":" << stun_port << "features:" << features_ << ")";

    if (features_ & FEATURE_BANDWIDTH)
    {
        startBandwidthProbing();
        sendTcpBandwidthProbe(Clock::now());
    }

    onStart();

    if (features_ & FEATURE_UDP)
        connectToUdp();
}

//--------------------------------------------------------------------------------------------------
quint32 Client::clientId() const
{
    return tcp_channel_->instanceId();
}

//--------------------------------------------------------------------------------------------------
proto::peer::SessionType Client::sessionType() const
{
    return static_cast<proto::peer::SessionType>(tcp_channel_->peerSessionType());
}

//--------------------------------------------------------------------------------------------------
QVersionNumber Client::version() const
{
    return tcp_channel_->peerVersion();
}

//--------------------------------------------------------------------------------------------------
QString Client::osName() const
{
    return tcp_channel_->peerOsName();
}

//--------------------------------------------------------------------------------------------------
QString Client::computerName() const
{
    return tcp_channel_->peerComputerName();
}

//--------------------------------------------------------------------------------------------------
QString Client::displayName() const
{
    return tcp_channel_->peerDisplayName();
}

//--------------------------------------------------------------------------------------------------
QString Client::architecture() const
{
    return tcp_channel_->peerArchitecture();
}

//--------------------------------------------------------------------------------------------------
QString Client::userName() const
{
    return tcp_channel_->peerUserName();
}

//--------------------------------------------------------------------------------------------------
qint64 Client::pendingBytes() const
{
    qint64 result = tcp_channel_->pendingBytes();
    if (udp_channel_)
        result += udp_channel_->pendingBytes();

    return result;
}

//--------------------------------------------------------------------------------------------------
qint64 Client::bandwidth() const
{
    return udp_state_ == UdpState::READY ? udp_probe_.bandwidth : tcp_probe_.bandwidth;
}

//--------------------------------------------------------------------------------------------------
void Client::setFeature(Feature feature, bool enable)
{
    if (enable)
        features_ |= feature;
    else
        features_ &= ~feature;
}

//--------------------------------------------------------------------------------------------------
void Client::send(quint8 channel_id, const QByteArray& buffer, bool reliable)
{
    last_send_time_ = Clock::now();

    if (udp_state_ == UdpState::READY)
    {
        udp_channel_->send(channel_id, buffer, reliable);
        return;
    }

    tcp_channel_->send(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    CLOG(ERROR) << "TCP error:" << error_code;
    CCHECK(tcp_channel_);
    tcp_channel_->disconnect();
    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpMessageReceived(quint8 tcp_channel_id, const QByteArray& buffer)
{
    if (tcp_channel_id != proto::peer::CHANNEL_ID_CONTROL)
    {
        onMessage(tcp_channel_id, buffer);
        return;
    }

    proto::peer::ClientToHost message;
    if (!base::parse(buffer, &message))
    {
        CLOG(ERROR) << "Unable to parse control message";
        return;
    }

    if (message.has_direct_udp_reply())
        readDirectUdpReply(message.direct_udp_reply());
    else if (message.has_bandwidth_probe_ack())
        onTcpBandwidthProbeAck();
    else
        CLOG(WARNING) << "Unhandled control message";
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpReady()
{
    CLOG(INFO) << "UDP channel is connected, sending bandwidth probe";
    CCHECK(udp_channel_);

    // Do NOT set udp_ready_ yet. Send a bandwidth probe over UDP first. When the ACK arrives (see
    // onUdpBandwidthProbeAck), we know the channel works and have an initial bandwidth measurement
    // then we switch to UDP.
    udp_channel_->setPaused(false);
    setUdpState(FROM_HERE, UdpState::CONNECTED);

    sendUdpBandwidthProbe(Clock::now());

    // If the probe ACK does not arrive within the timeout, treat UDP as broken.
    QTimer::singleShot(kInitialProbeTimeoutMs, this, [this]()
    {
        if (udp_state_ == UdpState::PROBED || udp_state_ == UdpState::READY)
            return;

        CLOG(WARNING) << "UDP bandwidth probe timed out, channel is not usable";
        udp_probe_.pending = false;
        onUdpErrorOccurred();
    });
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpErrorOccurred()
{
    CLOG(INFO) << "UDP channel error (" << udp_phase_ << ")";
    CCHECK(udp_channel_);

    bool was_connected = udp_state_ == UdpState::READY;

    setUdpState(FROM_HERE, UdpState::DISCONNECTED);
    udp_probe_.send_time = TimePoint();
    udp_probe_.bandwidth = 0;
    udp_probe_.pending = false;

    udp_channel_->disconnect();
    udp_channel_->deleteLater();
    udp_channel_ = nullptr;

    // If the UDP was already working and then dropped, switch to TCP and try to reconnect.
    if (was_connected)
    {
        CLOG(INFO) << "UDP was connected, switching to TCP and scheduling reconnect";
        udp_phase_ = UdpConnectPhase::NONE;
        hole_punching_attempt_ = 0;
        emit sig_channelChanged();

        QTimer::singleShot(kUdpReconnectDelayMs, this, &Client::connectToUdp);
        return;
    }

    // Fallback chain depending on the current phase.
    switch (udp_phase_)
    {
        case UdpConnectPhase::DIRECT_LAN:
        {
            if (direct_)
            {
                udp_phase_ = UdpConnectPhase::NONE;
                return;
            }

            CLOG(INFO) << "Direct LAN UDP failed, trying hole punching";
            udp_phase_ = UdpConnectPhase::HOLE_PUNCHING;
            startUdpHolePunching();
        }
        break;

        case UdpConnectPhase::HOLE_PUNCHING:
        {
            if (hole_punching_attempt_ < kMaxHolePunchingAttempts)
            {
                CLOG(INFO) << "UDP hole punching attempt" << hole_punching_attempt_
                          << "failed, retrying in" << kHolePunchingRetryDelayMs << "ms";
                QTimer::singleShot(kHolePunchingRetryDelayMs, this, &Client::startUdpHolePunching);
            }
            else
            {
                CLOG(INFO) << "All UDP attempts exhausted, staying on TCP";
                udp_phase_ = UdpConnectPhase::NONE;
            }
        }
        break;

        default:
            udp_phase_ = UdpConnectPhase::NONE;
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpMessageReceived(quint8 udp_channel_id, const QByteArray& buffer)
{
    if (udp_channel_id != proto::peer::CHANNEL_ID_CONTROL)
    {
        onMessage(udp_channel_id, buffer);
        return;
    }

    proto::peer::ClientToHost message;
    if (!base::parse(buffer, &message))
    {
        CLOG(ERROR) << "Unable to parse control message";
        return;
    }

    if (message.has_bandwidth_probe_ack())
        onUdpBandwidthProbeAck();
    else
        CLOG(WARNING) << "Unhandled control message";
}

//--------------------------------------------------------------------------------------------------
void Client::connectToUdp()
{
    if (udp_state_ != UdpState::DISCONNECTED || udp_channel_)
        return;

    if (direct_ || peer_equals_)
    {
        udp_phase_ = UdpConnectPhase::DIRECT_LAN;
        startDirectUdp(-1, QString(), 0);
        return;
    }

    if (stun_host_.isEmpty() || !stun_port_)
        return;

    udp_phase_ = UdpConnectPhase::HOLE_PUNCHING;
    startUdpHolePunching();
}

//--------------------------------------------------------------------------------------------------
void Client::startUdpHolePunching()
{
    if (stun_host_.isEmpty() || !stun_port_)
    {
        CLOG(ERROR) << "No stun server data";
        return;
    }

    ++hole_punching_attempt_;

    CLOG(INFO) << "Stun server data:" << stun_host_ << ':' << stun_port_
              << "(attempt" << hole_punching_attempt_ << ")";

    stun_peer_ = new base::StunPeer(this);

    connect(stun_peer_, &base::StunPeer::sig_channelReady, this,
        [this](const QString& external_address, quint16 external_port)
    {
        CLOG(INFO) << "External UDP endpoint received:" << external_address << ':' << external_port
                  << "(" << udp_phase_ << ")";
        CCHECK(stun_peer_);

        bool is_white_ip = false;

        // On retry attempts we always use the ready socket (previous bind attempt
        // already failed for white IP, or previous ready_socket attempt failed for NAT).
        if (hole_punching_attempt_ <= 1)
        {
            QStringList local_ip_list = base::NetUtils::localIpList();
            for (const auto& local_ip : std::as_const(local_ip_list))
            {
                if (!base::NetUtils::isAddressEqual(external_address, local_ip))
                    continue;

                // The peer has a white external address (without NAT).
                is_white_ip = true;
                break;
            }
        }

        qintptr socket = -1;
        QString address;
        quint16 port = 0;

        if (!is_white_ip)
        {
            // Use the STUN socket with the discovered external endpoint.
            socket = stun_peer_->takeSocket();
            address = external_address;
            port = external_port;
            CLOG(INFO) << "Using ready UDP socket (NAT or forced retry)";
        }
        else
        {
            address = external_address;
            CLOG(INFO) << "White IP detected, using bind for UDP (address:" << address << ")";
        }

        stun_peer_->disconnect();
        stun_peer_->deleteLater();
        stun_peer_ = nullptr;

        startDirectUdp(socket, address, port);
    });

    connect(stun_peer_, &base::StunPeer::sig_errorOccurred, this, [this]()
    {
        CLOG(ERROR) << "STUN error (" << udp_phase_ << ")";
        CCHECK(stun_peer_);

        stun_peer_->disconnect();
        stun_peer_->deleteLater();
        stun_peer_ = nullptr;

        // STUN failed, retry if attempts remain.
        if (hole_punching_attempt_ < kMaxHolePunchingAttempts)
        {
            CLOG(INFO) << "STUN failed (attempt" << hole_punching_attempt_
                      << "), retrying in" << kHolePunchingRetryDelayMs << "ms";
            QTimer::singleShot(kHolePunchingRetryDelayMs, this, &Client::startUdpHolePunching);
        }
        else
        {
            CLOG(INFO) << "All STUN attempts exhausted, staying on TCP";
            udp_phase_ = UdpConnectPhase::NONE;
        }
    });

    stun_peer_->start(stun_host_, stun_port_);
}

//--------------------------------------------------------------------------------------------------
void Client::startDirectUdp(qintptr socket, const QString& address, quint16 port)
{
    CLOG(INFO) << "Starting direct UDP...";
    CCHECK(!udp_channel_);

    udp_channel_ = new base::UdpChannel(this);

    connect(udp_channel_, &base::UdpChannel::sig_ready, this, &Client::onUdpReady);
    connect(udp_channel_, &base::UdpChannel::sig_errorOccurred, this, &Client::onUdpErrorOccurred);
    connect(udp_channel_, &base::UdpChannel::sig_messageReceived, this, &Client::onUdpMessageReceived);

    if (socket != -1)
        udp_channel_->bind(socket);
    else
        udp_channel_->bind(&port);

    PendingUdp context;

    context.key_pair = base::KeyPair::create(base::KeyPair::Type::X25519);
    if (!context.key_pair.isValid())
    {
        CLOG(ERROR) << "Failed to create UDP key pair";
        return;
    }

    context.iv = base::Random::byteArray(12);
    if (context.iv.isEmpty())
    {
        CLOG(ERROR) << "Unable to create IV for UDP";
        return;
    }

    static const quint32 kSupportedEncryptios =
        proto::key_exchange::ENCRYPTION_AES256_GCM | proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305;

    proto::peer::HostToClient message;
    proto::peer::DirectUdpRequest* request = message.mutable_direct_udp_request();

    if (address.isEmpty())
    {
        QStringList local_ip_list = base::NetUtils::localIpList();
        for (const auto& local_ip : std::as_const(local_ip_list))
            request->add_address(local_ip.toStdString());
    }
    else
    {
        request->add_address(address.toStdString());
    }

    request->set_port(port);
    request->set_encryptions(kSupportedEncryptios);
    request->set_public_key(context.key_pair.publicKey().toStdString());
    request->set_iv(context.iv.toStdString());

    pending_udp_context_ = std::move(context);

    // If the request contains data for connecting to STUN, then this is a connection via STUN;
    // otherwise, this is an attempt at a direct connection.
    if (!stun_host_.isEmpty() && stun_port_)
    {
        request->set_stun_host(stun_host_.toStdString());
        request->set_stun_port(stun_port_);
    }

    CLOG(INFO) << "Sending direct UDP request";
    tcp_channel_->send(proto::peer::CHANNEL_ID_CONTROL, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Client::readDirectUdpReply(const proto::peer::DirectUdpReply& reply)
{
    CLOG(INFO) << "Direct UDP reply is received";

    if (!udp_channel_ || !pending_udp_context_.has_value())
    {
        CLOG(ERROR) << "UDP reply received but no UDP channel or pending UDP data";
        return;
    }

    PendingUdp context = std::move(*pending_udp_context_);
    pending_udp_context_.reset();

    QByteArray host_public_key = QByteArray::fromStdString(reply.public_key());
    QByteArray host_iv = QByteArray::fromStdString(reply.iv());
    quint32 encryption = reply.encryption();

    QByteArray session_key = context.key_pair.sessionKey(host_public_key);
    if (session_key.isEmpty())
    {
        CLOG(ERROR) << "Failed to derive UDP session key";
        return;
    }

    std::unique_ptr<base::DatagramEncryptor> encryptor;
    std::unique_ptr<base::DatagramDecryptor> decryptor;

    if (encryption == proto::key_exchange::ENCRYPTION_AES256_GCM)
    {
        encryptor = base::DatagramEncryptor::createForAes256Gcm(session_key, context.iv);
        decryptor = base::DatagramDecryptor::createForAes256Gcm(session_key, host_iv);
    }
    else if (encryption == proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305)
    {
        encryptor = base::DatagramEncryptor::createForChaCha20Poly1305(session_key, context.iv);
        decryptor = base::DatagramDecryptor::createForChaCha20Poly1305(session_key, host_iv);
    }
    else
    {
        CLOG(ERROR) << "Unknown UDP encryption type:" << encryption;
        return;
    }

    if (!encryptor || !decryptor)
    {
        CLOG(ERROR) << "Failed to create UDP encryptor/decryptor";
        return;
    }

    udp_channel_->setEncryptor(std::move(encryptor));
    udp_channel_->setDecryptor(std::move(decryptor));

    // These values contain the CLIENT's external address and port, which were obtained via STUN.
    // If this data is missing, this is a direct connection attempt.
    QString client_address = QString::fromStdString(reply.address());
    quint16 client_port = static_cast<quint16>(reply.port());

    if (!client_address.isEmpty() && client_port)
    {
        CLOG(INFO) << "Setting client's external address:" << client_address << ':' << client_port;
        udp_channel_->setPeerAddress(client_address, client_port);

        // If ENet connection is not established within the deadline, treat as failure.
        QTimer::singleShot(kUdpConnectTimeoutMs, this, [this]()
        {
            if (!udp_channel_ || udp_state_ == UdpState::PROBED || udp_state_ == UdpState::READY)
                return;

            CLOG(WARNING) << "UDP connection timed out after setPeerAddress";
            onUdpErrorOccurred();
        });
    }
}

//--------------------------------------------------------------------------------------------------
QByteArray Client::makeBandwidthProbeData()
{
    std::string payload;
    payload.resize(kProbeDataSize);
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>(i & 0xFF);

    proto::peer::HostToClient message;
    proto::peer::BandwidthProbe* probe = message.mutable_bandwidth_probe();
    probe->set_payload(std::move(payload));

    return base::serialize(message);
}

//--------------------------------------------------------------------------------------------------
void Client::startBandwidthProbing()
{
    last_send_time_ = TimePoint();
    probe_timer_->start(kProbeIntervalMs);
}

//--------------------------------------------------------------------------------------------------
void Client::sendTcpBandwidthProbe(const TimePoint& time)
{
    if (tcp_probe_.pending)
        return;

    tcp_probe_.send_time = time;
    tcp_probe_.pending = true;

    tcp_channel_->send(proto::peer::CHANNEL_ID_CONTROL, makeBandwidthProbeData());
}

//--------------------------------------------------------------------------------------------------
void Client::sendUdpBandwidthProbe(const TimePoint& time)
{
    if (!udp_channel_ || udp_state_ == UdpState::DISCONNECTED || udp_probe_.pending)
        return;

    udp_probe_.send_time = time;
    udp_probe_.pending = true;

    udp_channel_->send(proto::peer::CHANNEL_ID_CONTROL, makeBandwidthProbeData(), true);
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpBandwidthProbeAck()
{
    if (!tcp_probe_.pending)
        return;

    auto rtt = std::chrono::duration_cast<Milliseconds>(Clock::now() - tcp_probe_.send_time);
    if (rtt.count() <= 0)
        rtt = Milliseconds(1);

    tcp_probe_.bandwidth = (kProbeDataSize * 1000) / rtt.count();
    tcp_probe_.pending = false;

    CLOG(INFO) << "TCP RTT:" << rtt.count() << "ms, bandwidth:" << (tcp_probe_.bandwidth / 1024) << "kB/s";
    checkBandwidth();
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpBandwidthProbeAck()
{
    if (!udp_probe_.pending)
        return;

    auto rtt = std::chrono::duration_cast<Milliseconds>(Clock::now() - udp_probe_.send_time);
    if (rtt.count() <= 0)
        rtt = Milliseconds(1);

    udp_probe_.bandwidth = (kProbeDataSize * 1000) / rtt.count();
    udp_probe_.pending = false;

    CLOG(INFO) << "UDP RTT:" << rtt.count() << "ms, bandwidth:" << (udp_probe_.bandwidth / 1024) << "kB/s";
    checkBandwidth();
}

//--------------------------------------------------------------------------------------------------
void Client::checkBandwidth()
{
    if (udp_probe_.bandwidth != 0 && udp_state_ == UdpState::CONNECTED)
        setUdpState(FROM_HERE, UdpState::PROBED);

    if (tcp_probe_.bandwidth == 0)
        return;

    if (udp_probe_.bandwidth == 0)
    {
        onBandwidthChanged(tcp_probe_.bandwidth);
        return;
    }

    if (udp_probe_.bandwidth > 1000 * 1024 || udp_probe_.bandwidth * 2 >= tcp_probe_.bandwidth)
    {
        if (udp_state_ == UdpState::PROBED)
        {
            CLOG(INFO) << "Switching traffic to UDP";
            setUdpState(FROM_HERE, UdpState::READY);
            emit sig_channelChanged();
        }

        onBandwidthChanged(udp_probe_.bandwidth);
        return;
    }

    if (udp_state_ == UdpState::READY)
    {
        CLOG(INFO) << "Switching traffic to TCP";
        setUdpState(FROM_HERE, UdpState::PROBED);
        emit sig_channelChanged();
    }

    onBandwidthChanged(tcp_probe_.bandwidth);
}

//--------------------------------------------------------------------------------------------------
void Client::setUdpState(const base::Location& location, UdpState state)
{
    CLOG(INFO) << "UDP state changed from" << udp_state_ << "to" << state << "(" << location << ")";
    udp_state_ = state;
}

} // namespace host
