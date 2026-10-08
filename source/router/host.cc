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

#include "router/host.h"

#include <set>

#include "base/serialization.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "base/threading/worker.h"
#include "proto/router_constants.h"
#include "proto/router_host.h"
#include "proto/router_peer.h"
#include "router/database.h"
#include "router/handlers/host_id_handler.h"
#include "router/workers/client_worker.h"

namespace {

const size_t kHostKeySize = 512;
const size_t kMaxTelemetrySize = 4096;
const Seconds kMinTelemetryInterval{ 5 };

thread_local std::set<HostId> g_assigned_temp_host_ids;

//--------------------------------------------------------------------------------------------------
qint64 createHostId()
{
    static qint64 last_session_id = 0;
    ++last_session_id;
    return last_session_id;
}

//--------------------------------------------------------------------------------------------------
HostId reserveTempHostId()
{
    HostId host_id = kInvalidHostId;
    do
    {
        host_id = createTempHostId();
    }
    while (g_assigned_temp_host_ids.contains(host_id));

    g_assigned_temp_host_ids.insert(host_id);
    return host_id;
}

//--------------------------------------------------------------------------------------------------
void releaseTempHostId(HostId host_id)
{
    g_assigned_temp_host_ids.erase(host_id);
}

} // namespace

//--------------------------------------------------------------------------------------------------
Host::Host(Database& database, TcpChannel* channel, QObject* parent)
    : QObject(parent),
      database_(database),
      session_id_(createHostId()),
      tcp_channel_(channel)
{
    CLOG(TRACE) << "Ctor";

    CDCHECK(tcp_channel_);
    tcp_channel_->setParent(this);

    connect(tcp_channel_, &TcpChannel::sig_errorOccurred, this, &Host::onTcpErrorOccurred);
    connect(tcp_channel_, &TcpChannel::sig_messageReceived, this, &Host::onTcpMessageReceived);
    connect(Worker::current(), &Worker::sig_tick, tcp_channel_, &TcpChannel::tick);
}

//--------------------------------------------------------------------------------------------------
Host::~Host()
{
    CLOG(TRACE) << "Dtor";

    if (isTempHostId(host_id_))
        releaseTempHostId(host_id_);

    // If a remove command was sent over this connection, the host has either processed it (and
    // disconnected/uninstalled) or got dropped before it could; either way the host_id will not
    // resurface, so finalize the hosts_remove row here.
    if (remove_command_sent_ && host_id_ != kInvalidHostId)
    {
        if (database_.isValid())
        {
            if (!database_.finalizeHostRemoval(host_id_))
                CLOG(WARNING) << "Failed to finalize removal for host_id:" << host_id_;
        }
        else
        {
            CLOG(ERROR) << "Failed to connect to database during finalize";
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Host::start()
{
    start_time_ = Clock::now();
    tcp_channel_->setPaused(false);
    emit sig_started(session_id_);
    emit sig_notifyChanged(ClientWorker::NOTIFY_HOSTS);
}

//--------------------------------------------------------------------------------------------------
void Host::stop()
{
    tcp_channel_->setPaused(true);
    disconnect();
}

//--------------------------------------------------------------------------------------------------
QVersionNumber Host::version() const
{
    return tcp_channel_->peerVersion();
}

//--------------------------------------------------------------------------------------------------
const std::string& Host::osName() const
{
    return tcp_channel_->peerOsName();
}

//--------------------------------------------------------------------------------------------------
const std::string& Host::computerName() const
{
    return tcp_channel_->peerComputerName();
}

//--------------------------------------------------------------------------------------------------
const std::string& Host::architecture() const
{
    return tcp_channel_->peerArchitecture();
}

//--------------------------------------------------------------------------------------------------
void Host::sendConnectionOffer(const proto::router::ConnectionOffer& offer)
{
    proto::router::RouterToHost message;
    message.mutable_connection_offer()->CopyFrom(offer);
    sendMessage(0, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Host::sendRemoveCommand()
{
    proto::router::RouterToHost message;
    message.mutable_host_command()->set_command_name(proto::router::kCommandHostRemove);

    // Mark the connection as "remove sent" only once we actually have a buffer to hand to the channel.
    // The destructor treats this flag as proof the host was told, so it must not be set if the
    // command was never serialized. TCP delivery itself is the documented reliability assumption.
    const QByteArray serialized = serialize(message);
    if (serialized.isEmpty())
    {
        CLOG(ERROR) << "Failed to serialize remove command; not marking as sent";
        return;
    }

    sendMessage(0, serialized);
    remove_command_sent_ = true;
}

//--------------------------------------------------------------------------------------------------
void Host::sendUpdateCommand()
{
    proto::router::RouterToHost message;
    message.mutable_host_command()->set_command_name(proto::router::kCommandHostUpdate);
    sendMessage(0, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Host::onTcpErrorOccurred(TcpChannel::ErrorCode error_code)
{
    CLOG(INFO) << "Network error:" << error_code;
    emit sig_finished(session_id_);
}

//--------------------------------------------------------------------------------------------------
void Host::onTcpMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    proto::router::HostToRouter message;
    if (!parse(buffer, &message))
    {
        CLOG(ERROR) << "Could not read message from host";
        return;
    }

    if (message.has_host_id_request())
    {
        readHostIdRequest(message.host_id_request());
    }
    else if (message.has_host_telemetry())
    {
        readHostTelemetry(message.host_telemetry(), Clock::now());
    }
    else
    {
        CLOG(ERROR) << "Unhandled message from host";
    }
}

//--------------------------------------------------------------------------------------------------
void Host::sendMessage(quint8 channel_id, const QByteArray& message)
{
    tcp_channel_->send(channel_id, message);
}

//--------------------------------------------------------------------------------------------------
void Host::readHostIdRequest(const proto::router::HostIdRequest& host_id_request)
{
    HostIdPeer peer;
    peer.computer_name = computerName();
    peer.architecture = architecture();
    peer.version = version().toString().toStdString();
    peer.os_name = osName();
    peer.address = address();

    ++id_request_count_;

    const HostIdResult result =
        handleHostIdRequest(database_, host_id_request, peer, host_id_, id_request_count_);

    if (result.action == HostIdResult::Action::IGNORE)
        return;

    if (result.action == HostIdResult::Action::CLOSE)
    {
        emit sig_finished(sessionId());
        return;
    }

    hw_id_ = result.hardware_id;
    portable_ = result.portable;

    proto::router::RouterToHost message;
    proto::router::HostIdResponse* host_id_response = message.mutable_host_id_response();

    if (result.action == HostIdResult::Action::ISSUE_TEMP_ID)
    {
        // The key and the temporary id belong to this connection. The key is random material
        // handed to the host, and the id is reserved until the connection ends.
        std::string key = Random::string(kHostKeySize);
        key_hash_ = GenericHash::hash(GenericHash::Type::BLAKE2b512, key);
        host_id_ = reserveTempHostId();

        host_id_response->set_error_code(proto::router::kErrorOk);
        host_id_response->set_host_id(host_id_);
        host_id_response->set_key(std::move(key));

        if (!result.removal_pending)
        {
            emit sig_hostIdAssigned(host_id_);
            emit sig_notifyChanged(result.notify_flags);
        }

        sendMessage(0, serialize(message));
        return;
    }

    host_id_ = result.host_id;
    telemetry_ = result.telemetry;
    host_id_response->set_error_code(result.error_code);

    if (host_id_ != kInvalidHostId)
        host_id_response->set_host_id(host_id_);

    sendMessage(0, serialize(message));

    // Sent before the id is announced, so the worker already sees this host as one that is on its
    // way out and keeps it out of the reachable ones. The id is still handed over. The host needs
    // to know which record the command is about, and the removal is finalized when the connection
    // ends.
    if (result.removal_pending)
        sendRemoveCommand();

    if (host_id_ != kInvalidHostId)
    {
        // The worker is told about the id even for a host that is leaving, or a stale predecessor
        // of the same host would not be dropped in favour of this one.
        emit sig_hostIdAssigned(host_id_);

        if (!result.removal_pending)
            emit sig_notifyChanged(result.notify_flags);
    }
}

//--------------------------------------------------------------------------------------------------
void Host::readHostTelemetry(const proto::router::HostTelemetry& host_telemetry, TimePoint now)
{
    if (host_id_ == kInvalidHostId || isTempHostId(host_id_) || remove_command_sent_)
    {
        CLOG(TRACE) << "Telemetry ignored for host_id:" << host_id_;
        return;
    }

    const std::string& telemetry = host_telemetry.json();
    if (telemetry == telemetry_)
        return;

    if (now < next_telemetry_time_)
    {
        CLOG(TRACE) << "Telemetry is too frequent for host_id:" << host_id_;
        return;
    }

    next_telemetry_time_ = now + kMinTelemetryInterval;

    if (telemetry.size() > kMaxTelemetrySize)
    {
        CLOG(ERROR) << "Host reported an oversized telemetry (" << telemetry.size() << "bytes)";
        return;
    }

    if (!database_.updateHostTelemetry(host_id_, telemetry))
    {
        CLOG(WARNING) << "Failed to update telemetry for host_id:" << host_id_;
        return;
    }

    telemetry_ = telemetry;
}
