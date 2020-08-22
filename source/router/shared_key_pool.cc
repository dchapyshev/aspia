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

#include "router/shared_key_pool.h"

#include "proto/router_common.pb.h"

#include <map>

namespace router {

class SharedKeyPool::Impl
{
public:
    explicit Impl(Delegate* delegate);
    ~Impl() = default;

    void dettach();

    void addKey(RelayId relay_id, Key&& key);
    Key takeKey();
    void removeKeysForRelay(RelayId relay_id);
    void clear();
    size_t countForRelay(RelayId relay_id) const;
    size_t count() const;
    bool isEmpty() const;

private:
    using RelayPool = std::vector<Key>;

    std::map<RelayId, RelayPool> pool_;
    Delegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

SharedKeyPool::Impl::Impl(Delegate* delegate)
    : delegate_(delegate)
{
    // Nothing
}

void SharedKeyPool::Impl::dettach()
{
    delegate_ = nullptr;
}

void SharedKeyPool::Impl::addKey(RelayId relay_id, Key&& key)
{
    auto relay = pool_.find(relay_id);
    if (relay == pool_.end())
        relay = pool_.emplace(relay_id, RelayPool()).first;

    relay->second.emplace_back(std::move(key));
}

SharedKeyPool::Key SharedKeyPool::Impl::takeKey()
{
    auto preffered_relay = pool_.end();
    size_t max_count = 0;

    for (auto it = pool_.begin(); it != pool_.end(); ++it)
    {
        size_t count = it->second.size();
        if (count > max_count)
        {
            preffered_relay = it;
            max_count = count;
        }

        ++it;
    }

    if (preffered_relay == pool_.end())
        return nullptr;

    Key result = std::move(preffered_relay->second.back());

    // Removing the key from the pool.
    preffered_relay->second.pop_back();

    if (preffered_relay->second.empty())
    {
        pool_.erase(preffered_relay->first);

        // Notify that the pool for the relay is empty.
        if (delegate_)
            delegate_->onKeyPoolEmpty(preffered_relay->first);
    }

    return result;
}

void SharedKeyPool::Impl::removeKeysForRelay(RelayId relay_id)
{
    pool_.erase(relay_id);
}

void SharedKeyPool::Impl::clear()
{
    pool_.clear();
}

size_t SharedKeyPool::Impl::countForRelay(RelayId relay_id) const
{
    auto result = pool_.find(relay_id);
    if (result == pool_.end())
        return 0;

    return result->second.size();
}

size_t SharedKeyPool::Impl::count() const
{
    size_t result = 0;

    for (const auto& relay : pool_)
        result += relay.second.size();

    return result;
}

bool SharedKeyPool::Impl::isEmpty() const
{
    return pool_.empty();
}

SharedKeyPool::SharedKeyPool(Delegate* delegate)
    : impl_(std::make_shared<Impl>(delegate)),
      is_primary_(true)
{
    // Nothing
}

SharedKeyPool::SharedKeyPool(std::shared_ptr<Impl> impl)
    : impl_(std::move(impl)),
      is_primary_(false)
{
    // Nothing
}

SharedKeyPool::~SharedKeyPool()
{
    if (is_primary_)
        impl_->dettach();
}

std::unique_ptr<SharedKeyPool> SharedKeyPool::share()
{
    return std::unique_ptr<SharedKeyPool>(new SharedKeyPool(impl_));
}

void SharedKeyPool::addKey(RelayId relay_id, Key&& key)
{
    impl_->addKey(relay_id, std::move(key));
}

SharedKeyPool::Key SharedKeyPool::takeKey()
{
    return impl_->takeKey();
}

void SharedKeyPool::removeKeysForRelay(RelayId relay_id)
{
    impl_->removeKeysForRelay(relay_id);
}

void SharedKeyPool::clear()
{
    impl_->clear();
}

size_t SharedKeyPool::countForRelay(RelayId relay_id) const
{
    return impl_->countForRelay(relay_id);
}

size_t SharedKeyPool::count() const
{
    return impl_->count();
}

bool SharedKeyPool::isEmpty() const
{
    return impl_->isEmpty();
}

} // namespace router
