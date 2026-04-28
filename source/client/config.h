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

#include <QByteArray>
#include <QList>
#include <QString>

#include "proto/router.h"

namespace client {

struct RouterConfig
{
    bool isValid() const
    {
        return !address.isEmpty() && !username.isEmpty() && !password.isEmpty();
    }

    bool hasSameConnectionParams(const RouterConfig& other) const
    {
        return address == other.address && session_type == other.session_type &&
               username == other.username && password == other.password;
    }

    QString displayName() const
    {
        if (!display_name.isEmpty())
            return display_name;
        return address;
    }

    qint64 router_id = -1;
    QString display_name;
    QString address;
    proto::router::SessionType session_type = proto::router::SESSION_TYPE_CLIENT;
    QString username;
    QString password;
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
    QString password;
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
    bool expanded = false;
};

} // namespace client

#endif // CLIENT_CONFIG_H
