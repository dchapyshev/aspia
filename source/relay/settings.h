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

#include "base/xml_settings.h"

namespace relay {

class Settings
{
public:
    Settings();
    ~Settings();

    uint16_t controllerPort() const;
    uint16_t peerPort() const;
    size_t maxControllerCount() const;
    size_t maxPeerCount() const;
    base::ByteArray controllerPublicKey() const;
    base::ByteArray proxyPrivateKey() const;

private:
    base::XmlSettings impl_;
};

} // namespace relay

#endif // RELAY__SETTINGS_H
