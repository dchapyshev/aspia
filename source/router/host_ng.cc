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

#include "router/host_ng.h"

#include <set>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "proto/router_constants.h"
#include "proto/router_host.h"
#include "proto/router_peer.h"
#include "router/database.h"
#include "router/handlers/host_id_handler.h"

namespace {

const size_t kHostKeySize = 512;

thread_local std::set<HostId> g_assigned_temp_host_ids;

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
HostNG::HostNG(Database& database, TcpChannel* channel, QObject* parent)
    : Host(database, channel, parent)
{
    CLOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
HostNG::~HostNG()
{
    CLOG(TRACE) << "Dtor";

    if (isTempHostId(host_id_))
        releaseTempHostId(host_id_);

    // If a remove command was sent over this connection, the host has either processed it (and
    // disconnected/uninstalled) or got dropped before it could; either way the host_id will not
    // resurface, so finalize the hosts_remove row here.
    if (remove_command_sent_ && host_id_ != kInvalidHostId)
    {
        Database& db = database();
        if (db.isValid())
        {
            if (!db.finalizeHostRemoval(host_id_))
                CLOG(WARNING) << "Failed to finalize removal for host_id:" << host_id_;
        }
        else
        {
            CLOG(ERROR) << "Failed to connect to database during finalize";
        }
    }
}

//--------------------------------------------------------------------------------------------------
void HostNG::sendConnectionOffer(const proto::router::ConnectionOffer& offer)
{
    proto::router::RouterToHost message;
    message.mutable_connection_offer()->CopyFrom(offer);
    sendMessage(0, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void HostNG::sendRemoveCommand()
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
void HostNG::sendUpdateCommand()
{
    proto::router::RouterToHost message;
    message.mutable_host_command()->set_command_name(proto::router::kCommandHostUpdate);
    sendMessage(0, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void HostNG::onSessionMessage(quint8 channel_id, const QByteArray& buffer)
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
    else
    {
        CLOG(ERROR) << "Unhandled message from host";
    }
}

//--------------------------------------------------------------------------------------------------
void HostNG::readHostIdRequest(const proto::router::HostIdRequest& host_id_request)
{
    HostIdPeer peer;
    peer.computer_name = computerName();
    peer.architecture = architecture();
    peer.version = version().toString();
    peer.os_name = osName();
    peer.address = address();

    ++id_request_count_;

    const HostIdResult result =
        handleHostIdRequest(database(), host_id_request, peer, host_id_, id_request_count_);

    if (result.action == HostIdResult::Action::IGNORE)
        return;

    if (result.action == HostIdResult::Action::CLOSE)
    {
        emit sig_finished(sessionId());
        return;
    }

    hw_id_ = result.hardware_id;

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
