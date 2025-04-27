//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "router/session_relay.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "router/shared_key_pool.h"

namespace router {

//--------------------------------------------------------------------------------------------------
SessionRelay::SessionRelay()
    : Session(proto::ROUTER_SESSION_RELAY)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
SessionRelay::~SessionRelay()
{
    LOG(LS_INFO) << "Dtor";
    relayKeyPool().removeKeysForRelay(sessionId());
}

//--------------------------------------------------------------------------------------------------
void SessionRelay::sendKeyUsed(uint32_t key_id)
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_key_used()->set_key_id(key_id);
    sendMessage(proto::ROUTER_CHANNEL_ID_SESSION, outgoing_message_);
}

//--------------------------------------------------------------------------------------------------
void SessionRelay::disconnectPeerSession(const proto::PeerConnectionRequest& request)
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_peer_connection_request()->CopyFrom(request);
    sendMessage(proto::ROUTER_CHANNEL_ID_SESSION, outgoing_message_);
}

//--------------------------------------------------------------------------------------------------
void SessionRelay::onSessionReady()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SessionRelay::onSessionMessageReceived(uint8_t /* channel_id */, const QByteArray& buffer)
{
    incoming_message_.Clear();

    if (!base::parse(buffer, &incoming_message_))
    {
        LOG(LS_ERROR) << "Could not read message from relay server";
        return;
    }

    if (incoming_message_.has_key_pool())
    {
        readKeyPool(incoming_message_.key_pool());
    }
    else if (incoming_message_.has_relay_stat())
    {
        relay_stat_ = std::move(*incoming_message_.mutable_relay_stat());
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message from relay server";
    }
}

//--------------------------------------------------------------------------------------------------
void SessionRelay::onSessionMessageWritten(uint8_t /* channel_id */, size_t /* pending */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SessionRelay::readKeyPool(const proto::RelayKeyPool& key_pool)
{
    SharedKeyPool& pool = relayKeyPool();

    LOG(LS_INFO) << "Received key pool: " << key_pool.key_size() << " (" << address() << ")";

    peer_data_.emplace(std::make_pair(
        key_pool.peer_host(), static_cast<uint16_t>(key_pool.peer_port())));

    for (int i = 0; i < key_pool.key_size(); ++i)
        pool.addKey(sessionId(), key_pool.key(i));
}

} // namespace router
