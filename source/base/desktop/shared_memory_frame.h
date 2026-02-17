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

#ifndef BASE_DESKTOP_SHARED_MEMORY_FRAME_H
#define BASE_DESKTOP_SHARED_MEMORY_FRAME_H

#include "base/shared_pointer.h"
#include "base/desktop/frame.h"

#include <memory>

namespace base {

class SharedMemoryFactory;

class SharedMemoryFrame final : public Frame
{
public:
    SharedMemoryFrame();
    ~SharedMemoryFrame() final;

    static std::unique_ptr<Frame> create(
        const QSize& size, const PixelFormat& format, SharedMemoryFactory* shared_memory_factory);

    static std::unique_ptr<Frame> open(
        const QSize& size, const PixelFormat& format, int id, SharedMemoryFactory* shared_memory_factory);

    void attach(const QSize& size, const PixelFormat& format, SharedPointer<SharedMemory> shared_memory);
    void dettach();
    bool isAttached() const;

private:
    SharedMemoryFrame(const QSize& size,
                      const PixelFormat& format,
                      SharedPointer<SharedMemory> shared_memory);

    SharedPointer<SharedMemory> owned_shared_memory_;

    Q_DISABLE_COPY(SharedMemoryFrame)
};

} // namespace base

#endif // BASE_DESKTOP_SHARED_MEMORY_FRAME_H
