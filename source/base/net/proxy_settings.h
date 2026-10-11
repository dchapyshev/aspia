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

#ifndef BASE_NET_PROXY_SETTINGS_H
#define BASE_NET_PROXY_SETTINGS_H

#include <QObject>

#include "base/crypto/secure_string.h"

// A proxy server for outgoing connections. An empty |user| means that the proxy does not require
// authentication.
struct ProxySettings
{
    Q_GADGET

public:
    enum class Type { HTTP, HTTPS, SOCKS4, SOCKS4A, SOCKS5, SOCKS5H };
    Q_ENUM(Type)

    bool enabled = false;
    Type type = Type::HTTP;
    QString address;
    quint16 port = 0;
    QString user;
    SecureString password;
};

#endif // BASE_NET_PROXY_SETTINGS_H
