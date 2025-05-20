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

#include <QObject>

#include "base/macros_magic.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#endif // defined(Q_OS_WINDOWS)

#include <cstddef>
#include <memory>

namespace base {

class SharedMemoryBase : public QObject
{
    Q_OBJECT

public:
    explicit SharedMemoryBase(QObject* parent)
        : QObject(parent)
    {
        // Nothing
    }
    virtual ~SharedMemoryBase() = default;

    enum class Mode
    {
        READ_ONLY,
        READ_WRITE
    };

#if defined(Q_OS_WINDOWS)
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
    Q_OBJECT

public:
    virtual ~SharedMemory() final;

    static std::unique_ptr<SharedMemory> create(Mode mode, size_t size);
    static std::unique_ptr<SharedMemory> open(Mode mode, int id);

    // SharedMemoryBase implementation.
    void* data() final { return data_; }
    PlatformHandle handle() const final { return handle_.get(); }
    int id() const final { return id_; }

signals:
    void sig_destroyed(int id);

private:
    SharedMemory(int id, ScopedPlatformHandle&& handle, void* data, QObject* parent = nullptr);

    ScopedPlatformHandle handle_;
    void* data_;
    int id_;

    DISALLOW_COPY_AND_ASSIGN(SharedMemory);
};

} // namespace base

#endif // BASE_IPC_SHARED_MEMORY_H
