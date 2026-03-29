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
#include "build/build_config.h"

namespace console {

//--------------------------------------------------------------------------------------------------
void ComputerFactory::setDefaultDesktopManageConfig(proto::address_book::DesktopConfig* config)
{
    DCHECK(config);

    static const quint32 kDefaultFlags =
        proto::address_book::ENABLE_CLIPBOARD | proto::address_book::ENABLE_CURSOR_SHAPE |
        proto::address_book::DISABLE_EFFECTS | proto::address_book::DISABLE_WALLPAPER;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(proto::video::ENCODING_VP8);
    config->set_audio_encoding(proto::audio::ENCODING_OPUS);
}

//--------------------------------------------------------------------------------------------------
void ComputerFactory::setDefaultDesktopViewConfig(proto::address_book::DesktopConfig* config)
{
    DCHECK(config);

    static const quint32 kDefaultFlags =
        proto::address_book::DISABLE_EFFECTS | proto::address_book::DISABLE_WALLPAPER;

    config->set_flags(kDefaultFlags);
    config->set_video_encoding(proto::video::ENCODING_VP8);
    config->set_audio_encoding(proto::audio::ENCODING_OPUS);
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

//--------------------------------------------------------------------------------------------------
// static
proto::control::Config ComputerFactory::toClientConfig(const proto::address_book::DesktopConfig& config)
{
    proto::control::Config client_config;

    client_config.set_video_encoding(config.video_encoding());
    client_config.set_audio_encoding(config.audio_encoding());

    const quint32 flags = config.flags();

    client_config.set_cursor_shape(flags & proto::address_book::ENABLE_CURSOR_SHAPE);
    client_config.set_cursor_position(flags & proto::address_book::CURSOR_POSITION);
    client_config.set_clipboard(flags & proto::address_book::ENABLE_CLIPBOARD);
    client_config.set_effects(!(flags & proto::address_book::DISABLE_EFFECTS));
    client_config.set_wallpaper(!(flags & proto::address_book::DISABLE_WALLPAPER));
    client_config.set_block_input(flags & proto::address_book::BLOCK_REMOTE_INPUT);
    client_config.set_lock_at_disconnect(flags & proto::address_book::LOCK_AT_DISCONNECT);

    return client_config;
}

} // namespace console
