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

#include <cstddef>
#include <memory>

namespace ipc {

class SharedMemoryFactoryProxy;

class SharedMemoryBase
{
public:
    virtual ~SharedMemoryBase() = default;

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

    virtual void* data()  = 0;
    virtual Handle handle() const = 0;
    virtual int id() const = 0;
};

class SharedMemory : public SharedMemoryBase
{
public:
    virtual ~SharedMemory();

    static std::unique_ptr<SharedMemory> create(
        Mode mode, size_t size, std::shared_ptr<SharedMemoryFactoryProxy> factory_proxy = nullptr);
    static std::unique_ptr<SharedMemory> open(
        Mode mode, int id, std::shared_ptr<SharedMemoryFactoryProxy> factory_proxy = nullptr);

    // SharedMemoryBase implementation.
    void* data() override { return data_; }
    Handle handle() const override { return handle_.get(); }
    int id() const override { return id_; }

private:
    SharedMemory(int id,
                 base::win::ScopedHandle&& handle,
                 void* data,
                 std::shared_ptr<SharedMemoryFactoryProxy> factory_proxy);

    std::shared_ptr<SharedMemoryFactoryProxy> factory_proxy_;
    base::win::ScopedHandle handle_;
    void* data_;
    int id_;

    DISALLOW_COPY_AND_ASSIGN(SharedMemory);
};

} // namespace ipc

#endif // IPC__SHARED_MEMORY_H
