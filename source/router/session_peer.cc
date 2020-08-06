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

#include "router/session_peer.h"

#include "base/logging.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "base/net/network_channel.h"
#include "router/database.h"

namespace router {

namespace {

const size_t kPeerKeySize = 512;

} // namespace

SessionPeer::SessionPeer(proto::RouterSession session_type,
                         std::unique_ptr<base::NetworkChannel> channel,
                         std::shared_ptr<DatabaseFactory> database_factory)
    : Session(std::move(channel), std::move(database_factory)),
      session_type_(session_type)
{
    // Nothing
}

SessionPeer::~SessionPeer() = default;

void SessionPeer::onMessageReceived(const base::ByteArray& buffer)
{
    proto::PeerToRouter message;

    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Could not read message from peer";
        return;
    }

    if (message.has_peer_id_request())
    {
        readPeerIdRequest(message.peer_id_request());
    }
    else
    {
        if (peer_id_ == peer::kInvalidPeerId)
        {
            LOG(LS_ERROR) << "Request could not be processed (peer ID not assigned yet)";
            return;
        }

        if (message.has_connection_request())
        {
            LOG(LS_INFO) << "CONNECTION REQUEST";
        }
        else if (message.has_connection_candidate())
        {
            LOG(LS_INFO) << "CONNECTION CANDIDATE";
        }
        else
        {
            LOG(LS_WARNING) << "Unhandled message from peer";
        }
    }
}

void SessionPeer::onMessageWritten(size_t /* pending */)
{
    // Nothing
}

void SessionPeer::readPeerIdRequest(const proto::PeerIdRequest& peer_id_request)
{
    if (peer_id_ != peer::kInvalidPeerId)
    {
        LOG(LS_ERROR) << "Peer ID already assigned";
        return;
    }

    std::unique_ptr<Database> database = openDatabase();
    if (!database)
    {
        LOG(LS_ERROR) << "Failed to connect to database";
        return;
    }

    proto::RouterToPeer message;
    proto::PeerIdResponse* peer_id_response = message.mutable_peer_id_response();
    base::ByteArray keyHash;

    if (peer_id_request.type() == proto::PeerIdRequest::NEW_ID)
    {
        // Generate new key.
        std::string key = base::Random::string(kPeerKeySize);

        // Calculate hash for key.
        keyHash = base::GenericHash::hash(base::GenericHash::Type::BLAKE2b512, key);

        if (!database->addPeer(keyHash))
        {
            LOG(LS_ERROR) << "Unable to add peer";
            return;
        }

        peer_id_response->set_key(std::move(key));
    }
    else if (peer_id_request.type() == proto::PeerIdRequest::EXISTING_ID)
    {
        // Using existing key.
        keyHash = base::GenericHash::hash(
            base::GenericHash::Type::BLAKE2b512, peer_id_request.key());
    }
    else
    {
        LOG(LS_ERROR) << "Unknown request type: " << peer_id_request.type();
        return;
    }

    peer_id_ = database->peerId(keyHash);
    if (peer_id_ == peer::kInvalidPeerId)
    {
        LOG(LS_ERROR) << "Failed to get peer ID";
        return;
    }

    peer_id_response->set_peer_id(peer_id_);
    sendMessage(message);
}

} // namespace router
