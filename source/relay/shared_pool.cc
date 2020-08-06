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

#include "relay/shared_pool.h"

namespace relay {

namespace {

const SessionKey kInvalidKey;

} // namespace

class SharedPool::Pool
{
public:
    Pool() = default;
    ~Pool() = default;

    uint32_t addKey(uint32_t controller_id, SessionKey&& session_key);
    void removeKey(uint32_t key_id);
    void removeKeysForController(uint32_t controller_id);
    const SessionKey& key(uint32_t key_id) const;

private:
    struct Entry
    {
        Entry(uint32_t controller_id, SessionKey&& session_key) noexcept
            : controller_id(controller_id),
              session_key(std::move(session_key))
        {
            // Nothing
        }

        Entry(Entry&& other) noexcept
            : controller_id(other.controller_id),
              session_key(std::move(other.session_key))
        {
            // Nothing
        }

        Entry& operator=(Entry&& other) noexcept
        {
            if (&other != this)
            {
                controller_id = other.controller_id;
                session_key = std::move(other.session_key);
            }

            return *this;
        }

        uint32_t controller_id;
        SessionKey session_key;
    };

    std::map<uint32_t, Entry> map_;
    uint32_t current_key_id_ = 0;

    DISALLOW_COPY_AND_ASSIGN(Pool);
};

uint32_t SharedPool::Pool::addKey(uint32_t controller_id, SessionKey&& session_key)
{
    uint32_t key_id = current_key_id_++;
    map_.emplace(key_id, Entry(controller_id, std::move(session_key)));
    return key_id;
}

void SharedPool::Pool::removeKey(uint32_t key_id)
{
    map_.erase(key_id);
}

void SharedPool::Pool::removeKeysForController(uint32_t controller_id)
{
    for (auto it = map_.begin(); it != map_.end();)
    {
        if (it->second.controller_id == controller_id)
        {
            it = map_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

const SessionKey& SharedPool::Pool::key(uint32_t key_id) const
{
    auto result = map_.find(key_id);
    if (result == map_.end())
        return kInvalidKey;

    return result->second.session_key;
}

SharedPool::SharedPool()
    : pool_(std::make_shared<Pool>())
{
    // Nothing
}

SharedPool::SharedPool(std::shared_ptr<Pool> pool)
    : pool_(std::move(pool))
{
    // Nothing
}

SharedPool::~SharedPool() = default;

std::unique_ptr<SharedPool> SharedPool::share()
{
    return std::unique_ptr<SharedPool>(new SharedPool(pool_));
}

uint32_t SharedPool::addKey(uint32_t controller_id, SessionKey&& session_key)
{
    return pool_->addKey(controller_id, std::move(session_key));
}

void SharedPool::removeKey(uint32_t key_id)
{
    pool_->removeKey(key_id);
}

void SharedPool::removeKeysForController(uint32_t controller_id)
{
    pool_->removeKeysForController(controller_id);
}

const SessionKey& SharedPool::key(uint32_t key_id) const
{
    return pool_->key(key_id);
}

} // namespace relay
