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

#ifndef ROUTER_SHARED_KEY_POOL_H
#define ROUTER_SHARED_KEY_POOL_H

#include <QtGlobal>

#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "proto/router.h"

// Thread-safe pool of one-time relay keys.
class SharedKeyPool final
{
public:
    static SharedKeyPool& instance();

    struct Credentials
    {
        qint64 session_id;
        proto::router::RelayKey key;
        std::string peer_host;
        quint16 peer_port;
    };

    // Adds a key announced by relay |session_id| reachable at |peer_host|:|peer_port|. The key is
    // dropped when the per-relay or the global pool limit is reached.
    void add(qint64 session_id, const std::string& peer_host, quint16 peer_port,
             const proto::router::RelayKey& key);

    // Takes a one-time key from the relay with the largest pool. The caller must report the
    // consumed key to the relay session.
    std::optional<Credentials> take();

    size_t count(qint64 session_id) const;
    void remove(qint64 session_id);
    void clear();

private:
    SharedKeyPool() = default;
    ~SharedKeyPool() = default;

    struct Relay
    {
        std::string peer_host;
        quint16 peer_port = 0;
        std::vector<proto::router::RelayKey> keys;
    };

    mutable std::shared_mutex lock_;
    std::unordered_map<qint64, Relay> relays_;

    Q_DISABLE_COPY_MOVE(SharedKeyPool)
};

#endif // ROUTER_SHARED_KEY_POOL_H
