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

#include "console/computer_factory.h"

#include "build/build_config.h"
#include "base/logging.h"
#include "codec/video_util.h"

namespace console {

namespace {

const int kDefUpdateInterval = 30;
const int kDefCompressRatio = 8;

void setDefaultDesktopManageConfig(proto::DesktopConfig* config)
{
    DCHECK(config);

    static const uint32_t kDefaultFlags =
        proto::ENABLE_CLIPBOARD | proto::ENABLE_CURSOR_SHAPE | proto::DISABLE_DESKTOP_EFFECTS |
        proto::DISABLE_DESKTOP_WALLPAPER | proto::DISABLE_FONT_SMOOTHING;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(proto::VideoEncoding::VIDEO_ENCODING_VP8);
    config->set_compress_ratio(kDefCompressRatio);
    config->set_update_interval(kDefUpdateInterval);
    config->set_scale_factor(100);

    codec::serializePixelFormat(desktop::PixelFormat::RGB332(), config->mutable_pixel_format());
}

void setDefaultDesktopViewConfig(proto::DesktopConfig* config)
{
    DCHECK(config);

    static const uint32_t kDefaultFlags =
        proto::DISABLE_DESKTOP_EFFECTS | proto::DISABLE_DESKTOP_WALLPAPER |
        proto::DISABLE_FONT_SMOOTHING;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(proto::VideoEncoding::VIDEO_ENCODING_VP8);
    config->set_compress_ratio(kDefCompressRatio);
    config->set_update_interval(kDefUpdateInterval);
    config->set_scale_factor(100);

    codec::serializePixelFormat(desktop::PixelFormat::RGB332(), config->mutable_pixel_format());
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

} // namespace console
