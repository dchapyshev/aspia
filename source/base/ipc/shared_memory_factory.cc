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

#include "base/ipc/shared_memory_factory.h"

#include "base/ipc/shared_memory.h"

namespace base {

//--------------------------------------------------------------------------------------------------
SharedMemoryFactory::SharedMemoryFactory(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SharedMemoryFactory::~SharedMemoryFactory() = default;

//--------------------------------------------------------------------------------------------------
SharedMemory* SharedMemoryFactory::create(size_t size)
{
    SharedMemory* memory = SharedMemory::create(SharedMemory::Mode::READ_WRITE, size);
    if (!memory)
        return nullptr;

    connect(memory, &SharedMemory::sig_destroyed, this, &SharedMemoryFactory::sig_memoryDestroyed);

    emit sig_memoryCreated(memory->id());
    return memory;
}

//--------------------------------------------------------------------------------------------------
SharedMemory* SharedMemoryFactory::open(int id)
{
    SharedMemory* memory = SharedMemory::open(SharedMemory::Mode::READ_ONLY, id);
    if (!memory)
        return nullptr;

    connect(memory, &SharedMemory::sig_destroyed, this, &SharedMemoryFactory::sig_memoryDestroyed);

    emit sig_memoryCreated(memory->id());
    return memory;
}

} // namespace base
