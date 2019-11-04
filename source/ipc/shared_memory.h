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

#ifndef IPC__SHARED_MEMORY_H
#define IPC__SHARED_MEMORY_H

#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/scoped_object.h"
#endif // defined(OS_WIN)

#include <memory>

namespace ipc {

class SharedMemory
{
public:
    ~SharedMemory();

    enum class Mode
    {
        READ_ONLY,
        READ_WRITE
    };

#if defined(OS_WIN)
    using Handle = HANDLE;
#else
    using Handle = int;
#endif

    static const Handle kInvalidHandle;

    static std::unique_ptr<SharedMemory> create(Mode mode, size_t size);
    static std::unique_ptr<SharedMemory> open(Mode mode, int id);

    void* data() { return data_; }
    Handle handle() const { return handle_.get(); }
    int id() const { return id_; }

private:
    SharedMemory(int id, base::win::ScopedHandle&& handle, void* data);

    base::win::ScopedHandle handle_;
    void* data_;
    int id_;

    DISALLOW_COPY_AND_ASSIGN(SharedMemory);
};

} // namespace ipc

#endif // IPC__SHARED_MEMORY_H
