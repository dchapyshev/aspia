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

#ifndef PROXY__SHARED_POOL_H
#define PROXY__SHARED_POOL_H

#include "proxy/session_key.h"

namespace proxy {

class SharedPool
{
public:
    SharedPool();
    ~SharedPool();

    std::unique_ptr<SharedPool> share();

    uint32_t addKey(SessionKey&& session_key);
    void removeKey(uint32_t key_id);
    const SessionKey& key(uint32_t key_id) const;

private:
    class Pool;
    explicit SharedPool(std::shared_ptr<Pool> pool);

    std::shared_ptr<Pool> pool_;

    DISALLOW_COPY_AND_ASSIGN(SharedPool);
};

} // namespace proxy

#endif // PROXY__SHARED_POOL_H
