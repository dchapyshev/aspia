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

#include "console/computer_factory.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/desktop/pixel_format.h"
#include "build/build_config.h"

namespace console {

namespace {

const int kDefCompressRatio = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
void ComputerFactory::setDefaultDesktopManageConfig(proto::desktop::Config* config)
{
    DCHECK(config);

    static const quint32 kDefaultFlags =
        proto::desktop::ENABLE_CLIPBOARD | proto::desktop::ENABLE_CURSOR_SHAPE |
        proto::desktop::DISABLE_EFFECTS | proto::desktop::DISABLE_WALLPAPER |
        proto::desktop::CLEAR_CLIPBOARD;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(proto::desktop::VIDEO_ENCODING_VP8);
    config->set_audio_encoding(proto::desktop::AUDIO_ENCODING_OPUS);
    config->set_compress_ratio(kDefCompressRatio);
    config->mutable_pixel_format()->CopyFrom(base::serialize(base::PixelFormat::RGB332()));
}

//--------------------------------------------------------------------------------------------------
void ComputerFactory::setDefaultDesktopViewConfig(proto::desktop::Config* config)
{
    DCHECK(config);

    static const quint32 kDefaultFlags =
        proto::desktop::DISABLE_EFFECTS | proto::desktop::DISABLE_WALLPAPER;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(proto::desktop::VIDEO_ENCODING_VP8);
    config->set_audio_encoding(proto::desktop::AUDIO_ENCODING_OPUS);
    config->set_compress_ratio(kDefCompressRatio);
    config->mutable_pixel_format()->CopyFrom(base::serialize(base::PixelFormat::RGB332()));
}

//--------------------------------------------------------------------------------------------------
// static
proto::address_book::Computer ComputerFactory::defaultComputer()
{
    proto::address_book::Computer computer;

    computer.set_session_type(proto::peer::SESSION_TYPE_DESKTOP_MANAGE);
    computer.set_port(DEFAULT_HOST_TCP_PORT);

    proto::address_book::InheritConfig* inherit = computer.mutable_inherit();
    inherit->set_credentials(false);
    inherit->set_desktop_manage(true);
    inherit->set_desktop_view(true);

    setDefaultDesktopManageConfig(computer.mutable_session_config()->mutable_desktop_manage());
    setDefaultDesktopViewConfig(computer.mutable_session_config()->mutable_desktop_view());

    return computer;
}

} // namespace console
