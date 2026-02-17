//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/serialization.h"
#include "base/desktop/pixel_format.h"

namespace client {

namespace {

const proto::desktop::VideoEncoding kDefaultVideoEncoding = proto::desktop::VIDEO_ENCODING_VP8;
const proto::desktop::AudioEncoding kDefaultAudioEncoding = proto::desktop::AUDIO_ENCODING_OPUS;

const int kDefCompressRatio = 8;
const int kMinCompressRatio = 1;
const int kMaxCompressRatio = 22;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
proto::desktop::Config ConfigFactory::defaultDesktopManageConfig()
{
    proto::desktop::Config config;
    setDefaultDesktopManageConfig(&config);
    return config;
}

//--------------------------------------------------------------------------------------------------
// static
proto::desktop::Config ConfigFactory::defaultDesktopViewConfig()
{
    proto::desktop::Config config;
    setDefaultDesktopViewConfig(&config);
    return config;
}

//--------------------------------------------------------------------------------------------------
// static
void ConfigFactory::setDefaultDesktopManageConfig(proto::desktop::Config* config)
{
    DCHECK(config);

    static const quint32 kDefaultFlags =
        proto::desktop::ENABLE_CLIPBOARD | proto::desktop::ENABLE_CURSOR_SHAPE |
        proto::desktop::DISABLE_EFFECTS | proto::desktop::DISABLE_WALLPAPER |
        proto::desktop::CLEAR_CLIPBOARD;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(kDefaultVideoEncoding);
    config->set_audio_encoding(kDefaultAudioEncoding);
    config->set_compress_ratio(kDefCompressRatio);
    config->mutable_pixel_format()->CopyFrom(base::serialize(base::PixelFormat::RGB332()));

    fixupDesktopConfig(config);
}

//--------------------------------------------------------------------------------------------------
// static
void ConfigFactory::setDefaultDesktopViewConfig(proto::desktop::Config* config)
{
    DCHECK(config);

    static const quint32 kDefaultFlags =
        proto::desktop::DISABLE_EFFECTS | proto::desktop::DISABLE_WALLPAPER;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(kDefaultVideoEncoding);
    config->set_audio_encoding(kDefaultAudioEncoding);
    config->set_compress_ratio(kDefCompressRatio);
    config->mutable_pixel_format()->CopyFrom(base::serialize(base::PixelFormat::RGB332()));

    fixupDesktopConfig(config);
}

//--------------------------------------------------------------------------------------------------
// static
void ConfigFactory::fixupDesktopConfig(proto::desktop::Config* config)
{
    DCHECK(config);

    if (config->compress_ratio() < kMinCompressRatio || config->compress_ratio() > kMaxCompressRatio)
        config->set_compress_ratio(kDefCompressRatio);

    if (config->audio_encoding() == proto::desktop::AUDIO_ENCODING_DEFAULT)
        config->set_audio_encoding(kDefaultAudioEncoding);
}

} // namespace client
