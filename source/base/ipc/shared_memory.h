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

#ifndef BASE_IPC_SHARED_MEMORY_H
#define BASE_IPC_SHARED_MEMORY_H

#include "base/macros_magic.h"
#include "base/logging.h"
#include "base/memory/local_memory.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/scoped_object.h"
#endif // defined(OS_WIN)

#include <cstddef>
#include <memory>

namespace base {

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
    using PlatformHandle = HANDLE;
    using ScopedPlatformHandle = ScopedHandle;
#else
    using PlatformHandle = int;

    class ScopedPlatformHandle
    {
    public:
        explicit ScopedPlatformHandle(PlatformHandle handle)
            : handle_(handle)
        {
            // Nothing
        }

        ~ScopedPlatformHandle() = default;

        PlatformHandle get() const { return handle_; }

    private:
        PlatformHandle handle_;
    };

#endif

    static const PlatformHandle kInvalidHandle;

    virtual void* data()  = 0;
    virtual PlatformHandle handle() const = 0;
    virtual int id() const = 0;
};

class SharedMemory final : public SharedMemoryBase
{
public:
    virtual ~SharedMemory() final;

    static std::unique_ptr<SharedMemory> create(
        Mode mode, size_t size, base::local_shared_ptr<SharedMemoryFactoryProxy> factory_proxy = nullptr);
    static std::unique_ptr<SharedMemory> open(
        Mode mode, int id, base::local_shared_ptr<SharedMemoryFactoryProxy> factory_proxy = nullptr);

    // SharedMemoryBase implementation.
    void* data() final { return data_; }
    PlatformHandle handle() const final { return handle_.get(); }
    int id() const final { return id_; }

private:
    SharedMemory(int id,
                 ScopedPlatformHandle&& handle,
                 void* data,
                 base::local_shared_ptr<SharedMemoryFactoryProxy> factory_proxy);

    base::local_shared_ptr<SharedMemoryFactoryProxy> factory_proxy_;
    ScopedPlatformHandle handle_;
    void* data_;
    int id_;

    DISALLOW_COPY_AND_ASSIGN(SharedMemory);
};

} // namespace base

#endif // BASE_IPC_SHARED_MEMORY_H
