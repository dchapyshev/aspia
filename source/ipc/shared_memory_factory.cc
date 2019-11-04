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

#include "ipc/shared_memory_factory.h"

#include "ipc/shared_memory.h"

namespace ipc {

std::unique_ptr<SharedMemory> SharedMemoryFactory::create(size_t size)
{
    return SharedMemory::create(SharedMemory::Mode::READ_WRITE, size);
}

std::unique_ptr<SharedMemory> SharedMemoryFactory::open(int id)
{
    return SharedMemory::open(SharedMemory::Mode::READ_ONLY, id);
}

} // namespace ipc
