//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "relay/shared_pool.h"

#include "base/logging.h"

#include <map>
#include <mutex>

namespace relay {

class SharedPool::Pool
{
public:
    explicit Pool(Delegate* delegate);
    ~Pool() = default;

    void dettach();

    uint32_t addKey(SessionKey&& session_key);
    bool removeKey(uint32_t key_id);
    void setKeyExpired(uint32_t key_id);
    std::optional<Key> key(uint32_t key_id, std::string_view peer_public_key) const;
    void clear();

private:
    Delegate* delegate_;

    mutable std::mutex pool_lock_;
    std::map<uint32_t, SessionKey> map_;
    uint32_t current_key_id_ = 0;

    DISALLOW_COPY_AND_ASSIGN(Pool);
};

//--------------------------------------------------------------------------------------------------
SharedPool::Pool::Pool(Delegate* delegate)
    : delegate_(delegate)
{
    DCHECK(delegate_);
}

//--------------------------------------------------------------------------------------------------
void SharedPool::Pool::dettach()
{
    delegate_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
uint32_t SharedPool::Pool::addKey(SessionKey&& session_key)
{
    std::scoped_lock lock(pool_lock_);

    uint32_t key_id = current_key_id_++;
    map_.emplace(key_id, std::move(session_key));

    LOG(LS_INFO) << "Key with id " << key_id << " added to pool";
    return key_id;
}

//--------------------------------------------------------------------------------------------------
bool SharedPool::Pool::removeKey(uint32_t key_id)
{
    std::scoped_lock lock(pool_lock_);

    auto result = map_.find(key_id);
    if (result != map_.end())
    {
        map_.erase(result);

        LOG(LS_INFO) << "Key with id " << key_id << " removed from pool";
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void SharedPool::Pool::setKeyExpired(uint32_t key_id)
{
    if (removeKey(key_id))
    {
        LOG(LS_INFO) << "Key with ID " << key_id << " expired. It has been removed";

        if (delegate_)
            delegate_->onPoolKeyExpired(key_id);
    }
}

//--------------------------------------------------------------------------------------------------
std::optional<SharedPool::Key> SharedPool::Pool::key(
    uint32_t key_id, std::string_view peer_public_key) const
{
    std::scoped_lock lock(pool_lock_);

    auto result = map_.find(key_id);
    if (result == map_.end())
        return std::nullopt;

    const SessionKey& session_key = result->second;
    return std::make_pair(session_key.sessionKey(peer_public_key), session_key.iv());
}

//--------------------------------------------------------------------------------------------------
void SharedPool::Pool::clear()
{
    std::scoped_lock lock(pool_lock_);

    LOG(LS_INFO) << "Key pool cleared";
    map_.clear();
}

//--------------------------------------------------------------------------------------------------
SharedPool::SharedPool(Delegate* delegate)
    : pool_(std::make_shared<Pool>(delegate)),
      is_primary_(true)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SharedPool::SharedPool(std::shared_ptr<Pool> pool)
    : pool_(std::move(pool)),
      is_primary_(false)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SharedPool::~SharedPool()
{
    if (is_primary_)
        pool_->dettach();
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<SharedPool> SharedPool::share()
{
    return std::unique_ptr<SharedPool>(new SharedPool(pool_));
}

//--------------------------------------------------------------------------------------------------
uint32_t SharedPool::addKey(SessionKey&& session_key)
{
    return pool_->addKey(std::move(session_key));
}

//--------------------------------------------------------------------------------------------------
bool SharedPool::removeKey(uint32_t key_id)
{
    return pool_->removeKey(key_id);
}

//--------------------------------------------------------------------------------------------------
void SharedPool::setKeyExpired(uint32_t key_id)
{
    return pool_->setKeyExpired(key_id);
}

//--------------------------------------------------------------------------------------------------
std::optional<SharedPool::Key> SharedPool::key(
    uint32_t key_id, std::string_view peer_public_key) const
{
    return pool_->key(key_id, peer_public_key);
}

//--------------------------------------------------------------------------------------------------
void SharedPool::clear()
{
    pool_->clear();
}

} // namespace relay
