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

#ifndef RELAY__SETTINGS_H
#define RELAY__SETTINGS_H

#include "base/settings/json_settings.h"

namespace relay {

class Settings
{
public:
    Settings();
    ~Settings();

    void setRouterAddress(const std::u16string& address);
    std::u16string routerAddress() const;

    void setRouterPort(uint16_t port);
    uint16_t routerPort() const;

    void setRouterPublicKey(const base::ByteArray& public_key);
    base::ByteArray routerPublicKey() const;

    void setPeerPort(uint16_t port);
    uint16_t peerPort() const;

    void setMaxPeerCount(uint32_t count);
    uint32_t maxPeerCount() const;

private:
    base::JsonSettings impl_;
};

} // namespace relay

#endif // RELAY__SETTINGS_H
