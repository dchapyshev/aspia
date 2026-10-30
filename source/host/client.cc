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
#include "base/threading/worker.h"
#include "host/udp_attempt.h"
#include "proto/peer.h"

namespace {

//--------------------------------------------------------------------------------------------------
quint32 nextRequestId()
{
    static quint32 request_id = 0;
    return ++request_id;
}

//--------------------------------------------------------------------------------------------------
QByteArray makeBandwidthProbeData(quint32 train_id, quint32 index, quint32 count)
{
    proto::peer::HostToClient message;
    proto::peer::BandwidthProbe* probe = message.mutable_bandwidth_probe();
    probe->set_train_id(train_id);
    probe->set_index(index);
    probe->set_count(count);
    return serialize(message);
}

} // namespace

//--------------------------------------------------------------------------------------------------
Client::Client(TcpChannel* tcp_channel, QObject* parent)
    : QObject(parent),
      tcp_channel_(tcp_channel)
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

    connect(Worker::current(), &Worker::sig_tick, this, &Client::onTimer);

    slice_timer_ = new QTimer(this);
    slice_timer_->setInterval(BandwidthController::kPaddingSlice);
    connect(slice_timer_, &QTimer::timeout, this, &Client::onSliceTimer);
    slice_timer_->start();
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

    onStart();

    if (features_ & FEATURE_UDP)
        connectToUdp();
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
proto::desktop::Overflow::State Client::overflowState(TimePoint now) const
{
    return bandwidth_controller_.overflowState(now);
}

//--------------------------------------------------------------------------------------------------
void Client::send(quint8 channel_id, const QByteArray& buffer)
{
    bandwidth_controller_.onBytesSent(buffer.size());

    if (udp_state_ == UdpState::READY && udp_channel_)
    {
        udp_channel_->send(channel_id, buffer);
        return;
    }

    tcp_channel_->send(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void Client::onTimer(TimePoint now)
{
    tcp_channel_->tick(now);

    bandwidth_controller_.tick(now);
    checkBandwidth();
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
    // The peer's first message proves it reads the channel in real time. A probe sent earlier would
    // wait in the buffer of the paused channel and look like a stalled path.
    if (!peer_ready_)
    {
        peer_ready_ = true;
        bandwidth_controller_.start();
        checkBandwidth();

        // The first probe goes out before the first frame, so it measures the path and not the queue.
        if (bandwidth_controller_.startProbe(Clock::now()))
            sendDelayProbe();
    }

    if (tcp_channel_id != proto::peer::CHANNEL_ID_CONTROL &&
        tcp_channel_id != proto::peer::CHANNEL_ID_PROBE)
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
    else if (message.has_udp_switch())
        readUdpSwitch();
    else if (message.has_bandwidth_probe_ack())
        onBandwidthProbeAck(message.bandwidth_probe_ack());
    else if (message.has_receive_rate())
        onReceiveRate(message.receive_rate());
    else
        CLOG(WARNING) << "Unhandled control message";
}

//--------------------------------------------------------------------------------------------------
void Client::onSliceTimer()
{
    const TimePoint now = Clock::now();

    if (bandwidth_controller_.startProbe(now))
        sendDelayProbe();

    const qint64 due = bandwidth_controller_.paddingDue(now);
    if (due > 0)
        send(proto::peer::CHANNEL_ID_PADDING, QByteArray(static_cast<int>(due), '\0'));
}

//--------------------------------------------------------------------------------------------------
void Client::connectToUdp()
{
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
void Client::readUdpSwitch()
{
    udp_switch_pending_ = false;

    while (!held_udp_messages_.empty())
    {
        auto [channel_id, buffer] = std::move(held_udp_messages_.front());
        held_udp_messages_.pop_front();
        onMessage(channel_id, buffer);
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onAttemptConnected(quint32 request_id)
{
    if (udp_channel_)
        return; // Already have a winner.

    UdpAttempt* attempt = findAttempt(request_id);
    if (attempt)
        selectAttempt(attempt);
}

//--------------------------------------------------------------------------------------------------
void Client::onAttemptError(quint32 request_id)
{
    // A parallel attempt failed (the live session channel reports errors via onUdpErrorOccurred).
    CLOG(INFO) << "UDP attempt" << request_id << "failed";
    eraseAttempt(request_id);
}

//--------------------------------------------------------------------------------------------------
void Client::selectAttempt(UdpAttempt* attempt)
{
    CCHECK(attempt);
    const quint32 id = attempt->requestId();

    CLOG(INFO) << "Nominating UDP attempt" << id;

    // Take the channel from the attempt and drive the session through it directly. Any aux resource
    // (gateway mapping) is parented to the channel and travels with it.
    udp_channel_ = attempt->takeChannel();

    if (!udp_channel_)
    {
        CLOG(ERROR) << "Selected attempt has no channel";
        clearAttempts();
        return;
    }

    // The channel is still a child of the attempt that created it, so it has to be adopted before
    // the attempts are dropped.
    udp_channel_->setParent(this);

    // The losing attempts go now. One that gets its own acknowledgement a moment later is refused
    // the channel but stops timing out, and would hold its socket and its gateway mapping until the
    // session ends.
    clearAttempts();
    connect(udp_channel_, &UdpChannel::sig_messageReceived, this, &Client::onUdpMessageReceived);
    connect(udp_channel_, &UdpChannel::sig_errorOccurred, this, &Client::onUdpErrorOccurred);

    udp_switch_pending_ = true;
    udp_channel_->setPaused(false);

    setUdpState(FROM_HERE, UdpState::CONNECTED);
    checkBandwidth();
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
void Client::onUdpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::peer::CHANNEL_ID_CONTROL && channel_id != proto::peer::CHANNEL_ID_PROBE)
    {
        if (udp_switch_pending_)
        {
            held_udp_messages_.emplace_back(channel_id, buffer);
            return;
        }

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
        onBandwidthProbeAck(message.bandwidth_probe_ack());
    else
        CLOG(WARNING) << "Unhandled control message";
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpErrorOccurred()
{
    if (udp_state_ == UdpState::READY)
    {
        CLOG(ERROR) << "UDP channel dropped";
        udp_channel_->disconnect();
        finish();
        return;
    }

    if (udp_channel_)
    {
        udp_channel_->disconnect();
        udp_channel_.reset();
    }

    setUdpState(FROM_HERE, UdpState::DISCONNECTED);
}

//--------------------------------------------------------------------------------------------------
void Client::sendDelayProbe()
{
    static const quint8 kDelayProbeChannels[] = { proto::peer::CHANNEL_ID_CONTROL,
                                                  proto::peer::CHANNEL_ID_PROBE };
    static const int kDelayProbeTrainLength = std::size(kDelayProbeChannels);

    delay_probe_train_id_ = ++next_train_id_;

    for (int i = 0; i < kDelayProbeTrainLength; ++i)
    {
        send(kDelayProbeChannels[i],
             makeBandwidthProbeData(delay_probe_train_id_, i, kDelayProbeTrainLength));
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onBandwidthProbeAck(const proto::peer::BandwidthProbeAck& ack)
{
    if (ack.train_id() != delay_probe_train_id_)
        return;

    bandwidth_controller_.finishProbe(Clock::now());
    checkBandwidth();
}

//--------------------------------------------------------------------------------------------------
void Client::onReceiveRate(const proto::peer::ReceiveRate& /* rate */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void Client::checkBandwidth()
{
    if (!bandwidth_controller_.bandwidth())
        return;

    if (udp_state_ == UdpState::CONNECTED)
    {
        CLOG(INFO) << "Switching traffic to UDP";

        proto::peer::HostToClient message;
        message.mutable_udp_switch()->set_dummy(1);
        tcp_channel_->send(proto::peer::CHANNEL_ID_CONTROL, serialize(message));

        setUdpState(FROM_HERE, UdpState::READY);
        bandwidth_controller_.resetPath();

        if (bandwidth_controller_.startProbe(Clock::now()))
            sendDelayProbe();

        clearAttempts();
        emit sig_channelChanged();
    }

    publishBandwidth(bandwidth_controller_.bandwidth());

    if (udp_channel_)
    {
        udp_channel_->setWindowSize(
            bandwidth_controller_.windowSize(MilliSeconds(udp_channel_->roundTripTime())));
    }
}

//--------------------------------------------------------------------------------------------------
void Client::publishBandwidth(qint64 bandwidth)
{
    if (bandwidth == published_bandwidth_)
        return;

    published_bandwidth_ = bandwidth;
    onBandwidthChanged(bandwidth);
}

//--------------------------------------------------------------------------------------------------
void Client::setUdpState(const Location& location, UdpState state)
{
    CLOG(INFO) << "UDP state changed:" << udp_state_ << "->" << state << "(" << location << ")";
    udp_state_ = state;
}
