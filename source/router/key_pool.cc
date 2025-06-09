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

#include "router/key_pool.h"

#include "base/logging.h"
#include "router/key_factory.h"

namespace router {

//--------------------------------------------------------------------------------------------------
KeyPool::KeyPool(KeyFactory* factory)
    : factory_(factory)
{
    DCHECK(factory_);
}

//--------------------------------------------------------------------------------------------------
void KeyPool::dettach()
{
    factory_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void KeyPool::addKey(Session::SessionId session_id, const proto::router::RelayKey& key)
{
    auto relay = pool_.find(session_id);
    if (relay == pool_.end())
    {
        LOG(LS_INFO) << "Host not found in pool. It will be added";
        relay = pool_.insert(session_id, Keys());
    }

    LOG(LS_INFO) << "Added key with id " << key.key_id() << " for host '" << session_id << "'";
    relay.value().append(key);
}

//--------------------------------------------------------------------------------------------------
std::optional<KeyPool::Credentials> KeyPool::takeCredentials()
{
    if (pool_.empty())
    {
        LOG(LS_ERROR) << "Empty key pool";
        return std::nullopt;
    }

    auto preffered_relay = pool_.end();
    int max_count = 0;

    for (auto it = pool_.begin(), it_end = pool_.end(); it != it_end; ++it)
    {
        int count = it.value().size();
        if (count > max_count)
        {
            preffered_relay = it;
            max_count = count;
        }
    }

    if (preffered_relay == pool_.end())
    {
        LOG(LS_ERROR) << "Empty key pool";
        return std::nullopt;
    }

    LOG(LS_INFO) << "Preffered relay: " << preffered_relay.key();

    QList<proto::router::RelayKey>& keys = preffered_relay.value();
    if (keys.isEmpty())
    {
        LOG(LS_ERROR) << "Empty key pool for relay";
        return std::nullopt;
    }

    Credentials credentials;
    credentials.session_id = preffered_relay.key();
    credentials.key = std::move(keys.back());

    // Removing the key from the pool.
    keys.pop_back();

    if (keys.isEmpty())
    {
        LOG(LS_INFO) << "Last key in the pool for relay. The relay will be removed from the pool";
        pool_.remove(preffered_relay.key());
    }

    if (factory_)
        emit factory_->sig_keyUsed(credentials.session_id, credentials.key.key_id());

    return std::move(credentials);
}

//--------------------------------------------------------------------------------------------------
void KeyPool::removeKeysForRelay(Session::SessionId session_id)
{
    LOG(LS_INFO) << "All keys for relay '" << session_id << "' removed";
    pool_.remove(session_id);
}

//--------------------------------------------------------------------------------------------------
void KeyPool::clear()
{
    LOG(LS_INFO) << "Key pool cleared";
    pool_.clear();
}

//--------------------------------------------------------------------------------------------------
size_t KeyPool::countForRelay(Session::SessionId session_id) const
{
    auto result = pool_.find(session_id);
    if (result == pool_.end())
        return 0;

    return result.value().size();
}

//--------------------------------------------------------------------------------------------------
size_t KeyPool::count() const
{
    size_t result = 0;

    for (const auto& relay : std::as_const(pool_))
        result += relay.size();

    return result;
}

//--------------------------------------------------------------------------------------------------
bool KeyPool::isEmpty() const
{
    return pool_.isEmpty();
}

} // namespace router
