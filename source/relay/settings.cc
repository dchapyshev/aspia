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

#include "relay/settings.h"

namespace relay {

Settings::Settings()
    : impl_("aspia_relay")
{
    // Nothing
}

Settings::~Settings() = default;

void Settings::setRouterAddress(const std::u16string& address)
{
    impl_.set<std::u16string>("RouterAddress", address);
}

std::u16string Settings::routerAddress() const
{
    return impl_.get<std::u16string>("RouterAddress");
}

void Settings::setRouterPort(uint16_t port)
{
    impl_.set<uint16_t>("RouterPort", port);
}

uint16_t Settings::routerPort() const
{
    return impl_.get<uint16_t>("RouterPort", DEFAULT_ROUTER_TCP_PORT);
}

void Settings::setRouterPublicKey(const base::ByteArray& public_key)
{
    impl_.set<base::ByteArray>("RouterPublicKey", public_key);
}

base::ByteArray Settings::routerPublicKey() const
{
    return impl_.get<base::ByteArray>("RouterPublicKey");
}

void Settings::setPeerPort(uint16_t port)
{
    impl_.set<uint16_t>("PeerPort", port);
}

uint16_t Settings::peerPort() const
{
    return impl_.get<uint16_t>("PeerPort", DEFAULT_RELAY_PEER_TCP_PORT);
}

} // namespace relay
