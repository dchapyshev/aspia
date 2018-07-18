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

#include "desktop_capture/desktop_frame_aligned.h"

namespace aspia {

DesktopFrameAligned::DesktopFrameAligned(const QSize& size,
                                         const PixelFormat& format,
                                         int stride,
                                         quint8* data)
    : DesktopFrame(size, format, stride, data)
{
    // Nothing
}

DesktopFrameAligned::~DesktopFrameAligned()
{
    qFreeAligned(data_);
}

// static
std::unique_ptr<DesktopFrameAligned> DesktopFrameAligned::create(
    const QSize& size, const PixelFormat& format)
{
    int bytes_per_row = size.width() * format.bytesPerPixel();

    quint8* data = reinterpret_cast<quint8*>(qMallocAligned(bytes_per_row * size.height(), 16));
    if (!data)
        return nullptr;

    return std::unique_ptr<DesktopFrameAligned>(
        new DesktopFrameAligned(size, format, bytes_per_row, data));
}

} // namespace aspia
