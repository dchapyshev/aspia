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

#include "router/host_id_handler.h"

#include "base/logging.h"
#include "base/crypto/generic_hash.h"
#include "proto/router_constants.h"
#include "proto/router_host.h"
#include "router/database.h"
#include "router/workers/client_worker.h"

namespace {

// The hardware id is opaque to the router and only ever stored, so the single rule about it is a
// bound: an unauthenticated peer must not be able to push an arbitrary blob into the database.
constexpr size_t kMaxHardwareIdSize = 64;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
HostIdHandler::Result HostIdHandler::handle(Database& database,
                                            const proto::router::HostIdRequest& request,
                                            const Peer& peer, HostId current_host_id)
{
    Result result;

    // A host requests its id exactly once per session. Reject repeats so an untrusted host cannot
    // overwrite the assigned id and desync the pending-removal finalization the session does when
    // it ends. A failed request leaves the id unassigned, so a legitimate retry still works.
    if (current_host_id != kInvalidHostId)
    {
        LOG(ERROR) << "Ignoring repeated host id request; host id" << current_host_id
                   << "already assigned";
        result.action = Action::IGNORE;
        return result;
    }

    if (request.hw_id().size() > kMaxHardwareIdSize)
    {
        LOG(ERROR) << "Host reported an oversized hardware id (" << request.hw_id().size()
                   << "bytes); disconnecting";
        result.action = Action::CLOSE;
        return result;
    }

    result.hardware_id = QByteArray::fromStdString(request.hw_id());
    if (result.hardware_id.isEmpty())
    {
        LOG(ERROR) << "Host did not report a hardware id; disconnecting";
        result.action = Action::CLOSE;
        return result;
    }

    if (request.type() == proto::router::HostIdRequest::NEW_ID)
    {
        // A new host is not persisted until an administrator approves it: it gets a temporary id
        // and a freshly generated key, and nothing is written to the database. If it reconnects
        // before the approval, its key is not found and it simply asks for a new id again.
        result.action = Action::ISSUE_TEMP_ID;
        result.notify_flags = ClientWorker::NOTIFY_TEMP_HOSTS;
        return result;
    }

    if (request.type() != proto::router::HostIdRequest::EXISTING_ID)
    {
        LOG(ERROR) << "Unknown request type:" << request.type();
        result.action = Action::IGNORE;
        return result;
    }

    if (!database.isValid())
    {
        LOG(ERROR) << "Failed to connect to database";
        result.action = Action::IGNORE;
        return result;
    }

    const QByteArray key_hash =
        GenericHash::hash(GenericHash::Type::BLAKE2b512, request.key());

    HostId host_id = kInvalidHostId;
    const std::string_view error_code = database.hostId(key_hash, &host_id);

    result.action = Action::SEND_RESPONSE;
    result.error_code = error_code;

    if (error_code != proto::router::kErrorOk)
        return result;

    result.host_id = host_id;
    result.notify_flags = ClientWorker::NOTIFY_HOSTS;

    // A host whose removal is scheduled is told to uninstall itself instead of being welcomed
    // back: its connect metadata is not stored, the record is on its way out.
    if (database.hasPendingHostRemoval(host_id))
    {
        LOG(INFO) << "Host" << host_id << "has pending removal, sending remove command";
        result.removal_pending = true;
        return result;
    }

    if (!database.updateHostInfo(host_id, result.hardware_id, peer.computer_name,
                                 peer.architecture, peer.version, peer.os_name, peer.address))
    {
        LOG(WARNING) << "Failed to update host info for host_id:" << host_id;
    }

    return result;
}
