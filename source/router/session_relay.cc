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

namespace router {

namespace {

const uint32_t kDefaultPoolSize = 25;

} // namespace

SessionRelay::SessionRelay(std::unique_ptr<base::NetworkChannel> channel,
                           std::shared_ptr<DatabaseFactory> database_factory)
    : Session(proto::ROUTER_SESSION_RELAY, std::move(channel), std::move(database_factory))
{
    // Nothing
}

SessionRelay::~SessionRelay() = default;

uint32_t SessionRelay::poolSize() const
{
    return pool_.size();
}

void SessionRelay::onSessionReady()
{
    sendKeyPoolRequest(kDefaultPoolSize);
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

void SessionRelay::sendKeyPoolRequest(uint32_t pool_size)
{
    LOG(LS_INFO) << "Send key pool request: " << pool_size;

    proto::RouterToRelay message;
    message.mutable_key_pool_request()->set_pool_size(pool_size);
    sendMessage(message);
}

void SessionRelay::readKeyPool(const proto::RelayKeyPool& key_pool)
{
    LOG(LS_INFO) << "Received key pool: " << key_pool.key_size();

    for (int i = 0; i < key_pool.key_size(); ++i)
        pool_.push_back(key_pool.key(i));
}

} // namespace router
