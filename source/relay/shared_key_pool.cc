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

#include "relay/shared_key_pool.h"

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
// static
SharedKeyPool& SharedKeyPool::instance()
{
    static SharedKeyPool instance;
    return instance;
}

//--------------------------------------------------------------------------------------------------
quint32 SharedKeyPool::add(SessionKey&& session_key)
{
    std::unique_lock lock(lock_);

    quint32 key_id = key_counter_++;
    keys_.try_emplace(key_id, std::move(session_key));

    LOG(INFO) << "Key with id" << key_id << "added to pool";
    return key_id;
}

//--------------------------------------------------------------------------------------------------
bool SharedKeyPool::remove(quint32 key_id)
{
    std::unique_lock lock(lock_);

    auto result = keys_.find(key_id);
    if (result == keys_.end())
        return false;

    keys_.erase(result);
    LOG(INFO) << "Key with id" << key_id << "removed from pool";
    return true;
}

//--------------------------------------------------------------------------------------------------
std::optional<SharedKeyPool::Key> SharedKeyPool::find(
    quint32 key_id, const std::string& peer_public_key) const
{
    std::shared_lock lock(lock_);

    auto result = keys_.find(key_id);
    if (result == keys_.end())
        return std::nullopt;

    const SessionKey& session_key = result->second;
    return std::make_pair(session_key.sessionKey(peer_public_key), session_key.iv());
}

//--------------------------------------------------------------------------------------------------
size_t SharedKeyPool::count() const
{
    std::shared_lock lock(lock_);
    return keys_.size();
}

//--------------------------------------------------------------------------------------------------
void SharedKeyPool::clear()
{
    std::unique_lock lock(lock_);

    LOG(INFO) << "Key pool cleared";
    keys_.clear();
}
