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
#include "codec/video_util.h"

namespace client {

namespace {

const int kDefUpdateInterval = 30;
const int kMinUpdateInterval = 15;
const int kMaxUpdateInterval = 100;

const int kDefCompressRatio = 8;
const int kMinCompressRatio = 1;
const int kMaxCompressRatio = 22;

} // namespace

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
        proto::ENABLE_CLIPBOARD | proto::ENABLE_CURSOR_SHAPE |
        proto::DISABLE_DESKTOP_EFFECTS | proto::DISABLE_DESKTOP_WALLPAPER;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(proto::VideoEncoding::VIDEO_ENCODING_ZSTD);
    config->set_compress_ratio(kDefCompressRatio);
    config->set_update_interval(kDefUpdateInterval);

    codec::serializePixelFormat(desktop::PixelFormat::RGB565(), config->mutable_pixel_format());
}

// static
void ConfigFactory::setDefaultDesktopViewConfig(proto::DesktopConfig* config)
{
    DCHECK(config);

    static const uint32_t kDefaultFlags =
        proto::DISABLE_DESKTOP_EFFECTS | proto::DISABLE_DESKTOP_WALLPAPER;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(proto::VideoEncoding::VIDEO_ENCODING_ZSTD);
    config->set_compress_ratio(kDefCompressRatio);
    config->set_update_interval(kDefUpdateInterval);

    codec::serializePixelFormat(desktop::PixelFormat::RGB565(), config->mutable_pixel_format());
}

// static
void ConfigFactory::fixupDesktopConfig(proto::DesktopConfig* config)
{
    config->set_scale_factor(100);

    if (config->update_interval() < kMinUpdateInterval || config->update_interval() > kMaxUpdateInterval)
        config->set_update_interval(kDefUpdateInterval);

    if (config->compress_ratio() < kMinCompressRatio || config->compress_ratio() > kMaxCompressRatio)
        config->set_compress_ratio(kDefCompressRatio);
}

} // namespace client
