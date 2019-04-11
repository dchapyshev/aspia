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

#include "host/desktop_config_tracker.h"

namespace host {

namespace {

bool isEqualPixelFormat(const proto::desktop::PixelFormat& first,
                        const proto::desktop::PixelFormat& second)
{
    return first.red_max()        == second.red_max()     &&
           first.green_max()      == second.green_max()   &&
           first.blue_max()       == second.blue_max()    &&
           first.red_shift()      == second.red_shift()   &&
           first.green_shift()    == second.green_shift() &&
           first.blue_shift()     == second.blue_shift()  &&
           first.bits_per_pixel() == second.bits_per_pixel();
}

} // namespace

uint32_t DesktopConfigTracker::changesMask(const proto::desktop::Config& new_config)
{
    if (!old_config_.has_value())
    {
        old_config_ = new_config;
        return std::numeric_limits<uint32_t>::max();
    }

    uint32_t result = 0;

    if (old_config_->video_encoding() != new_config.video_encoding())
        result |= HAS_VIDEO;

    if (!isEqualPixelFormat(old_config_->pixel_format(), new_config.pixel_format()))
        result |= HAS_VIDEO;

    if (old_config_->update_interval() != new_config.update_interval())
        result |= HAS_VIDEO;

    if (old_config_->compress_ratio() != new_config.compress_ratio())
        result |= HAS_VIDEO;

    if ((old_config_->flags() & proto::desktop::ENABLE_CURSOR_SHAPE) !=
        (new_config.flags() & proto::desktop::ENABLE_CURSOR_SHAPE))
    {
        result |= HAS_VIDEO;
    }

    if ((old_config_->flags() & proto::desktop::ENABLE_CLIPBOARD) !=
        (new_config.flags() & proto::desktop::ENABLE_CLIPBOARD))
    {
        result |= HAS_CLIPBOARD;
    }

    if ((old_config_->flags() & proto::desktop::DISABLE_DESKTOP_EFFECTS) !=
        (new_config.flags() & proto::desktop::DISABLE_DESKTOP_EFFECTS))
    {
        result |= HAS_VIDEO;
    }

    if ((old_config_->flags() & proto::desktop::DISABLE_DESKTOP_WALLPAPER) !=
        (new_config.flags() & proto::desktop::DISABLE_DESKTOP_WALLPAPER))
    {
        result |= HAS_VIDEO;
    }

    if ((old_config_->flags() & proto::desktop::DISABLE_FONT_SMOOTHING) !=
        (new_config.flags() & proto::desktop::DISABLE_FONT_SMOOTHING))
    {
        result |= HAS_VIDEO;
    }

    if ((old_config_->flags() & proto::desktop::BLOCK_REMOTE_INPUT) !=
        (new_config.flags() & proto::desktop::BLOCK_REMOTE_INPUT))
    {
        result |= HAS_INPUT;
    }

    old_config_ = new_config;
    return result;
}

} // namespace host
