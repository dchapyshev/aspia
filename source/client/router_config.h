//
// Aspia Project
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

#ifndef CLIENT_ROUTER_CONFIG_H
#define CLIENT_ROUTER_CONFIG_H

#include <QList>
#include <QString>

namespace client {

struct RouterConfig
{
    RouterConfig();
    ~RouterConfig();

    RouterConfig(const RouterConfig& other) = default;
    RouterConfig& operator=(const RouterConfig& other) = default;

    RouterConfig(RouterConfig&& other) noexcept = default;
    RouterConfig& operator=(RouterConfig&& other) noexcept = default;

    bool isValid() const;

    QString address;
    quint16 port;
    QString username;
    QString password;
};

using RouterConfigList = QList<RouterConfig>;

} // namespace client

#endif // CLIENT_ROUTER_CONFIG_H
