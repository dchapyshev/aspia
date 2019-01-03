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

#include "desktop_capture/screen_settings_tracker.h"

namespace aspia {

bool ScreenSettingsTracker::isRectChanged(const QRect& screen_rect)
{
    if (screen_rect != screen_rect_)
    {
        screen_rect_ = screen_rect;
        return true;
    }

    return false;
}

bool ScreenSettingsTracker::isSizeChanged(const QSize& screen_size)
{
    if (screen_size != screen_rect_.size())
    {
        screen_rect_.setSize(screen_size);
        return true;
    }

    return false;
}

bool ScreenSettingsTracker::isFormatChanged(const PixelFormat& pixel_format)
{
    if (pixel_format != pixel_format_)
    {
        pixel_format_ = pixel_format;
        return true;
    }

    return false;
}

} // namespace aspia
