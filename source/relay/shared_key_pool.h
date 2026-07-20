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

#ifndef RELAY_SHARED_KEY_POOL_H
#define RELAY_SHARED_KEY_POOL_H

#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/crypto/secure_byte_array.h"
#include "relay/session_key.h"

// Thread-safe pool of one-time session keys announced to the router.
class SharedKeyPool final
{
public:
    static SharedKeyPool& instance();

    using Key = std::pair<SecureByteArray, QByteArray>;

    // Adds |session_key| to the pool and returns the identifier assigned to it.
    quint32 add(SessionKey&& session_key);

    // Removes the key |key_id| from the pool. Returns false if there is no such key.
    bool remove(quint32 key_id);

    // Derives the session key and iv for the key |key_id| using |peer_public_key|.
    std::optional<Key> find(quint32 key_id, const std::string& peer_public_key) const;

    size_t count() const;
    void clear();

private:
    SharedKeyPool() = default;
    ~SharedKeyPool() = default;

    mutable std::shared_mutex lock_;
    std::unordered_map<quint32, SessionKey> keys_;
    quint32 key_counter_ = 0;

    Q_DISABLE_COPY_MOVE(SharedKeyPool)
};

#endif // RELAY_SHARED_KEY_POOL_H
