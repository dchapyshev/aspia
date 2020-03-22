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

#include "proxy/settings.h"

namespace proxy {

Settings::Settings()
    : impl_(base::XmlSettings::Scope::SYSTEM, "aspia", "proxy")
{
    // Nothing
}

Settings::~Settings() = default;

uint16_t Settings::controllerPort() const
{
    return impl_.get<uint16_t>("ControllerPort", DEFAULT_PROXY_CONTROLLER_TCP_PORT);
}

uint16_t Settings::peerPort() const
{
    return impl_.get<uint16_t>("PeerPort", DEFAULT_PROXY_PEER_TCP_PORT);
}

size_t Settings::maxControllerCount() const
{
    return impl_.get<size_t>("MaxControllerCount", 1);
}

size_t Settings::maxPeerCount() const
{
    return impl_.get<size_t>("MaxPeerCount", 100);
}

base::ByteArray Settings::controllerPublicKey() const
{
    return base::fromHex(impl_.get<std::string>("ControllerPublicKey"));
}

base::ByteArray Settings::proxyPrivateKey() const
{
    return base::fromHex(impl_.get<std::string>("ProxyPrivateKey"));
}

} // namespace proxy
