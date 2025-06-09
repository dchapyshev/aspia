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
#include "router/key_pool.h"

namespace router {

//--------------------------------------------------------------------------------------------------
SessionRelay::SessionRelay(QObject* parent)
    : Session(proto::router::SESSION_TYPE_RELAY, parent)
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
void SessionRelay::sendKeyUsed(quint32 key_id)
{
    outgoing_message_.newMessage().mutable_key_used()->set_key_id(key_id);
    sendMessage(proto::router::CHANNEL_ID_SESSION, outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void SessionRelay::disconnectPeerSession(const proto::router::PeerConnectionRequest& request)
{
    outgoing_message_.newMessage().mutable_peer_connection_request()->CopyFrom(request);
    sendMessage(proto::router::CHANNEL_ID_SESSION, outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void SessionRelay::onSessionReady()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SessionRelay::onSessionMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    if (!incoming_message_.parse(buffer))
    {
        LOG(LS_ERROR) << "Could not read message from relay server";
        return;
    }

    if (incoming_message_->has_key_pool())
    {
        readKeyPool(incoming_message_->key_pool());
    }
    else if (incoming_message_->has_relay_stat())
    {
        relay_stat_ = std::move(*incoming_message_->mutable_relay_stat());
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message from relay server";
    }
}

//--------------------------------------------------------------------------------------------------
void SessionRelay::onSessionMessageWritten(quint8 /* channel_id */, size_t /* pending */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SessionRelay::readKeyPool(const proto::router::RelayKeyPool& key_pool)
{
    KeyPool& pool = relayKeyPool();

    LOG(LS_INFO) << "Received key pool: " << key_pool.key_size() << " (" << address() << ")";

    peer_data_.emplace(std::make_pair(
        key_pool.peer_host(), static_cast<quint16>(key_pool.peer_port())));

    for (int i = 0; i < key_pool.key_size(); ++i)
        pool.addKey(sessionId(), key_pool.key(i));
}

} // namespace router
