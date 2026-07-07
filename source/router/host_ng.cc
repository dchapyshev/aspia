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

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "proto/router_constants.h"
#include "proto/router_host.h"
#include "proto/router_peer.h"
#include "router/database.h"
#include "router/service.h"

namespace {

const size_t kHostKeySize = 512;

} // namespace

//--------------------------------------------------------------------------------------------------
HostNG::HostNG(TcpChannel* channel, QObject* parent)
    : Host(channel, parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
HostNG::~HostNG()
{
    CLOG(INFO) << "Dtor";

    // If a remove command was sent during this session, the host has either processed it (and
    // disconnected/uninstalled) or got dropped before it could; either way the host_id will not
    // resurface, so finalize the hosts_remove row here.
    if (remove_command_sent_ && host_id_ != kInvalidHostId)
    {
        Database& database = Database::instance();
        if (database.isValid())
        {
            if (!database.finalizeHostRemoval(host_id_))
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

    // Mark the session as "remove sent" only once we actually have a buffer to hand to the channel.
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
    // A host requests its id exactly once per session. Reject repeats so an untrusted host cannot
    // overwrite host_id_ after assignment and desync the pending-removal finalization done in the
    // destructor. A failed request leaves host_id_ invalid, so a legitimate retry still works.
    if (host_id_ != kInvalidHostId)
    {
        CLOG(ERROR) << "Ignoring repeated host id request; host id" << host_id_ << "already assigned";
        return;
    }

    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return;
    }

    proto::router::RouterToHost message;
    proto::router::HostIdResponse* host_id_response = message.mutable_host_id_response();
    QByteArray key_hash;

    if (host_id_request.type() == proto::router::HostIdRequest::NEW_ID)
    {
        // Generate new key.
        std::string key = Random::string(kHostKeySize);

        // Calculate hash for key.
        key_hash = GenericHash::hash(GenericHash::Type::BLAKE2b512, key);

        if (!database.addHost(key_hash))
        {
            CLOG(ERROR) << "Unable to add host";
            return;
        }

        host_id_response->set_key(std::move(key));
    }
    else if (host_id_request.type() == proto::router::HostIdRequest::EXISTING_ID)
    {
        // Using existing key.
        key_hash = GenericHash::hash(GenericHash::Type::BLAKE2b512, host_id_request.key());
    }
    else
    {
        CLOG(ERROR) << "Unknown request type:" << host_id_request.type();
        return;
    }

    std::string_view error_code = database.hostId(key_hash, &host_id_);
    host_id_response->set_error_code(error_code);

    if (error_code == proto::router::kErrorOk)
    {
        host_id_response->set_host_id(host_id_);
        emit sig_hostIdAssigned(host_id_);
        Service::instance()->notifyChanged(Service::NOTIFY_HOSTS);
    }

    sendMessage(0, serialize(message));

    if (error_code != proto::router::kErrorOk)
        return;

    // If the host has a pending removal record, ignore connect metadata and reissue the remove
    // command; the destructor will finalize the hosts_remove row when this session disconnects.
    if (database.hasPendingHostRemoval(host_id_))
    {
        CLOG(INFO) << "Host" << host_id_ << "has pending removal, sending remove command";
        sendRemoveCommand();
        return;
    }

    if (!database.updateHostInfo(
            host_id_, computerName(), architecture(), version().toString(), osName(), address()))
    {
        CLOG(WARNING) << "Failed to update host info for host_id:" << host_id_;
    }
}
