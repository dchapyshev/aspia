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

#ifndef BASE_IPC_SHARED_MEMORY_H
#define BASE_IPC_SHARED_MEMORY_H

#include <QObject>

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#endif // defined(Q_OS_WINDOWS)

namespace base {

class SharedMemory final : public QObject
{
    Q_OBJECT

public:
    virtual ~SharedMemory() final;

    enum class Mode
    {
        READ_ONLY,
        READ_WRITE
    };
    Q_ENUM(Mode)

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

    static SharedMemory* create(Mode mode, size_t size);
    static SharedMemory* open(Mode mode, int id);

    void* data() { return data_; }
    PlatformHandle handle() const { return handle_.get(); }
    int id() const { return id_; }

signals:
    void sig_destroyed(int id);

private:
    SharedMemory(int id, ScopedPlatformHandle&& handle, void* data, QObject* parent = nullptr);

    ScopedPlatformHandle handle_;
    void* data_;
    int id_;

    Q_DISABLE_COPY(SharedMemory)
};

} // namespace base

#endif // BASE_IPC_SHARED_MEMORY_H
