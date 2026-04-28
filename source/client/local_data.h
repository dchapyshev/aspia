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

#ifndef CLIENT_LOCAL_DATA_H
#define CLIENT_LOCAL_DATA_H

#include <QByteArray>
#include <QString>

namespace client {

struct ComputerData
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

struct GroupData
{
    qint64 id = -1;
    qint64 parent_id = 0;
    QString name;
    QString comment;
    bool expanded = false;
};

} // namespace client

#endif // CLIENT_LOCAL_DATA_H
