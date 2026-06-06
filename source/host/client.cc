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

#include <QTimer>

#include "base/location.h"
#include "base/serialization.h"
#include "base/net/udp_channel.h"
#include "host/udp_attempt.h"
#include "proto/peer.h"

namespace {

const qint64 kProbeIntervalMs = 5000;    // Check every 5 seconds.
const qint64 kIdleThresholdMs = 5000;    // Consider idle after 5 seconds of no sends.
const qint64 kProbeDataSize = 32 * 1024; // 32 KB probe payload.
const int kUdpInitialDelayMs = 5000;     // Delay before the first UDP negotiation (let key frames flush).
const int kUdpReconnectDelayMs = 5000;   // 5 seconds before attempting UDP reconnection.

//--------------------------------------------------------------------------------------------------
quint32 nextRequestId()
{
    static quint32 request_id = 0;
    return ++request_id;
}

//--------------------------------------------------------------------------------------------------
QByteArray makeBandwidthProbeData()
{
    std::string payload;
    payload.resize(kProbeDataSize);
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>(i & 0xFF);

    proto::peer::HostToClient message;
    message.mutable_bandwidth_probe()->set_payload(std::move(payload));
    return serialize(message);
}

//--------------------------------------------------------------------------------------------------
qint64 bandwidthFromRttMs(qint64 rtt_ms)
{
    if (rtt_ms <= 0)
        rtt_ms = 1;
    return (kProbeDataSize * 1000) / rtt_ms;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Client::Client(TcpChannel* tcp_channel, QObject* parent)
    : QObject(parent),
      tcp_channel_(tcp_channel),
      probe_timer_(new QTimer(this))
{
    CLOG(INFO) << "Ctor";
    CCHECK(tcp_channel_);

    tcp_channel_->setParent(this);

    connect(tcp_channel_, &TcpChannel::sig_errorOccurred, this, &Client::onTcpErrorOccurred);
    connect(tcp_channel_, &TcpChannel::sig_messageReceived, this, &Client::onTcpMessageReceived);
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

        if (udp_state_ == UdpState::READY)
        {
            sendUdpBandwidthProbe(current_time);
            return;
        }

        sendTcpBandwidthProbe(current_time);
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

    // Delay UDP negotiation so the initial burst of key frames flushes over TCP first.
    if (features_ & FEATURE_UDP)
        QTimer::singleShot(kUdpInitialDelayMs, this, &Client::connectToUdp);
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
QString Client::address() const
{
    return QString::fromStdString(tcp_channel_->peerAddress());
}

//--------------------------------------------------------------------------------------------------
QString Client::osName() const
{
    return QString::fromStdString(tcp_channel_->peerOsName());
}

//--------------------------------------------------------------------------------------------------
QString Client::computerName() const
{
    return QString::fromStdString(tcp_channel_->peerComputerName());
}

//--------------------------------------------------------------------------------------------------
QString Client::displayName() const
{
    return QString::fromStdString(tcp_channel_->peerDisplayName());
}

//--------------------------------------------------------------------------------------------------
QString Client::architecture() const
{
    return QString::fromStdString(tcp_channel_->peerArchitecture());
}

//--------------------------------------------------------------------------------------------------
QString Client::userName() const
{
    return QString::fromStdString(tcp_channel_->peerUserName());
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

    if (udp_state_ == UdpState::READY && udp_channel_)
    {
        udp_channel_->send(channel_id, buffer, reliable);
        return;
    }

    tcp_channel_->send(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpErrorOccurred(TcpChannel::ErrorCode error_code)
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
    if (!parse(buffer, &message))
    {
        CLOG(ERROR) << "Unable to parse control message";
        return;
    }

    if (message.has_udp_reply())
        readUdpReply(message.udp_reply());
    else if (message.has_bandwidth_probe_ack())
        onTcpBandwidthProbeAck();
    else
        CLOG(WARNING) << "Unhandled control message";
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
void Client::onAttemptConnected(quint32 request_id, qint64 bandwidth)
{
    if (udp_channel_)
        return; // Already have a winner.

    UdpAttempt* attempt = findAttempt(request_id);
    if (attempt)
        selectAttempt(attempt, bandwidth);
}

//--------------------------------------------------------------------------------------------------
void Client::selectAttempt(UdpAttempt* attempt, qint64 bandwidth)
{
    CCHECK(attempt);

    CLOG(INFO) << "Nominating UDP attempt" << attempt->requestId();

    // Take the channel from the attempt and drive the session through it directly. Any aux resource
    // (UPnP mapping) is parented to the channel and travels with it; the attempt is then discarded.
    const quint32 id = attempt->requestId();
    udp_channel_ = attempt->takeChannel();
    eraseAttempt(id);

    if (!udp_channel_)
    {
        CLOG(ERROR) << "Selected attempt has no channel";
        return;
    }

    udp_channel_->setParent(this);
    connect(udp_channel_, &UdpChannel::sig_messageReceived, this, &Client::onUdpMessageReceived);
    connect(udp_channel_, &UdpChannel::sig_errorOccurred, this, &Client::onUdpErrorOccurred);

    udp_channel_->setPaused(false);

    // The attempt's probe round-trip already confirmed the channel and measured its initial
    // bandwidth, so no probe is sent here; further measurements come from the periodic probe timer.
    udp_probe_.send_time = TimePoint();
    udp_probe_.pending = false;
    udp_probe_.bandwidth = bandwidth;

    setUdpState(FROM_HERE, UdpState::CONNECTED);
    checkBandwidth();
}

//--------------------------------------------------------------------------------------------------
void Client::onAttemptError(quint32 request_id)
{
    // A parallel attempt failed (the live session channel reports errors via onUdpErrorOccurred).
    CLOG(INFO) << "UDP attempt" << request_id << "failed";
    eraseAttempt(request_id);
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::peer::CHANNEL_ID_CONTROL)
    {
        onMessage(channel_id, buffer);
        return;
    }

    proto::peer::ClientToHost message;
    if (!parse(buffer, &message))
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
void Client::onUdpErrorOccurred()
{
    const bool was_ready = (udp_state_ == UdpState::READY);

    if (udp_channel_)
    {
        udp_channel_->disconnect();
        udp_channel_.reset();
    }

    setUdpState(FROM_HERE, UdpState::DISCONNECTED);
    udp_probe_.send_time = TimePoint();
    udp_probe_.bandwidth = 0;
    udp_probe_.pending = false;

    if (was_ready)
    {
        // A working UDP channel dropped: fall back to TCP and schedule a fresh attempt.
        CLOG(INFO) << "UDP channel dropped, switching to TCP and scheduling reconnect";
        clearAttempts();
        emit sig_channelChanged();
        QTimer::singleShot(kUdpReconnectDelayMs, this, &Client::connectToUdp);
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void Client::connectToUdp()
{
    if (!attempts_.empty() || udp_channel_)
        return;

    // A direct (non-relayed) connection means the host is reachable directly, so nothing else is
    // needed.
    if (direct_)
    {
        addAndStart(new DirectUdpAttempt(nextRequestId(), this));
        return;
    }

    // Otherwise try the available transports in parallel; the first whose channel passes the probe
    // wins.
    CLOG(INFO) << "Starting parallel UDP attempts";

    // A direct LAN connection only has a chance when the peers appear to share a network. Since this
    // is just a hint and may be wrong, it runs alongside the other transports rather than alone; when
    // the hint is absent the host's local addresses are unreachable, so it is skipped entirely.
    if (peer_equals_)
        addAndStart(new DirectUdpAttempt(nextRequestId(), this));

    if (!stun_host_.isEmpty() && stun_port_)
        addAndStart(new StunUdpAttempt(nextRequestId(), stun_host_, stun_port_, this));

    addAndStart(new HostUpnpUdpAttempt(nextRequestId(), this));
    addAndStart(new ClientUpnpUdpAttempt(nextRequestId(), this));
}

//--------------------------------------------------------------------------------------------------
void Client::readUdpReply(const proto::peer::UdpReply& reply)
{
    CLOG(INFO) << "UDP reply is received (attempt" << reply.request_id() << ")";

    UdpAttempt* attempt = findAttempt(reply.request_id());
    if (!attempt)
    {
        CLOG(WARNING) << "UDP reply for unknown attempt" << reply.request_id() << ", ignoring";
        return;
    }

    attempt->onReply(reply);
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
    tcp_probe_.bandwidth = bandwidthFromRttMs(rtt.count());
    tcp_probe_.pending = false;

    CLOG(INFO) << "TCP RTT:" << rtt.count() << "ms bandwidth:" << (tcp_probe_.bandwidth / 1024) << "kB/s";
    checkBandwidth();
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpBandwidthProbeAck()
{
    if (!udp_probe_.pending)
        return;

    auto rtt = std::chrono::duration_cast<Milliseconds>(Clock::now() - udp_probe_.send_time);
    udp_probe_.bandwidth = bandwidthFromRttMs(rtt.count());
    udp_probe_.pending = false;

    CLOG(INFO) << "UDP RTT:" << rtt.count() << "ms bandwidth:" << (udp_probe_.bandwidth / 1024) << "kB/s";
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

    if (udp_state_ == UdpState::PROBED)
    {
        CLOG(INFO) << "Switching traffic to UDP";
        setUdpState(FROM_HERE, UdpState::READY);
        clearAttempts();
        emit sig_channelChanged();
    }

    onBandwidthChanged(udp_probe_.bandwidth);
}

//--------------------------------------------------------------------------------------------------
void Client::setUdpState(const Location& location, UdpState state)
{
    CLOG(INFO) << "UDP state changed:" << udp_state_ << "->" << state << "(" << location << ")";
    udp_state_ = state;
}
