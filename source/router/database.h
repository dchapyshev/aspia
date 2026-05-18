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

#ifndef ROUTER_DATABASE_H
#define ROUTER_DATABASE_H

#include <QString>

#include <string_view>

#include "base/peer/host_id.h"
#include "base/peer/router_user.h"
#include "router/workspace.h"

class Database
{
public:
    Database();
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    ~Database();

    static Database create();
    static Database open();
    static QString filePath();

    bool isValid() const;
    QVector<RouterUser> userList() const;
    bool addUser(const RouterUser& user);
    bool modifyUser(const RouterUser& user);
    bool removeUser(qint64 entry_id);
    RouterUser findUser(const QString& username) const;
    std::string_view hostId(const QByteArray& key_hash, HostId* host_id) const;
    bool addHost(const QByteArray& key_hash);

    QVector<Workspace> workspaceList() const;
    Workspace findWorkspace(qint64 entry_id) const;
    std::string_view addWorkspace(const QString& name, qint64* entry_id);
    std::string_view modifyWorkspace(qint64 entry_id, const QString& name);
    std::string_view removeWorkspace(qint64 entry_id);

private:
    explicit Database(const QString& connection_name);
    static QString databaseDirectory();

    QString connection_name_;

    Q_DISABLE_COPY(Database)
};

#endif // ROUTER_DATABASE_H
