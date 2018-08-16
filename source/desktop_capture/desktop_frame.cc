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

#include "desktop_capture/desktop_frame.h"

namespace aspia {

DesktopFrame::DesktopFrame(const DesktopSize& size,
                           const PixelFormat& format,
                           int stride,
                           uint8_t* data)
    : size_(size),
      format_(format),
      stride_(stride),
      data_(data)
{
    // Nothing
}

bool DesktopFrame::contains(int x, int y) const
{
    return (x >= 0 && x <= size_.width() && y >= 0 && y <= size_.height());
}

uint8_t* DesktopFrame::frameDataAtPos(const DesktopPoint& pos) const
{
    return frameData() + stride() * pos.y() + format_.bytesPerPixel() * pos.x();
}

uint8_t* DesktopFrame::frameDataAtPos(int x, int y) const
{
    return frameDataAtPos(DesktopPoint(x, y));
}

} // namespace aspia
