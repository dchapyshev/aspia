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
    relayKeyPool().removeKeysForRelay(host_);
}

void SessionRelay::sendKeyUsed(uint32_t key_id)
{
    proto::RouterToRelay message;
    message.mutable_key_used()->set_key_id(key_id);
    sendMessage(message);
}

void SessionRelay::onSessionReady()
{
    // Nothing
}

void SessionRelay::onMessageReceived(const base::ByteArray& buffer)
{
    proto::RelayToRouter message;

    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Could not read message from relay server";
        return;
    }

    if (message.has_key_pool())
    {
        readKeyPool(message.key_pool());
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

    host_ = key_pool.peer_host();
    uint16_t port = key_pool.peer_port();

    for (int i = 0; i < key_pool.key_size(); ++i)
    {
        pool.addKey(host_, port, key_pool.key(i));
    }
}

} // namespace router
