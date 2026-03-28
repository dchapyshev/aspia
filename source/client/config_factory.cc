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

namespace client {

namespace {

const proto::video::Encoding kDefaultVideoEncoding = proto::video::ENCODING_VP8;
const proto::audio::Encoding kDefaultAudioEncoding = proto::audio::ENCODING_OPUS;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
proto::control::Config ConfigFactory::defaultDesktopManageConfig()
{
    proto::control::Config config;
    setDefaultDesktopManageConfig(&config);
    return config;
}

//--------------------------------------------------------------------------------------------------
// static
proto::control::Config ConfigFactory::defaultDesktopViewConfig()
{
    proto::control::Config config;
    setDefaultDesktopViewConfig(&config);
    return config;
}

//--------------------------------------------------------------------------------------------------
// static
void ConfigFactory::setDefaultDesktopManageConfig(proto::control::Config* config)
{
    DCHECK(config);

    static const quint32 kDefaultFlags =
        proto::control::ENABLE_CLIPBOARD | proto::control::ENABLE_CURSOR_SHAPE |
        proto::control::DISABLE_EFFECTS | proto::control::DISABLE_WALLPAPER;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(kDefaultVideoEncoding);
    config->set_audio_encoding(kDefaultAudioEncoding);
}

//--------------------------------------------------------------------------------------------------
// static
void ConfigFactory::setDefaultDesktopViewConfig(proto::control::Config* config)
{
    DCHECK(config);

    static const quint32 kDefaultFlags =
        proto::control::DISABLE_EFFECTS | proto::control::DISABLE_WALLPAPER;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(kDefaultVideoEncoding);
    config->set_audio_encoding(kDefaultAudioEncoding);
}

} // namespace client
