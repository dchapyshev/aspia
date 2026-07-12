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

#include <algorithm>

#include "base/location.h"
#include "base/serialization.h"
#include "base/net/udp_channel.h"
#include "host/udp_attempt.h"
#include "proto/peer.h"

namespace {

// Probe train cadence. Trains run on this fixed schedule regardless of session activity: they are
// cheap, they measure the bottleneck correctly under load (the probes travel back to back through
// the same queue as the traffic), and they are the only signal that can RAISE the estimate while
// the session is busy - arrival-rate feedback never exceeds the production rate, which the estimate
// itself caps.
const qint64 kProbeIntervalMs = 10000;

// A train sent within this window of session traffic counts as sent under load; its result is
// trusted only upward (see the ack handlers).
const qint64 kIdleThresholdMs = 5000;
const int kUdpInitialDelayMs = 7000;        // Delay before the first UDP negotiation (let key frames flush).
const int kUdpReconnectDelayMs = 5000;      // 5 seconds before attempting UDP reconnection.

// A probe train: several probes sent back to back. The receiver measures the arrival spacing, which
// reflects the bottleneck rate of the path independently of the RTT (a single request/response probe
// measures the RTT instead and grossly lowballs fast links). The total size is kept small on purpose:
// the link may be very slow, and flooding it with a large probe would hurt the session.
const int kProbeTrainLength = 4;
const qint64 kProbePayloadSize = 4 * 1024; // 4 KB per probe, 16 KB per train.

// Estimates are clamped: the ceiling caps the "arrived faster than the clock can measure" case on
// fast local links, the floor keeps a pathological measurement from stalling the session entirely.
const qint64 kMaxBandwidthEstimate = 64 * 1024 * 1024; // 64 MB/s
const qint64 kMinBandwidthEstimate = 8 * 1024;         // 8 KB/s

// The send queue is treated as saturated - the path is the limiting factor, so the receive rate
// reported by the client equals the path capacity - when the queued data is worth more than this
// much transfer time at the current estimate, or exceeds the absolute cap (which also catches an
// overestimated link, whose relative threshold would be unreachably high).
const qint64 kSaturatedDrainTimeMs = 125;
const qint64 kSaturatedPendingBytes = 256 * 1024;

// Estimate changes below these fractions are not propagated to consumers - the quality tiers are
// coarse, and re-publishing every small fluctuation would make them flap at band boundaries. The
// thresholds are asymmetric on purpose: reacting to a degrading link quickly matters (queued frames
// turn into visible lag), while an improvement can wait until it is substantial.
const int kPublishUpThresholdPercent = 25;
const int kPublishDownThresholdPercent = 10;

// Queuing-delay detection on the UDP path. The peer RTT rising this much above the lowest RTT seen
// on the channel means buffers are filling somewhere along the path - congestion that neither the
// send queue nor the arrival rate can show yet. The estimate is cut by the given percent after the
// growth persists for this many consecutive receive-rate reports (roughly seconds).
const int kQueuingDelayThresholdMs = 150;
const int kQueuingDelaySamples = 3;
const int kQueuingDelayCutPercent = 15;

//--------------------------------------------------------------------------------------------------
quint32 nextRequestId()
{
    static quint32 request_id = 0;
    return ++request_id;
}

//--------------------------------------------------------------------------------------------------
QByteArray makeBandwidthProbeData(quint32 train_id, quint32 index, quint32 count)
{
    std::string payload;
    payload.resize(kProbePayloadSize);
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>(i & 0xFF);

    proto::peer::HostToClient message;
    proto::peer::BandwidthProbe* probe = message.mutable_bandwidth_probe();
    probe->set_train_id(train_id);
    probe->set_index(index);
    probe->set_count(count);
    probe->set_payload(std::move(payload));
    return serialize(message);
}

//--------------------------------------------------------------------------------------------------
// Bottleneck bandwidth from a train ack: the bytes that arrived after the first probe over the time
// they took to arrive. A zero interval means the train arrived faster than the clock resolution
// (a fast local link) - treated as the measurable maximum.
qint64 bandwidthFromTrainAck(const proto::peer::BandwidthProbeAck& ack)
{
    if (!ack.bytes())
        return 0;

    if (!ack.delta_us())
        return kMaxBandwidthEstimate;

    const qint64 bandwidth =
        static_cast<qint64>(ack.bytes() * 1'000'000 / ack.delta_us());
    return std::clamp(bandwidth, kMinBandwidthEstimate, kMaxBandwidthEstimate);
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
        if (!peer_ready_)
            return;

        TimePoint current_time = Clock::now();

        // A train whose ack never came (lost probes on UDP, a client that went away mid-measurement)
        // must not block probing forever.
        auto expireProbe = [&current_time](BandwidthProbe& probe)
        {
            if (!probe.pending)
                return;

            auto age = std::chrono::duration_cast<Milliseconds>(current_time - probe.send_time);
            if (age.count() >= kProbeIntervalMs)
                probe.pending = false;
        };
        expireProbe(tcp_probe_);
        expireProbe(udp_probe_);

        // Do not add probe bytes to a queue that is already backed up - it would only add to the
        // lag the queue represents, and the saturation feedback is measuring the capacity there
        // anyway.
        if (pendingBytes() > kSaturatedPendingBytes)
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
void Client::start(const QString& stun_host, quint16 stun_port)
{
    stun_host_ = stun_host;
    stun_port_ = stun_port;

    CLOG(INFO) << "Starting (type:" << tcp_channel_->type() << "stun:" << stun_host << ":"
               << stun_port << "features:" << features_ << ")";

    // No probe train is sent here: the first one goes out when the peer's first message arrives
    // (see onTcpMessageReceived) - only that proves the peer reads the channel in real time.
    if (features_ & FEATURE_BANDWIDTH)
        startBandwidthProbing();

    onStart();

    // Delay UDP negotiation so the initial burst of key frames flushes over TCP first.
    if (features_ & FEATURE_UDP)
        QTimer::singleShot(kUdpInitialDelayMs, this, &Client::connectToUdp);
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
void Client::finish()
{
    // The single point that emits sig_finished(), guarded so it fires at most once. A second emission
    // would reach a client already removed from the service list and trip the CHECK in
    // Service::onClientFinished. Every teardown path (TCP error, IPC error, timeouts, an explicit stop
    // from Service) calls this instead of emitting the signal directly.
    if (finished_emitted_)
        return;

    finished_emitted_ = true;
    emit sig_finished();
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
    finish();
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpMessageReceived(quint8 tcp_channel_id, const QByteArray& buffer)
{
    // The peer's first message proves it finished its handshake and reads the channel in real time -
    // only now is a probe train meaningful. A train sent earlier could arrive while the peer's
    // channel was still paused, get read out of the buffer in one burst on unpause, and measure as
    // an instant arrival: a fast-link verdict on any link, however slow.
    if (!peer_ready_)
    {
        peer_ready_ = true;

        if (features_ & FEATURE_BANDWIDTH)
            sendTcpBandwidthProbe(Clock::now());
    }

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
        onTcpBandwidthProbeAck(message.bandwidth_probe_ack());
    else if (message.has_receive_rate())
        onReceiveRate(message.receive_rate());
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
    const quint32 id = attempt->requestId();

    CLOG(INFO) << "Nominating UDP attempt" << id;

    // Take the channel from the attempt and drive the session through it directly. Any aux resource
    // (gateway mapping) is parented to the channel and travels with it; the attempt is then discarded.
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

    // The attempt's round-trip only proves the path works; the bandwidth it implies is RTT-bound and
    // grossly lowballs fast links. Same physical link as TCP, so carry the TCP estimate over as the
    // starting point, and measure the UDP path for real with an immediate probe train.
    udp_probe_.send_time = TimePoint();
    udp_probe_.pending = false;
    udp_probe_.bandwidth = std::max(bandwidth, tcp_probe_.bandwidth);

    // The RTT baseline belongs to a specific path; a fresh channel measures its own.
    udp_base_rtt_ms_ = 0;
    udp_high_rtt_count_ = 0;

    setUdpState(FROM_HERE, UdpState::CONNECTED);
    sendUdpBandwidthProbe(Clock::now());
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
        onUdpBandwidthProbeAck(message.bandwidth_probe_ack());
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
    udp_base_rtt_ms_ = 0;
    udp_high_rtt_count_ = 0;

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

    if (tcp_channel_->type() == TcpChannel::Type::DIRECT)
    {
        addAndStart(new DirectUdpAttempt(nextRequestId(), this));
        return;
    }

    CLOG(INFO) << "Starting parallel UDP attempts";

    addAndStart(new DirectUdpAttempt(nextRequestId(), this));
    addAndStart(new StunUdpAttempt(nextRequestId(), stun_host_, stun_port_, this));
    addAndStart(new HostGatewayUdpAttempt(nextRequestId(), this));
    addAndStart(new ClientGatewayUdpAttempt(nextRequestId(), this));
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

    auto idle = std::chrono::duration_cast<Milliseconds>(time - last_send_time_);

    tcp_probe_.train_id = ++next_train_id_;
    tcp_probe_.send_time = time;
    tcp_probe_.pending = true;
    tcp_probe_.under_load = idle.count() < kIdleThresholdMs;

    // The probes must go out back to back - the receiver measures their arrival spacing.
    for (int i = 0; i < kProbeTrainLength; ++i)
    {
        tcp_channel_->send(proto::peer::CHANNEL_ID_CONTROL,
                           makeBandwidthProbeData(tcp_probe_.train_id, i, kProbeTrainLength));
    }
}

//--------------------------------------------------------------------------------------------------
void Client::sendUdpBandwidthProbe(const TimePoint& time)
{
    if (!udp_channel_ || udp_state_ == UdpState::DISCONNECTED || udp_probe_.pending)
        return;

    auto idle = std::chrono::duration_cast<Milliseconds>(time - last_send_time_);

    udp_probe_.train_id = ++next_train_id_;
    udp_probe_.send_time = time;
    udp_probe_.pending = true;
    udp_probe_.under_load = idle.count() < kIdleThresholdMs;

    for (int i = 0; i < kProbeTrainLength; ++i)
    {
        udp_channel_->send(proto::peer::CHANNEL_ID_CONTROL,
                           makeBandwidthProbeData(udp_probe_.train_id, i, kProbeTrainLength), true);
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpBandwidthProbeAck(const proto::peer::BandwidthProbeAck& ack)
{
    if (!tcp_probe_.pending || tcp_probe_.train_id != ack.train_id())
        return;

    tcp_probe_.pending = false;

    const qint64 bandwidth = bandwidthFromTrainAck(ack);
    if (!bandwidth)
        return;

    // A train sent while traffic was flowing can come out skewed low (retransmissions, scheduling
    // jitter at the receiver), so under load it is trusted only upward; degradation is detected by
    // the saturation and RTT signals instead. An idle train is a clean measurement and applies both
    // ways.
    if (tcp_probe_.under_load && bandwidth <= tcp_probe_.bandwidth)
        return;

    tcp_probe_.bandwidth = bandwidth;

    CLOG(INFO) << "TCP probe train:" << ack.delta_us() << "us bandwidth:"
               << (bandwidth / 1024) << "kB/s";
    checkBandwidth();
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpBandwidthProbeAck(const proto::peer::BandwidthProbeAck& ack)
{
    if (!udp_probe_.pending || udp_probe_.train_id != ack.train_id())
        return;

    udp_probe_.pending = false;

    const qint64 bandwidth = bandwidthFromTrainAck(ack);
    if (!bandwidth)
        return;

    // See onTcpBandwidthProbeAck: an under-load train is trusted only upward.
    if (udp_probe_.under_load && bandwidth <= udp_probe_.bandwidth)
        return;

    udp_probe_.bandwidth = bandwidth;

    CLOG(INFO) << "UDP probe train:" << ack.delta_us() << "us bandwidth:"
               << (bandwidth / 1024) << "kB/s";
    checkBandwidth();
}

//--------------------------------------------------------------------------------------------------
void Client::onReceiveRate(const proto::peer::ReceiveRate& rate)
{
    if (!rate.bytes() || !rate.interval_ms())
        return;

    const qint64 arrival_rate = std::clamp(
        static_cast<qint64>(rate.bytes() * 1000 / rate.interval_ms()),
        kMinBandwidthEstimate, kMaxBandwidthEstimate);

    // The client reports how fast the session traffic actually arrived. With a backed-up send queue
    // the path itself was the limiting factor, so the arrival rate IS the path capacity - including
    // when the path degraded mid-session. With an empty queue the path was not saturated and the
    // arrival rate is only a lower bound: it can raise the estimate but never lower it. Both moves
    // are half-steps toward the sample rather than jumps: a single interval can be skewed by
    // buffering bursts, and repeated genuine readings still converge within a couple of seconds.
    BandwidthProbe& probe = (udp_state_ == UdpState::READY) ? udp_probe_ : tcp_probe_;

    // Early congestion detection on the UDP path: ENet measures the peer RTT continuously with its
    // own acks and pings. Sustained growth over the channel's lowest RTT is queuing delay - buffers
    // filling somewhere along the path - and warrants cutting the estimate before packet loss or a
    // visible lag spike would force it. (The TCP path needs none of this: kernel backpressure
    // surfaces in the send queue, which the saturation logic below already watches.)
    if (udp_state_ == UdpState::READY && udp_channel_ && probe.bandwidth > 0)
    {
        const int rtt_ms = udp_channel_->roundTripTimeMs();
        if (rtt_ms > 0)
        {
            if (!udp_base_rtt_ms_ || rtt_ms < udp_base_rtt_ms_)
                udp_base_rtt_ms_ = rtt_ms;

            if (rtt_ms - udp_base_rtt_ms_ > kQueuingDelayThresholdMs)
                ++udp_high_rtt_count_;
            else
                udp_high_rtt_count_ = 0;

            if (udp_high_rtt_count_ >= kQueuingDelaySamples)
            {
                udp_high_rtt_count_ = 0;
                probe.bandwidth = std::max(kMinBandwidthEstimate,
                                           probe.bandwidth * (100 - kQueuingDelayCutPercent) / 100);

                CLOG(INFO) << "Sustained UDP RTT growth (" << rtt_ms << "ms over base"
                           << udp_base_rtt_ms_ << "ms) - reducing estimate to"
                           << (probe.bandwidth / 1024) << "kB/s";

                publishBandwidth(probe.bandwidth);
                return;
            }
        }
    }

    const qint64 pending = pendingBytes();
    const bool saturated = pending > kSaturatedPendingBytes ||
        (probe.bandwidth > 0 && pending * 1000 / probe.bandwidth > kSaturatedDrainTimeMs);
    if (!probe.bandwidth)
        probe.bandwidth = arrival_rate; // First sample of the path - take it as is.
    else if (saturated)
        probe.bandwidth = (probe.bandwidth + arrival_rate) / 2;
    else if (arrival_rate > probe.bandwidth)
        probe.bandwidth += (arrival_rate - probe.bandwidth) / 2;
    else
        return;

    publishBandwidth(probe.bandwidth);
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
        publishBandwidth(tcp_probe_.bandwidth);
        return;
    }

    if (udp_state_ == UdpState::PROBED)
    {
        CLOG(INFO) << "Switching traffic to UDP";
        setUdpState(FROM_HERE, UdpState::READY);
        clearAttempts();
        emit sig_channelChanged();
    }

    publishBandwidth(udp_probe_.bandwidth);
}

//--------------------------------------------------------------------------------------------------
void Client::publishBandwidth(qint64 bandwidth)
{
    // Consumers map the estimate onto coarse quality tiers; propagating every fluctuation would make
    // the tiers flap at band boundaries (and the Windows H264 encoder re-creates itself on every
    // change). Only meaningful moves go through - see the threshold constants for the asymmetry.
    if (published_bandwidth_ != 0)
    {
        const qint64 delta = bandwidth - published_bandwidth_;

        if (delta >= 0 && delta < published_bandwidth_ * kPublishUpThresholdPercent / 100)
            return;
        if (delta < 0 && -delta < published_bandwidth_ * kPublishDownThresholdPercent / 100)
            return;
    }

    CLOG(INFO) << "Publishing bandwidth estimate:" << (bandwidth / 1024) << "kB/s";

    published_bandwidth_ = bandwidth;
    onBandwidthChanged(bandwidth);
}

//--------------------------------------------------------------------------------------------------
void Client::setUdpState(const Location& location, UdpState state)
{
    CLOG(INFO) << "UDP state changed:" << udp_state_ << "->" << state << "(" << location << ")";
    udp_state_ = state;
}
