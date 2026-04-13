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

#include "build/build_config.h"

namespace client {

//--------------------------------------------------------------------------------------------------
RouterConfig::RouterConfig()
    : uuid(QUuid::createUuid()),
      port(DEFAULT_ROUTER_TCP_PORT),
      session_type(proto::router::SESSION_TYPE_CLIENT)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
RouterConfig::~RouterConfig() = default;

//--------------------------------------------------------------------------------------------------
bool RouterConfig::isValid() const
{
    return !address.isEmpty() && port && !username.isEmpty() && !password.isEmpty();
}

//--------------------------------------------------------------------------------------------------
bool RouterConfig::hasSameConnectionParams(const RouterConfig& other) const
{
    return address == other.address
        && port == other.port
        && session_type == other.session_type
        && username == other.username
        && password == other.password;
}

//--------------------------------------------------------------------------------------------------
Config::Config()
    : port(DEFAULT_HOST_TCP_PORT),
      session_type(proto::peer::SESSION_TYPE_DESKTOP)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Config::~Config() = default;

} // namespace client
