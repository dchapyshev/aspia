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

#ifndef RELAY_KEY_POOL_H
#define RELAY_KEY_POOL_H

#include "relay/session_key.h"

#include <optional>
#include <mutex>
#include <unordered_map>

namespace relay {

class KeyFactory;

class KeyPool
{
public:
    ~KeyPool();

    using Key = std::pair<QByteArray, QByteArray>;

    quint32 addKey(SessionKey&& session_key);
    bool removeKey(quint32 key_id);
    void setKeyExpired(quint32 key_id);
    std::optional<Key> key(quint32 key_id, const std::string& peer_public_key) const;
    void clear();

private:
    friend class KeyFactory;

    explicit KeyPool(KeyFactory* factory);
    void dettach();

    mutable std::mutex lock_;
    KeyFactory* factory_;
    std::unordered_map<quint32, SessionKey> map_;
    quint32 current_key_id_ = 0;

    DISALLOW_COPY_AND_ASSIGN(KeyPool);
};

} // namespace relay

#endif // RELAY_KEY_POOL_H
