//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "desktop/desktop_frame_aligned.h"

#include "base/aligned_memory.h"

namespace desktop {

DesktopFrameAligned::DesktopFrameAligned(const QSize& size,
                                         const PixelFormat& format,
                                         int stride,
                                         uint8_t* data)
    : DesktopFrame(size, format, stride, data)
{
    // Nothing
}

DesktopFrameAligned::~DesktopFrameAligned()
{
    base::alignedFree(data_);
}

// static
std::unique_ptr<DesktopFrameAligned> DesktopFrameAligned::create(
    const QSize& size, const PixelFormat& format, size_t alignment)
{
    int bytes_per_row = size.width() * format.bytesPerPixel();

    uint8_t* data =
        reinterpret_cast<uint8_t*>(base::alignedAlloc(bytes_per_row * size.height(), alignment));
    if (!data)
        return nullptr;

    return std::unique_ptr<DesktopFrameAligned>(
        new DesktopFrameAligned(size, format, bytes_per_row, data));
}

} // namespace desktop
