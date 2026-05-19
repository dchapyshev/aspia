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

#include <QByteArray>
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

    // Called by the router on every host connection to keep computers in sync. If at least one
    // computers row references this host_id, all of them get their computer_name, address and
    // last_connect refreshed. Otherwise an unassigned (workspace_id=0, group_id=0) row is
    // inserted so the host shows up in the admin UI awaiting assignment.
    bool updateComputerInfo(HostId host_id, const QString& computer_name, const QString& address);

    QVector<Workspace> workspaceList() const;
    Workspace findWorkspace(qint64 entry_id) const;
    // Initial access list may be empty - in that case the workspace has no GK yet; it will
    // be generated when the first access is granted via modifyWorkspace().
    std::string_view addWorkspace(
        const QString& name, const QVector<Workspace::Access>& initial_access, qint64* entry_id);
    // Updates name and synchronizes access in a single transaction. desired_access is the
    // complete final access list: user_ids missing from it are revoked, user_ids absent from
    // the current DB record are inserted with the supplied wrapped_gk, and user_ids already
    // present preserve their existing wrapped_gk (the value in desired_access is ignored).
    std::string_view modifyWorkspace(
        qint64 entry_id, const QString& name, const QVector<Workspace::Access>& desired_access);
    std::string_view removeWorkspace(qint64 entry_id);

    QVector<Workspace::Access> workspaceAccessList(qint64 workspace_id) const;
    QVector<Workspace::Access> workspaceAccessListForUser(qint64 user_id) const;

private:
    explicit Database(const QString& connection_name);
    static QString databaseDirectory();

    QString connection_name_;

    Q_DISABLE_COPY(Database)
};

#endif // ROUTER_DATABASE_H
