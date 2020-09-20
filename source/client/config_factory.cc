//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/config_factory.h"

#include "base/logging.h"

namespace client {

// static
proto::DesktopConfig ConfigFactory::defaultDesktopManageConfig()
{
    proto::DesktopConfig config;
    setDefaultDesktopManageConfig(&config);
    return config;
}

// static
proto::DesktopConfig ConfigFactory::defaultDesktopViewConfig()
{
    proto::DesktopConfig config;
    setDefaultDesktopViewConfig(&config);
    return config;
}

// static
void ConfigFactory::setDefaultDesktopManageConfig(proto::DesktopConfig* config)
{
    DCHECK(config);

    static const uint32_t kDefaultFlags =
        proto::ENABLE_CLIPBOARD | proto::ENABLE_CURSOR_SHAPE | proto::DISABLE_DESKTOP_EFFECTS |
        proto::DISABLE_DESKTOP_WALLPAPER | proto::DISABLE_FONT_SMOOTHING;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(proto::VideoEncoding::VIDEO_ENCODING_VP8);

    fixupDesktopConfig(config);
}

// static
void ConfigFactory::setDefaultDesktopViewConfig(proto::DesktopConfig* config)
{
    DCHECK(config);

    static const uint32_t kDefaultFlags =
        proto::DISABLE_DESKTOP_EFFECTS | proto::DISABLE_DESKTOP_WALLPAPER |
        proto::DISABLE_FONT_SMOOTHING;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(proto::VideoEncoding::VIDEO_ENCODING_VP8);

    fixupDesktopConfig(config);
}

// static
void ConfigFactory::fixupDesktopConfig(proto::DesktopConfig* config)
{
    config->set_scale_factor(100);
    config->set_update_interval(30);
}

} // namespace client
