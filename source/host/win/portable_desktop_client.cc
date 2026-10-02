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

#include "host/win/portable_desktop_client.h"

#include "base/logging.h"
#include "base/numeric_utils.h"
#include "base/serialization.h"
#include "proto/desktop_channel.h"

//--------------------------------------------------------------------------------------------------
PortableDesktopClient::PortableDesktopClient(TcpChannel* tcp_channel, QObject* parent)
    : Client(tcp_channel, parent)
{
    CLOG(TRACE) << "Ctor";

    overflow_detection_enabled_ = !qEnvironmentVariableIsSet("ASPIA_NO_OVERFLOW_DETECTION");
    if (overflow_detection_enabled_)
        CLOG(INFO) << "Overflow detection enabled";
}

//--------------------------------------------------------------------------------------------------
PortableDesktopClient::~PortableDesktopClient()
{
    CLOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopClient::readDesktopMessage(quint32 channel_id, const QByteArray& buffer, bool reliable)
{
    quint16 net_channel_id = lowWord(channel_id);
    quint16 ipc_channel_id = highWord(channel_id);

    if (force_reliable_)
        reliable = true;

    if (ipc_channel_id == proto::desktop::IPC_CHANNEL_ID_SESSION)
        send(net_channel_id, buffer, reliable);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopClient::onUserMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::desktop::CHANNEL_ID_CLIPBOARD || channel_id == proto::desktop::CHANNEL_ID_FILE)
    {
        if (!config_.has_value() || !config_->clipboard())
            return;
    }

    send(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopClient::onStart()
{
    emit sig_started();
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopClient::onMessage(quint8 net_channel_id, const QByteArray& buffer)
{
    if (net_channel_id == proto::desktop::CHANNEL_ID_CONTROL)
    {
        proto::control::ClientToHost message;
        if (!parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse control message";
            return;
        }

        if (message.has_config())
        {
            config_ = message.config();
            sendDesktopSessionMessage(net_channel_id, buffer);
        }
        else if (message.has_capabilities())
        {
            sendDesktopSessionMessage(net_channel_id, buffer);
        }
        else if (message.has_feedback())
        {
            readFeedback(message.feedback());
        }
        else
        {
            CLOG(WARNING) << "Control message is not served in user mode";
        }
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_CLIPBOARD)
    {
        if (!config_.has_value() || !config_->clipboard())
            return;

        emit sig_userMessage(net_channel_id, buffer);
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_USER)
    {
        emit sig_userMessage(net_channel_id, buffer);
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_FILE)
    {
        if (!config_.has_value() || !config_->clipboard())
            return;

        emit sig_userMessage(net_channel_id, buffer);
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_POWER ||
             net_channel_id == proto::desktop::CHANNEL_ID_TASK_MANAGER ||
             net_channel_id == proto::desktop::CHANNEL_ID_TOOLS)
    {
        CLOG(WARNING) << "Channel is not served in user mode:" << net_channel_id;
    }
    else
    {
        sendDesktopSessionMessage(net_channel_id, buffer);
    }
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopClient::onBandwidthChanged(qint64 bandwidth)
{
    proto::desktop::ServiceToAgentClient message;
    proto::desktop::BandwidthChange* bandwidth_change = message.mutable_bandwidth_change();
    bandwidth_change->set_bandwidth(bandwidth);
    sendDesktopServiceMessage(serialize(message));
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopClient::onTimer(TimePoint now)
{
    Client::onTimer(now);

    if (isFinished() || !overflow_detection_enabled_)
        return;

    // Byte thresholds: an absolute backstop that fires regardless of the bandwidth estimate. They
    // bound the damage when the estimate is too optimistic - an overestimated link would otherwise
    // be allowed to queue tens of megabytes before the drain-time brake below trips.
    static const qint64 kCriticalPendingBytes = 1 * 1024 * 1024; // 1 MB
    static const qint64 kWarningPendingBytes = 512 * 1024; // 512 kB

    // Drain-time thresholds: how long the queued data would take to go out at the estimated link
    // rate. They trip much earlier than the byte backstop on slow links, where a fixed byte count
    // would mean seconds of added latency before any throttling starts.
    static const MilliSeconds kCriticalDrainTime{ 1000 };
    static const MilliSeconds kWarningDrainTime{ 300 };

    // The excess over the path's normal in-flight window; a healthy long-RTT UDP link would trip
    // the thresholds constantly on the raw value.
    proto::desktop::Overflow::State state = proto::desktop::Overflow::STATE_NONE;
    qint64 pending = excessPendingBytes();
    const qint64 estimated_bandwidth = bandwidth();

    if (pending > kCriticalPendingBytes)
        state = proto::desktop::Overflow::STATE_CRITICAL;
    else if (pending > kWarningPendingBytes)
        state = proto::desktop::Overflow::STATE_WARNING;

    if (estimated_bandwidth > 0 && state != proto::desktop::Overflow::STATE_CRITICAL)
    {
        const MilliSeconds drain_time{ pending * 1000 / estimated_bandwidth };

        if (drain_time > kCriticalDrainTime)
            state = proto::desktop::Overflow::STATE_CRITICAL;
        else if (drain_time > kWarningDrainTime && state == proto::desktop::Overflow::STATE_NONE)
            state = proto::desktop::Overflow::STATE_WARNING;
    }

    if (state != last_state_)
    {
        CLOG(INFO) << "Overflow state:" << state << "pending:" << pending;
        last_state_ = state;
    }

    proto::desktop::ServiceToAgentClient message;
    proto::desktop::Overflow* overflow = message.mutable_overflow();
    overflow->set_state(state);
    sendDesktopServiceMessage(serialize(message));
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopClient::sendDesktopSessionMessage(quint8 net_channel_id, const QByteArray& buffer)
{
    emit sig_desktopMessage(makeUint32(proto::desktop::IPC_CHANNEL_ID_SESSION, net_channel_id), buffer);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopClient::sendDesktopServiceMessage(const QByteArray& buffer)
{
    emit sig_desktopMessage(makeUint32(proto::desktop::IPC_CHANNEL_ID_SERVICE, 0), buffer);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopClient::readFeedback(const proto::control::Feedback& feedback)
{
    if (feedback.command_name() == "reliable")
    {
        if (feedback.value_case() != proto::control::Feedback::kBoolean)
        {
            CLOG(WARNING) << "Feedback 'reliable' expects boolean value";
            return;
        }

        if (force_reliable_ != feedback.boolean())
        {
            CLOG(INFO) << "Force reliable changed:" << force_reliable_ << "->" << feedback.boolean();
            force_reliable_ = feedback.boolean();
        }
    }
    else
    {
        CLOG(WARNING) << "Unknown feedback command:" << feedback.command_name();
    }
}
