//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef RELAY__SHARED_POOL_H
#define RELAY__SHARED_POOL_H

#include "relay/session_key.h"

#include <optional>

namespace relay {

class SharedPool
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onPoolKeyExpired(uint32_t key_id) = 0;
    };

    using Key = std::pair<base::ByteArray, base::ByteArray>;

    explicit SharedPool(Delegate* delegate);
    ~SharedPool();

    std::unique_ptr<SharedPool> share();

    uint32_t addKey(SessionKey&& session_key);
    bool removeKey(uint32_t key_id);
    void setKeyExpired(uint32_t key_id);
    std::optional<Key> key(uint32_t key_id, std::string_view peer_public_key) const;
    void clear();

private:
    class Pool;
    explicit SharedPool(std::shared_ptr<Pool> pool);

    std::shared_ptr<Pool> pool_;
    bool is_primary_;

    DISALLOW_COPY_AND_ASSIGN(SharedPool);
};

} // namespace relay

#endif // RELAY__SHARED_POOL_H
