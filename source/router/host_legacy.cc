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

#include "router/host_legacy.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/generic_hash.h"
#include "proto/router_constants.h"
#include "proto/router_legacy_host.h"
#include "router/database.h"
#include "router/workers/client_worker.h"

namespace {

constexpr qsizetype kMaxHostIdsPerSession = 32;

} // namespace

//--------------------------------------------------------------------------------------------------
HostLegacy::HostLegacy(TcpChannel* channel, QObject* parent)
    : Host(channel, parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
HostLegacy::~HostLegacy()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool HostLegacy::hasHostId(HostId host_id) const
{
    return host_id_list_.contains(host_id);
}

//--------------------------------------------------------------------------------------------------
bool HostLegacy::removeHostId(HostId host_id)
{
    for (auto it = host_id_list_.begin(), it_end = host_id_list_.end(); it != it_end; ++it)
    {
        if (*it == host_id)
        {
            CLOG(INFO) << "Host ID" << host_id << "removed from legacy session list";
            host_id_list_.erase(it);
            emit sig_notifyChanged(ClientWorker::NOTIFY_HOSTS);
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void HostLegacy::sendConnectionOffer(const proto::router::legacy::ConnectionOffer& offer)
{
    proto::router::legacy::RouterToPeer message;
    message.mutable_connection_offer()->CopyFrom(offer);
    sendMessage(0, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void HostLegacy::onSessionMessage(quint8 /* channel_id */, const QByteArray& buffer)
{
    proto::router::legacy::PeerToRouter message;
    if (!parse(buffer, &message))
    {
        CLOG(ERROR) << "Could not read message from host";
        return;
    }

    if (message.has_host_id_request())
        readHostIdRequest(message.host_id_request());
    else if (message.has_reset_host_id())
        readResetHostId(message.reset_host_id());
    else
        CLOG(ERROR) << "Unhandled message from host";
}

//--------------------------------------------------------------------------------------------------
void HostLegacy::readHostIdRequest(const proto::router::legacy::HostIdRequest& host_id_request)
{
    if (host_id_list_.size() >= kMaxHostIdsPerSession)
    {
        CLOG(ERROR) << "Rejecting host id request: per-session limit of" << kMaxHostIdsPerSession
                    << "reached";
        return;
    }

    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return;
    }

    proto::router::legacy::RouterToPeer message;
    proto::router::legacy::HostIdResponse* host_id_response = message.mutable_host_id_response();

    // Legacy hosts are no longer issued new ids; they may only present an existing one. Reject a
    // NEW_ID (or any non-existing) request so no host row is ever created for a legacy peer.
    if (host_id_request.type() != proto::router::legacy::HostIdRequest::EXISTING_ID)
    {
        CLOG(ERROR) << "Rejecting host id request from legacy host (type:"
                    << host_id_request.type() << "); only existing ids are allowed";
        host_id_response->set_error_code(proto::router::legacy::HostIdResponse::UNKNOWN);
        sendMessage(0, serialize(message));
        return;
    }

    const QByteArray key_hash = GenericHash::hash(GenericHash::Type::BLAKE2b512, host_id_request.key());

    HostId host_id = kInvalidHostId;
    std::string_view error_code = database.hostId(key_hash, &host_id);

    if (error_code == proto::router::kErrorOk)
    {
        // Legacy protocol has no router->host remove command. A reconnecting host with a
        // pending removal must not become online again; consume the pending row and tell the
        // legacy peer that the old id is gone.
        if (database.hasPendingHostRemoval(host_id))
        {
            CLOG(INFO) << "Legacy host" << host_id << "has pending removal";
            if (database.finalizeHostRemoval(host_id))
            {
                host_id_response->set_error_code(proto::router::legacy::HostIdResponse::NO_HOST_FOUND);
                emit sig_notifyChanged(ClientWorker::NOTIFY_HOSTS);
            }
            else
            {
                CLOG(ERROR) << "Failed to finalize pending removal for legacy host" << host_id;
                host_id_response->set_error_code(proto::router::legacy::HostIdResponse::UNKNOWN);
            }

            sendMessage(0, serialize(message));
            return;
        }

        host_id_response->set_error_code(proto::router::legacy::HostIdResponse::SUCCESS);
        host_id_response->set_host_id(host_id);

        if (!host_id_list_.contains(host_id))
        {
            host_id_list_.emplace_back(host_id);
            emit sig_hostIdAssigned(host_id);
            emit sig_notifyChanged(ClientWorker::NOTIFY_HOSTS);
        }
    }
    else if (error_code == proto::router::kErrorNotFound)
    {
        host_id_response->set_error_code(proto::router::legacy::HostIdResponse::NO_HOST_FOUND);
    }
    else
    {
        host_id_response->set_error_code(proto::router::legacy::HostIdResponse::UNKNOWN);
    }

    sendMessage(0, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void HostLegacy::readResetHostId(const proto::router::legacy::ResetHostId& reset_host_id)
{
    HostId host_id = reset_host_id.host_id();
    if (host_id == kInvalidHostId)
    {
        CLOG(ERROR) << "Invalid host ID";
        return;
    }

    if (host_id_list_.isEmpty())
    {
        CLOG(ERROR) << "Empty host ID list";
        return;
    }

    if (!removeHostId(host_id))
        CLOG(ERROR) << "Host ID" << host_id << "is NOT found in list";
}
