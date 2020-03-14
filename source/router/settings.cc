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

#include "router/settings.h"

namespace router {

Settings::Settings()
    : impl_(base::XmlSettings::Scope::SYSTEM, "aspia", "router")
{
    // Nothing
}

Settings::~Settings() = default;

uint16_t Settings::port() const
{
    return impl_.get<uint16_t>("Port", DEFAULT_ROUTER_TCP_PORT);
}

base::ByteArray Settings::privateKey() const
{
    return impl_.get<base::ByteArray>("PrivateKey");
}

} // namespace router
