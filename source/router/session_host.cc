//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "router/session_host.h"

#include "base/logging.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "base/net/network_channel.h"
#include "router/database.h"
#include "router/server_proxy.h"

namespace router {

namespace {

const size_t kPeerKeySize = 512;

} // namespace

SessionHost::SessionHost()
    : Session(proto::ROUTER_SESSION_HOST)
{
    // Nothing
}

SessionHost::~SessionHost() = default;

void SessionHost::onSessionReady()
{
    // Nothing
}

void SessionHost::onMessageReceived(const base::ByteArray& buffer)
{
    proto::HostToRouter message;
    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Could not read message from host";
        return;
    }

    if (message.has_host_id_request())
    {
        readHostIdRequest(message.host_id_request());
    }
    else
    {
        if (host_id_ == base::kInvalidHostId)
        {
            LOG(LS_ERROR) << "Request could not be processed (host ID not assigned yet)";
            return;
        }

        LOG(LS_WARNING) << "Unhandled message from host";
    }
}

void SessionHost::onMessageWritten(size_t /* pending */)
{
    // Nothing
}

void SessionHost::readHostIdRequest(const proto::HostIdRequest& host_id_request)
{
    if (host_id_ != base::kInvalidHostId)
    {
        LOG(LS_ERROR) << "Host ID already assigned";
        return;
    }

    std::unique_ptr<Database> database = openDatabase();
    if (!database)
    {
        LOG(LS_ERROR) << "Failed to connect to database";
        return;
    }

    proto::RouterToHost message;
    proto::HostIdResponse* host_id_response = message.mutable_host_id_response();
    base::ByteArray keyHash;

    if (host_id_request.type() == proto::HostIdRequest::NEW_ID)
    {
        // Generate new key.
        std::string key = base::Random::string(kPeerKeySize);

        // Calculate hash for key.
        keyHash = base::GenericHash::hash(base::GenericHash::Type::BLAKE2b512, key);

        if (!database->addHost(keyHash))
        {
            LOG(LS_ERROR) << "Unable to add host";
            return;
        }

        host_id_response->set_key(std::move(key));
    }
    else if (host_id_request.type() == proto::HostIdRequest::EXISTING_ID)
    {
        // Using existing key.
        keyHash = base::GenericHash::hash(
            base::GenericHash::Type::BLAKE2b512, host_id_request.key());
    }
    else
    {
        LOG(LS_ERROR) << "Unknown request type: " << host_id_request.type();
        return;
    }

    host_id_ = database->hostId(keyHash);
    if (host_id_ == base::kInvalidHostId)
    {
        LOG(LS_ERROR) << "Failed to get host ID";
        return;
    }

    // Notify the server that the ID has been assigned.
    serverProxy().onHostSessionWithId(this);

    host_id_response->set_host_id(host_id_);
    sendMessage(message);
}

} // namespace router
