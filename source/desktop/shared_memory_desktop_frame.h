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

#ifndef DESKTOP__SHARED_MEMORY_DESKTOP_FRAME_H
#define DESKTOP__SHARED_MEMORY_DESKTOP_FRAME_H

#include "desktop/desktop_frame.h"

namespace ipc {
class SharedMemoryFactory;
} // namespace ipc

namespace desktop {

class SharedMemoryFrame : public Frame
{
public:
    ~SharedMemoryFrame();

    static std::unique_ptr<Frame> create(
        const Size& size, const PixelFormat& format, ipc::SharedMemoryFactory* shared_memory_factory);

    static std::unique_ptr<Frame> open(
        const Size& size, const PixelFormat& format, int id, ipc::SharedMemoryFactory* shared_memory_factory);

    static std::unique_ptr<Frame> attach(
        const Size& size, const PixelFormat& format, std::unique_ptr<ipc::SharedMemory> shared_memory);

private:
    SharedMemoryFrame(const Size& size,
                      const PixelFormat& format,
                      ipc::SharedMemory* shared_memory);

    DISALLOW_COPY_AND_ASSIGN(SharedMemoryFrame);
};

} // namespace desktop

#endif // DESKTOP__SHARED_MEMORY_DESKTOP_FRAME_H
