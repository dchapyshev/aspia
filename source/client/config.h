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

#include "base/crypto/secure_string.h"

namespace proto::router {
enum SessionType : int;
} // namespace proto::router

namespace proto::control {
class Config;
} // namespace proto::control

struct RouterConfig
{
    RouterConfig();

    bool isValid() const;
    bool hasSameParams(const RouterConfig& other) const;
    QString displayName() const;

    qint64 router_id = -1;
    QString display_name;
    QString address;
    proto::router::SessionType session_type;
    QString username;
    SecureString password;
    QByteArray data;
};

struct ComputerConfig
{
    qint64 id = -1;
    qint64 group_id = 0;
    qint64 router_id = 0;
    QString name;
    QString comment;
    QString address;
    QString username;
    SecureString password;
    qint64 create_time = 0;
    qint64 modify_time = 0;
    qint64 connect_time = 0;
    QByteArray data;
};

struct GroupConfig
{
    qint64 id = -1;
    qint64 parent_id = 0;
    QString name;
    QString comment;
    QByteArray data;
};

proto::control::Config defaultDesktopConfig();

#endif // CLIENT_CONFIG_H
