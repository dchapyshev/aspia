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

#ifndef ROUTER__SHARED_KEY_POOL_H
#define ROUTER__SHARED_KEY_POOL_H

#include "base/macros_magic.h"

#include <cstdint>
#include <memory>

namespace proto {
class RelayKey;
} // namespace proto

namespace router {

class SharedKeyPool
{
public:
    using RelayId = uint32_t;
    using Key = std::unique_ptr<proto::RelayKey>;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onKeyPoolEmpty(RelayId relay_id) = 0;
    };

    explicit SharedKeyPool(Delegate* delegate);
    ~SharedKeyPool();

    std::unique_ptr<SharedKeyPool> share();

    void addKey(RelayId relay_id, Key&& key);
    Key takeKey();
    void removeKeysForRelay(RelayId relay_id);
    void clear();
    size_t countForRelay(RelayId relay_id) const;
    size_t count() const;
    bool isEmpty() const;

private:
    class Impl;
    explicit SharedKeyPool(std::shared_ptr<Impl> impl);

    std::shared_ptr<Impl> impl_;
    const bool is_primary_;

    DISALLOW_COPY_AND_ASSIGN(SharedKeyPool);
};

} // namespace router

#endif // ROUTER__SHARED_KEY_POOL_H
