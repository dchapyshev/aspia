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

#ifndef CLIENT_CLIENT_CONFIG_H
#define CLIENT_CLIENT_CONFIG_H

#include "client/router_config.h"
#include "proto/common.h"

#include <optional>

namespace client {

struct Config
{
    Config();
    ~Config();

    std::optional<RouterConfig> router_config;

    QString computer_name;
    QString display_name;
    QString address_or_id;
    uint16_t port;
    QString username;
    QString password;
    proto::SessionType session_type;
};

} // namespace client

#endif // CLIENT_CLIENT_CONFIG_H
