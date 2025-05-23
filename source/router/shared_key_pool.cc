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

#include "router/shared_key_pool.h"

#include "base/logging.h"

#include <map>

namespace router {

class SharedKeyPool::Impl
{
public:
    explicit Impl(Delegate* delegate);
    ~Impl() = default;

    void dettach();

    void addKey(Session::SessionId session_id, const proto::RelayKey& key);
    std::optional<Credentials> takeCredentials();
    void removeKeysForRelay(Session::SessionId session_id);
    void clear();
    size_t countForRelay(Session::SessionId session_id) const;
    size_t count() const;
    bool isEmpty() const;

private:
    using Keys = std::vector<proto::RelayKey>;

    std::map<Session::SessionId, Keys> pool_;
    Delegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

//--------------------------------------------------------------------------------------------------
SharedKeyPool::Impl::Impl(Delegate* delegate)
    : delegate_(delegate)
{
    DCHECK(delegate_);
}

//--------------------------------------------------------------------------------------------------
void SharedKeyPool::Impl::dettach()
{
    delegate_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void SharedKeyPool::Impl::addKey(Session::SessionId session_id, const proto::RelayKey& key)
{
    auto relay = pool_.find(session_id);
    if (relay == pool_.end())
    {
        LOG(LS_INFO) << "Host not found in pool. It will be added";
        relay = pool_.emplace(session_id, Keys()).first;
    }

    LOG(LS_INFO) << "Added key with id " << key.key_id() << " for host '" << session_id << "'";
    relay->second.emplace_back(key);
}

//--------------------------------------------------------------------------------------------------
std::optional<SharedKeyPool::Credentials> SharedKeyPool::Impl::takeCredentials()
{
    if (pool_.empty())
    {
        LOG(LS_ERROR) << "Empty key pool";
        return std::nullopt;
    }

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
    }

    if (preffered_relay == pool_.end())
    {
        LOG(LS_ERROR) << "Empty key pool";
        return std::nullopt;
    }

    LOG(LS_INFO) << "Preffered relay: " << preffered_relay->first;

    std::vector<proto::RelayKey>& keys = preffered_relay->second;
    if (keys.empty())
    {
        LOG(LS_ERROR) << "Empty key pool for relay";
        return std::nullopt;
    }

    Credentials credentials;
    credentials.session_id = preffered_relay->first;
    credentials.key = std::move(keys.back());

    // Removing the key from the pool.
    keys.pop_back();

    if (keys.empty())
    {
        LOG(LS_INFO) << "Last key in the pool for relay. The relay will be removed from the pool";
        pool_.erase(preffered_relay->first);
    }

    if (delegate_)
        delegate_->onPoolKeyUsed(credentials.session_id, credentials.key.key_id());

    return std::move(credentials);
}

//--------------------------------------------------------------------------------------------------
void SharedKeyPool::Impl::removeKeysForRelay(Session::SessionId session_id)
{
    LOG(LS_INFO) << "All keys for relay '" << session_id << "' removed";
    pool_.erase(session_id);
}

//--------------------------------------------------------------------------------------------------
void SharedKeyPool::Impl::clear()
{
    LOG(LS_INFO) << "Key pool cleared";
    pool_.clear();
}

//--------------------------------------------------------------------------------------------------
size_t SharedKeyPool::Impl::countForRelay(Session::SessionId session_id) const
{
    auto result = pool_.find(session_id);
    if (result == pool_.end())
        return 0;

    return result->second.size();
}

//--------------------------------------------------------------------------------------------------
size_t SharedKeyPool::Impl::count() const
{
    size_t result = 0;

    for (const auto& relay : pool_)
        result += relay.second.size();

    return result;
}

//--------------------------------------------------------------------------------------------------
bool SharedKeyPool::Impl::isEmpty() const
{
    return pool_.empty();
}

//--------------------------------------------------------------------------------------------------
SharedKeyPool::SharedKeyPool(Delegate* delegate)
    : impl_(new Impl(delegate)),
      is_primary_(true)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SharedKeyPool::SharedKeyPool(base::SharedPointer<Impl> impl)
    : impl_(std::move(impl)),
      is_primary_(false)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SharedKeyPool::~SharedKeyPool()
{
    if (is_primary_)
        impl_->dettach();
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<SharedKeyPool> SharedKeyPool::share()
{
    return std::unique_ptr<SharedKeyPool>(new SharedKeyPool(impl_));
}

//--------------------------------------------------------------------------------------------------
void SharedKeyPool::addKey(Session::SessionId session_id, const proto::RelayKey& key)
{
    impl_->addKey(session_id, key);
}

//--------------------------------------------------------------------------------------------------
std::optional<SharedKeyPool::Credentials> SharedKeyPool::takeCredentials()
{
    return impl_->takeCredentials();
}

//--------------------------------------------------------------------------------------------------
void SharedKeyPool::removeKeysForRelay(Session::SessionId session_id)
{
    impl_->removeKeysForRelay(session_id);
}

//--------------------------------------------------------------------------------------------------
void SharedKeyPool::clear()
{
    impl_->clear();
}

//--------------------------------------------------------------------------------------------------
size_t SharedKeyPool::countForRelay(Session::SessionId session_id) const
{
    return impl_->countForRelay(session_id);
}

//--------------------------------------------------------------------------------------------------
size_t SharedKeyPool::count() const
{
    return impl_->count();
}

//--------------------------------------------------------------------------------------------------
bool SharedKeyPool::isEmpty() const
{
    return impl_->isEmpty();
}

} // namespace router
