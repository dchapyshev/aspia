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

#ifndef ASPIA_DESKTOP_CAPTURE__SCREEN_SETTINGS_TRACKER_H_
#define ASPIA_DESKTOP_CAPTURE__SCREEN_SETTINGS_TRACKER_H_

#include "base/macros_magic.h"
#include "desktop_capture/desktop_geometry.h"
#include "desktop_capture/pixel_format.h"

namespace aspia {

class ScreenSettingsTracker
{
public:
    ScreenSettingsTracker() = default;

    bool isRectChanged(const DesktopRect& screen_rect);
    bool isSizeChanged(const DesktopSize& screen_size);
    bool isFormatChanged(const PixelFormat& pixel_format);

    const DesktopRect& screenRect() const { return screen_rect_; }
    const DesktopSize& screenSize() const { return screen_rect_.size(); }
    const PixelFormat& format() const { return pixel_format_; }

private:
    DesktopRect screen_rect_;
    PixelFormat pixel_format_;

    DISALLOW_COPY_AND_ASSIGN(ScreenSettingsTracker);
};

} // namespace aspia

#endif // ASPIA_DESKTOP_CAPTURE__SCREEN_SETTINGS_TRACKER_H_
