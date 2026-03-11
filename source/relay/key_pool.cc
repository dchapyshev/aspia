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
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "relay/key_pool.h"

#include "base/logging.h"
#include "relay/key_factory.h"

namespace relay {

//--------------------------------------------------------------------------------------------------
KeyPool::KeyPool(KeyFactory* factory)
    : factory_(factory)
{
    LOG(INFO) << "Ctor";
    DCHECK(factory_);
}

//--------------------------------------------------------------------------------------------------
KeyPool::~KeyPool()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void KeyPool::dettach()
{
    LOG(INFO) << "Pool is dettached";
    factory_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
quint32 KeyPool::addKey(SessionKey&& session_key)
{
    quint32 key_id = current_key_id_++;
    map_.try_emplace(key_id, std::move(session_key));

    LOG(INFO) << "Key with id" << key_id << "added to pool";
    return key_id;
}

//--------------------------------------------------------------------------------------------------
bool KeyPool::removeKey(quint32 key_id)
{
    auto result = map_.find(key_id);
    if (result != map_.end())
    {
        map_.erase(result);

        LOG(INFO) << "Key with id" << key_id << "removed from pool";
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void KeyPool::setKeyExpired(quint32 key_id)
{
    if (removeKey(key_id))
    {
        LOG(INFO) << "Key with ID" << key_id << "expired. It has been removed";
        if (factory_)
            emit factory_->sig_keyExpired(key_id);
    }
}

//--------------------------------------------------------------------------------------------------
std::optional<KeyPool::Key> KeyPool::key(quint32 key_id, const std::string& peer_public_key) const
{
    auto result = map_.find(key_id);
    if (result == map_.end())
        return std::nullopt;

    const SessionKey& session_key = result->second;
    return std::make_pair(session_key.sessionKey(peer_public_key), session_key.iv());
}

//--------------------------------------------------------------------------------------------------
void KeyPool::clear()
{
    LOG(INFO) << "Key pool cleared";
    map_.clear();
}

} // namespace relay
