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

#ifndef ASPIA_DESKTOP__DESKTOP_FRAME_DIB_H
#define ASPIA_DESKTOP__DESKTOP_FRAME_DIB_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <memory>

#include "base/win/scoped_gdi_object.h"
#include "desktop/desktop_frame.h"

namespace desktop {

class DesktopFrameDIB : public DesktopFrame
{
public:
    ~DesktopFrameDIB() = default;

    static std::unique_ptr<DesktopFrameDIB> create(const QSize& size,
                                                   const PixelFormat& format,
                                                   HDC hdc);

    HBITMAP bitmap() { return bitmap_; }

private:
    DesktopFrameDIB(const QSize& size,
                    const PixelFormat& format,
                    int stride,
                    uint8_t* data,
                    HBITMAP bitmap);

    base::win::ScopedHBITMAP bitmap_;

    DISALLOW_COPY_AND_ASSIGN(DesktopFrameDIB);
};

} // namespace desktop

#endif // ASPIA_DESKTOP__DESKTOP_FRAME_DIB_H
