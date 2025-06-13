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

#include "base/desktop/shared_memory_frame.h"

#include "base/logging.h"
#include "base/ipc/shared_memory.h"
#include "base/ipc/shared_memory_factory.h"

namespace base {

//--------------------------------------------------------------------------------------------------
SharedMemoryFrame::SharedMemoryFrame()
    : Frame(Size(), PixelFormat(), 0, nullptr, nullptr)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SharedMemoryFrame::SharedMemoryFrame(const Size& size,
                                     const PixelFormat& format,
                                     SharedPointer<SharedMemory> shared_memory)
    : Frame(size, format, size.width() * format.bytesPerPixel(),
            reinterpret_cast<quint8*>(shared_memory->data()), shared_memory.get()),
      owned_shared_memory_(std::move(shared_memory))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SharedMemoryFrame::~SharedMemoryFrame() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<Frame> SharedMemoryFrame::create(
    const Size& size, const PixelFormat& format, SharedMemoryFactory* shared_memory_factory)
{
    const size_t buffer_size = calcMemorySize(size, format.bytesPerPixel());

    SharedMemory* shared_memory = shared_memory_factory->create(buffer_size);
    if (!shared_memory)
    {
        LOG(LS_ERROR) << "SharedMemoryFactory::create failed for size:" << buffer_size;
        return nullptr;
    }

    return std::unique_ptr<Frame>(new SharedMemoryFrame(
        size, format, SharedPointer<SharedMemory>(shared_memory)));
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<Frame> SharedMemoryFrame::open(
    const Size& size, const PixelFormat& format, int id, SharedMemoryFactory* shared_memory_factory)
{
    SharedMemory* shared_memory = shared_memory_factory->open(id);
    if (!shared_memory)
    {
        LOG(LS_ERROR) << "SharedMemoryFactory::open failed for id:" << id;
        return nullptr;
    }

    return std::unique_ptr<Frame>(new SharedMemoryFrame(
        size, format, SharedPointer<SharedMemory>(shared_memory)));
}

//--------------------------------------------------------------------------------------------------
void SharedMemoryFrame::attach(
    const Size& size, const PixelFormat& format, SharedPointer<SharedMemory> shared_memory)
{
    owned_shared_memory_ = std::move(shared_memory);

    data_ = reinterpret_cast<quint8*>(owned_shared_memory_->data());
    shared_memory_ = owned_shared_memory_.get();

    size_ = size;
    format_ = format;
    stride_ = size.width() * format.bytesPerPixel();
}

//--------------------------------------------------------------------------------------------------
void SharedMemoryFrame::dettach()
{
    owned_shared_memory_.reset();

    data_ = nullptr;
    shared_memory_ = nullptr;

    size_ = Size();
    format_ = PixelFormat();
    stride_ = 0;
}

//--------------------------------------------------------------------------------------------------
bool SharedMemoryFrame::isAttached() const
{
    return owned_shared_memory_;
}

} // namespace base
