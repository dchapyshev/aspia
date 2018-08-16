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

#include "client/computer_factory.h"

#include "codec/video_util.h"

namespace aspia {

// static
proto::address_book::Computer ComputerFactory::defaultComputer()
{
    proto::address_book::Computer computer;

    computer.set_session_type(proto::auth::SESSION_TYPE_DESKTOP_MANAGE);
    computer.set_port(kDefaultHostTcpPort);

    setDefaultDesktopManageConfig(computer.mutable_session_config()->mutable_desktop_manage());
    setDefaultDesktopViewConfig(computer.mutable_session_config()->mutable_desktop_view());

    return computer;
}

// static
proto::desktop::Config ComputerFactory::defaultDesktopManageConfig()
{
    proto::desktop::Config config;
    setDefaultDesktopManageConfig(&config);
    return config;
}

// static
proto::desktop::Config ComputerFactory::defaultDesktopViewConfig()
{
    proto::desktop::Config config;
    setDefaultDesktopViewConfig(&config);
    return config;
}

// static
void ComputerFactory::setDefaultDesktopManageConfig(proto::desktop::Config* config)
{
    Q_ASSERT(config);

    static const uint32_t kDefaultFlags =
        proto::desktop::ENABLE_CLIPBOARD | proto::desktop::ENABLE_CURSOR_SHAPE |
        proto::desktop::DISABLE_DESKTOP_EFFECTS | proto::desktop::DISABLE_DESKTOP_WALLPAPER;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);
    config->set_compress_ratio(6);
    config->set_scale_factor(100);
    config->set_update_interval(30);

    VideoUtil::toVideoPixelFormat(PixelFormat::RGB565(), config->mutable_pixel_format());
}

// static
void ComputerFactory::setDefaultDesktopViewConfig(proto::desktop::Config* config)
{
    Q_ASSERT(config);

    static const uint32_t kDefaultFlags =
        proto::desktop::DISABLE_DESKTOP_EFFECTS | proto::desktop::DISABLE_DESKTOP_WALLPAPER;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);
    config->set_compress_ratio(6);
    config->set_scale_factor(100);
    config->set_update_interval(30);

    VideoUtil::toVideoPixelFormat(PixelFormat::RGB565(), config->mutable_pixel_format());
}

} // namespace aspia
