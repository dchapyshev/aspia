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

#include "client/config.h"

#include "proto/desktop_control.h"
#include "proto/router.h"

//--------------------------------------------------------------------------------------------------
RouterConfig::RouterConfig()
    : session_type(proto::router::SESSION_TYPE_CLIENT)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool RouterConfig::isValid() const
{
    return !address.isEmpty() && !username.isEmpty() && !password.isEmpty();
}

//--------------------------------------------------------------------------------------------------
bool RouterConfig::hasSameParams(const RouterConfig& other) const
{
    return address == other.address && session_type == other.session_type &&
           username == other.username && password == other.password;
}

//--------------------------------------------------------------------------------------------------
QString RouterConfig::displayName() const
{
    if (!display_name.isEmpty())
        return display_name;
    return address;
}

//--------------------------------------------------------------------------------------------------
proto::control::Config defaultDesktopConfig()
{
    proto::control::Config config;
    config.set_audio(true);
    config.set_cursor_shape(true);
    config.set_cursor_position(false);
    config.set_clipboard(true);
    config.set_effects(false);
    config.set_wallpaper(false);
    config.set_block_input(false);
    config.set_lock_at_disconnect(false);
    return config;
}
