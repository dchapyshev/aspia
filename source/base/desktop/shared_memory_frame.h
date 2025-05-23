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

#ifndef BASE_DESKTOP_SHARED_MEMORY_FRAME_H
#define BASE_DESKTOP_SHARED_MEMORY_FRAME_H

#include "base/desktop/frame.h"
#include "base/memory/local_memory.h"

#include <memory>

namespace base {

class SharedMemoryFactory;

class SharedMemoryFrame final : public Frame
{
public:
    ~SharedMemoryFrame() final;

    static std::unique_ptr<Frame> create(
        const Size& size, const PixelFormat& format, SharedMemoryFactory* shared_memory_factory);

    static std::unique_ptr<Frame> open(
        const Size& size, const PixelFormat& format, int id, SharedMemoryFactory* shared_memory_factory);

    static std::unique_ptr<Frame> attach(
        const Size& size, const PixelFormat& format, local_shared_ptr<SharedMemoryBase> shared_memory);

private:
    SharedMemoryFrame(const Size& size,
                      const PixelFormat& format,
                      local_shared_ptr<SharedMemoryBase> shared_memory);

    local_shared_ptr<SharedMemoryBase> owned_shared_memory_;

    DISALLOW_COPY_AND_ASSIGN(SharedMemoryFrame);
};

} // namespace base

#endif // BASE_DESKTOP_SHARED_MEMORY_FRAME_H
