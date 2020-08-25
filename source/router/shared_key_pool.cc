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

#include <map>

namespace router {

class SharedKeyPool::Impl
{
public:
    explicit Impl(Delegate* delegate);
    ~Impl() = default;

    void dettach();

    void addKey(const std::u16string& host, uint16_t port, const proto::RelayKey& key);
    std::optional<Credentials> takeCredentials();
    void removeKeysForRelay(const std::u16string& host);
    void clear();
    size_t countForRelay(const std::u16string& host) const;
    size_t count() const;
    bool isEmpty() const;

private:
    struct RelayInfo
    {
        explicit RelayInfo(uint16_t port)
            : port(port)
        {
            // Nothing
        }

        uint16_t port = 0;
        std::vector<proto::RelayKey> keys;
    };

    std::map<std::u16string, RelayInfo> pool_;
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

void SharedKeyPool::Impl::addKey(
    const std::u16string& host, uint16_t port, const proto::RelayKey& key)
{
    auto relay = pool_.find(host);
    if (relay == pool_.end())
        relay = pool_.emplace(host, RelayInfo(port)).first;

    relay->second.keys.emplace_back(std::move(key));
}

std::optional<SharedKeyPool::Credentials> SharedKeyPool::Impl::takeCredentials()
{
    auto preffered_relay = pool_.end();
    size_t max_count = 0;

    for (auto it = pool_.begin(); it != pool_.end(); ++it)
    {
        size_t count = it->second.keys.size();
        if (count > max_count)
        {
            preffered_relay = it;
            max_count = count;
        }

        ++it;
    }

    if (preffered_relay == pool_.end())
        return std::nullopt;

    Credentials credentials;
    credentials.host = preffered_relay->first;
    credentials.port = preffered_relay->second.port;
    credentials.key = std::move(preffered_relay->second.keys.back());

    // Removing the key from the pool.
    preffered_relay->second.keys.pop_back();

    if (preffered_relay->second.keys.empty())
    {
        pool_.erase(preffered_relay->first);

        // Notify that the pool for the relay is empty.
        if (delegate_)
            delegate_->onKeyPoolEmpty(preffered_relay->first);
    }

    return credentials;
}

void SharedKeyPool::Impl::removeKeysForRelay(const std::u16string& host)
{
    pool_.erase(host);
}

void SharedKeyPool::Impl::clear()
{
    pool_.clear();
}

size_t SharedKeyPool::Impl::countForRelay(const std::u16string& host) const
{
    auto result = pool_.find(host);
    if (result == pool_.end())
        return 0;

    return result->second.keys.size();
}

size_t SharedKeyPool::Impl::count() const
{
    size_t result = 0;

    for (const auto& relay : pool_)
        result += relay.second.keys.size();

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

void SharedKeyPool::addKey(const std::u16string& host, uint16_t port, const proto::RelayKey& key)
{
    impl_->addKey(host, port, key);
}

std::optional<SharedKeyPool::Credentials> SharedKeyPool::takeCredentials()
{
    return impl_->takeCredentials();
}

void SharedKeyPool::removeKeysForRelay(const std::u16string& host)
{
    impl_->removeKeysForRelay(host);
}

void SharedKeyPool::clear()
{
    impl_->clear();
}

size_t SharedKeyPool::countForRelay(const std::u16string& host) const
{
    return impl_->countForRelay(host);
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
