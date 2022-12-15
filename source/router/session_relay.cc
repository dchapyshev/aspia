//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "router/shared_key_pool.h"

namespace router {

SessionRelay::SessionRelay()
    : Session(proto::ROUTER_SESSION_RELAY)
{
    // Nothing
}

SessionRelay::~SessionRelay()
{
    relayKeyPool().removeKeysForRelay(sessionId());
}

void SessionRelay::sendKeyUsed(uint32_t key_id)
{
    std::unique_ptr<proto::RouterToRelay> message = std::make_unique<proto::RouterToRelay>();
    message->mutable_key_used()->set_key_id(key_id);
    sendMessage(*message);
}

void SessionRelay::onSessionReady()
{
    // Nothing
}

void SessionRelay::onMessageReceived(const base::ByteArray& buffer)
{
    std::unique_ptr<proto::RelayToRouter> message = std::make_unique<proto::RelayToRouter>();

    if (!base::parse(buffer, message.get()))
    {
        LOG(LS_ERROR) << "Could not read message from relay server";
        return;
    }

    if (message->has_key_pool())
    {
        readKeyPool(message->key_pool());
    }
    else if (message->has_relay_stat())
    {
        relay_stat_ = std::move(*message->mutable_relay_stat());
    }
    else
    {
        LOG(LS_WARNING) << "Unhandled message from relay server";
    }
}

void SessionRelay::onMessageWritten(size_t /* pending */)
{
    // Nothing
}

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
