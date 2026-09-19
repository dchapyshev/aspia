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

#ifndef BASE_BUILD_CONFIG_H
#define BASE_BUILD_CONFIG_H

#include <QByteArrayView>
#include <QLatin1StringView>

#include <array>

inline constexpr QLatin1StringView kDefaultLocale("en");
inline constexpr QLatin1StringView kDefaultUpdateServer("https://aspia.org/updates");

inline constexpr quint16 kDefaultHostTcpPort             = 8050;
inline constexpr quint16 kDefaultRouterLegacyHostTcpPort = 8060;
inline constexpr quint16 kDefaultRouterHostTcpPort       = 8061;
inline constexpr quint16 kDefaultRouterClientTcpPort     = 8062;
inline constexpr quint16 kDefaultRouterRelayTcpPort      = 8063;
inline constexpr quint16 kDefaultRelayPeerTcpPort        = 8070;
inline constexpr quint16 kDefaultStunPort                = 8065;

// Public keys the update files are signed with. There is room for more than one so that a key
// can be replaced without cutting off the applications that know only the old one.
extern const std::array<QByteArrayView, 1> kUpdatePublicKeys;

#endif // BASE_BUILD_CONFIG_H
