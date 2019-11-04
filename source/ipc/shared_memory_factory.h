//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef IPC__SHARED_MEMORY_FACTORY_H
#define IPC__SHARED_MEMORY_FACTORY_H

#include "base/macros_magic.h"

#include <memory>

namespace ipc {

class SharedMemory;

class SharedMemoryFactory
{
public:
    SharedMemoryFactory() = default;
    ~SharedMemoryFactory() = default;

    std::unique_ptr<SharedMemory> create(size_t size);
    std::unique_ptr<SharedMemory> open(int id);

private:
    DISALLOW_COPY_AND_ASSIGN(SharedMemoryFactory);
};

} // namespace ipc

#endif // IPC__SHARED_MEMORY_FACTORY_H
