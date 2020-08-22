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

namespace {

SharedKeyPool::RelayId createRelayId()
{
    static SharedKeyPool::RelayId relay_id = 0;
    ++relay_id;
    return relay_id;
}

} // namespace

SessionRelay::SessionRelay()
    : Session(proto::ROUTER_SESSION_RELAY),
      relay_id_(createRelayId())
{
    // Nothing
}

SessionRelay::~SessionRelay() = default;

uint32_t SessionRelay::poolSize() const
{
    return relayKeyPool().countForRelay(relay_id_);
}

void SessionRelay::onSessionReady()
{
    LOG(LS_INFO) << "Relay session ready (address: " << address()
                 << " relay_id: " << relay_id_ << ")";
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
    LOG(LS_INFO) << "Received key pool: " << key_pool.key_size()
                 << " (relay_id: " << relay_id_ << ")";

    SharedKeyPool& pool = relayKeyPool();

    for (int i = 0; i < key_pool.key_size(); ++i)
        pool.addKey(relay_id_, std::make_unique<proto::RelayKey>(key_pool.key(i)));
}

} // namespace router
