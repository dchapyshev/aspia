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

#include "base/desktop/frame_aligned.h"

#include <cstring>
#include <limits>

#include "base/aligned_memory.h"
#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
FrameAligned::FrameAligned(const QSize& size, int stride, quint8* data)
    : Frame(size, stride, data)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
FrameAligned::~FrameAligned()
{
    alignedFree(data_);
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<FrameAligned> FrameAligned::create(const QSize& size, size_t alignment)
{
    if (size.width() <= 0 || size.height() <= 0)
    {
        LOG(ERROR) << "Invalid frame size:" << size;
        return nullptr;
    }

    // Rounding the stride up below is a power-of-two mask. Any other value gives a stride that does
    // not even fit the row, and every write to the frame goes past the buffer.
    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    {
        LOG(ERROR) << "Alignment is not a power of two:" << alignment;
        return nullptr;
    }

    // Round up stride to the alignment boundary so that every row starts at an aligned address. The
    // arithmetic is done in size_t: a width of half a billion overflows the int of the row size.
    const size_t row_size = static_cast<size_t>(size.width()) * kBytesPerPixel;
    const size_t stride = (row_size + alignment - 1) & ~(alignment - 1);

    if (stride > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        LOG(ERROR) << "Frame is too wide:" << size.width();
        return nullptr;
    }

    const size_t buffer_size = stride * static_cast<size_t>(size.height());

    quint8* data = reinterpret_cast<quint8*>(alignedAlloc(buffer_size, alignment));
    if (!data)
        return nullptr;

    // alignedAlloc hands over the buffer as the heap left it. Not every owner fills the whole frame:
    // a decoder converts only the rectangles the peer sent, and the rest would be painted as
    // garbage. Frames are created rarely, only when the size changes.
    memset(data, 0, buffer_size);

    return std::unique_ptr<FrameAligned>(new FrameAligned(size, static_cast<int>(stride), data));
}
