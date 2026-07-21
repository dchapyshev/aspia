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

#include "router/shared_key_pool.h"

#include <mutex>

#include "base/logging.h"

namespace {

constexpr size_t kMaxKeysPerSession = 1000;
constexpr size_t kMaxKeysTotal = 10000;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
SharedKeyPool& SharedKeyPool::instance()
{
    static SharedKeyPool instance;
    return instance;
}

//--------------------------------------------------------------------------------------------------
void SharedKeyPool::add(qint64 session_id, const std::string& peer_host, quint16 peer_port,
                        const proto::router::RelayKey& key)
{
    std::unique_lock lock(lock_);

    auto relay = relays_.find(session_id);

    if (relay != relays_.end() && relay->second.keys.size() >= kMaxKeysPerSession)
    {
        LOG(WARNING) << "Relay key limit reached for session" << session_id
                     << "key id" << key.key_id() << "will be dropped";
        return;
    }

    size_t key_count = 0;
    for (const auto& item : relays_)
        key_count += item.second.keys.size();

    if (key_count >= kMaxKeysTotal)
    {
        LOG(WARNING) << "Global relay key limit reached. Key id" << key.key_id()
                     << "from session" << session_id << "will be dropped";
        return;
    }

    if (relay == relays_.end())
    {
        LOG(INFO) << "Relay not found in key pool. It will be added";
        relay = relays_.try_emplace(session_id).first;
    }

    Relay& entry = relay->second;
    entry.peer_host = peer_host;
    entry.peer_port = peer_port;

    LOG(INFO) << "Added key with id" << key.key_id() << "for relay" << session_id;
    entry.keys.push_back(key);
}

//--------------------------------------------------------------------------------------------------
std::optional<SharedKeyPool::Credentials> SharedKeyPool::take()
{
    std::unique_lock lock(lock_);

    auto preffered_relay = relays_.end();
    size_t max_count = 0;

    for (auto it = relays_.begin(), it_end = relays_.end(); it != it_end; ++it)
    {
        if (it->second.keys.size() > max_count)
        {
            preffered_relay = it;
            max_count = it->second.keys.size();
        }
    }

    if (preffered_relay == relays_.end())
    {
        LOG(ERROR) << "Empty key pool";
        return std::nullopt;
    }

    LOG(INFO) << "Preffered relay:" << preffered_relay->first;

    Relay& entry = preffered_relay->second;

    Credentials credentials;
    credentials.session_id = preffered_relay->first;
    credentials.peer_host = entry.peer_host;
    credentials.peer_port = entry.peer_port;
    credentials.key = std::move(entry.keys.back());

    // Removing the key from the pool.
    entry.keys.pop_back();

    if (entry.keys.empty())
    {
        LOG(INFO) << "Last key in the pool for relay. The relay will be removed from the pool";
        relays_.erase(preffered_relay);
    }

    return credentials;
}

//--------------------------------------------------------------------------------------------------
size_t SharedKeyPool::count(qint64 session_id) const
{
    std::shared_lock lock(lock_);

    auto it = relays_.find(session_id);
    if (it == relays_.end())
        return 0;

    return it->second.keys.size();
}

//--------------------------------------------------------------------------------------------------
void SharedKeyPool::remove(qint64 session_id)
{
    std::unique_lock lock(lock_);

    if (relays_.erase(session_id) > 0)
        LOG(INFO) << "All keys for relay" << session_id << "removed";
}

//--------------------------------------------------------------------------------------------------
void SharedKeyPool::clear()
{
    std::unique_lock lock(lock_);
    relays_.clear();
}
