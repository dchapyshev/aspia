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

#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

#include <QList>
#include <QString>

#include "proto/peer.h"
#include "proto/router.h"

#include <optional>

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
    bool hasSameConnectionParams(const RouterConfig& other) const;

    qint64 id = -1;
    QString name;
    QString address;
    quint16 port;
    proto::router::SessionType session_type;
    QString username;
    QString password;
};

struct Config
{
    Config();
    ~Config();

    std::optional<RouterConfig> router_config;

    QString computer_name;
    QString display_name;
    QString address_or_id;
    quint16 port;
    QString username;
    QString password;
    proto::peer::SessionType session_type;
};

} // namespace client

#endif // CLIENT_CONFIG_H
