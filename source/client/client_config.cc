//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/client_config.h"

#include "build/build_config.h"

namespace client {

//--------------------------------------------------------------------------------------------------
Config::Config()
    : port(DEFAULT_HOST_TCP_PORT),
      session_type(proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Config::~Config() = default;

} // namespace client
