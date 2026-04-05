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

//--------------------------------------------------------------------------------------------------
// static
proto::control::Config ConfigFactory::defaultDesktopConfig()
{
    proto::control::Config config;
    setDefaultDesktopConfig(&config);
    return config;
}

//--------------------------------------------------------------------------------------------------
// static
void ConfigFactory::setDefaultDesktopConfig(proto::control::Config* config)
{
    DCHECK(config);
    config->set_audio(true);
    config->set_cursor_shape(true);
    config->set_cursor_position(false);
    config->set_clipboard(true);
    config->set_effects(false);
    config->set_wallpaper(false);
    config->set_block_input(false);
    config->set_lock_at_disconnect(false);
}

} // namespace client
