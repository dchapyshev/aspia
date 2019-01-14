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

#include "console/computer_factory.h"

#include "build/build_config.h"
#include "base/logging.h"
#include "codec/video_util.h"

namespace aspia {

namespace {

const int kDefUpdateInterval = 30;
const int kDefScaleFactor = 100;
const int kDefCompressRatio = 8;

void setDefaultDesktopManageConfig(proto::desktop::Config* config)
{
    DCHECK(config);

    static const uint32_t kDefaultFlags =
        proto::desktop::ENABLE_CLIPBOARD | proto::desktop::ENABLE_CURSOR_SHAPE |
        proto::desktop::DISABLE_DESKTOP_EFFECTS | proto::desktop::DISABLE_DESKTOP_WALLPAPER;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZSTD);
    config->set_compress_ratio(kDefCompressRatio);
    config->set_scale_factor(kDefScaleFactor);
    config->set_update_interval(kDefUpdateInterval);

    codec::VideoUtil::toVideoPixelFormat(
        desktop::PixelFormat::RGB565(), config->mutable_pixel_format());
}

void setDefaultDesktopViewConfig(proto::desktop::Config* config)
{
    DCHECK(config);

    static const uint32_t kDefaultFlags =
        proto::desktop::DISABLE_DESKTOP_EFFECTS | proto::desktop::DISABLE_DESKTOP_WALLPAPER;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZSTD);
    config->set_compress_ratio(kDefCompressRatio);
    config->set_scale_factor(kDefScaleFactor);
    config->set_update_interval(kDefUpdateInterval);

    codec::VideoUtil::toVideoPixelFormat(
        desktop::PixelFormat::RGB565(), config->mutable_pixel_format());
}

} // namespace

// static
proto::address_book::Computer ComputerFactory::defaultComputer()
{
    proto::address_book::Computer computer;

    computer.set_session_type(proto::SESSION_TYPE_DESKTOP_MANAGE);
    computer.set_port(DEFAULT_HOST_TCP_PORT);

    setDefaultDesktopManageConfig(computer.mutable_session_config()->mutable_desktop_manage());
    setDefaultDesktopViewConfig(computer.mutable_session_config()->mutable_desktop_view());

    return computer;
}

} // namespace aspia
