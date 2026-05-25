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
#include <QSet>
#include <QString>

#include <string_view>

#include "base/peer/host_id.h"
#include "base/peer/router_user.h"
#include "router/workspace.h"

struct HostInfo
{
    qint64 host_id      = 0; // hosts.id - the dialable host identifier.
    qint64 workspace_id = 0;
    qint64 group_id     = 0;
    QString display_name;
    QString computer_name;
    QString cpu_arch;
    QString version;
    QString os_name;
    QString address;
    QByteArray comment;
    QByteArray user_name;
    QByteArray password;
    qint64 last_connect = 0;
    qint64 last_modify  = 0;
};

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
    RouterUser findUser(qint64 entry_id) const;
    std::string_view hostId(const QByteArray& key_hash, HostId* host_id) const;
    bool addHost(const QByteArray& key_hash);

    // Called by the router on every host connection to refresh the host's last-seen metadata
    // (computer_name, cpu_arch, version, os_name, address, last_connect). If display_name has
    // never been set by the admin it is seeded from computer_name so the host has a readable
    // label in the UI.
    bool updateHostInfo(HostId host_id,
                        const QString& computer_name,
                        const QString& cpu_arch,
                        const QString& version,
                        const QString& os_name,
                        const QString& address);

    // Schedules a host removal: copies hosts.id/key into hosts_remove (together with the current
    // timestamp) and deletes the original hosts row. The host_id is kept intact so that a
    // reconnecting offline host can be matched by key in hostId().
    bool scheduleHostRemoval(HostId host_id);
    // Returns true if host_id has a pending removal in hosts_remove.
    bool hasPendingHostRemoval(HostId host_id) const;
    // Removes the hosts_remove row once the host has acknowledged the removal command.
    bool finalizeHostRemoval(HostId host_id);

    QVector<Workspace> workspaceList() const;
    Workspace findWorkspace(qint64 entry_id) const;
    // Initial access list may be empty - in that case the workspace has no GK yet; it will
    // be generated when the first access is granted via modifyWorkspace(). The comment is
    // stored as opaque bytes (AEAD-encrypted with the workspace GK on the client).
    std::string_view addWorkspace(
        const QString& name, const QByteArray& comment,
        const QVector<Workspace::Access>& initial_access, qint64* entry_id);
    // Updates name/comment and synchronizes access in a single transaction. desired_access is
    // the complete final access list: user_ids missing from it are revoked, user_ids absent
    // from the current DB record are inserted with the supplied wrapped_gk, and user_ids
    // already present preserve their existing wrapped_gk (the value in desired_access is
    // ignored).
    std::string_view modifyWorkspace(
        qint64 entry_id, const QString& name, const QByteArray& comment,
        const QVector<Workspace::Access>& desired_access);
    std::string_view removeWorkspace(qint64 entry_id);

    // Assigns hosts to the given workspace. desired_host_ids is the complete final set: hosts
    // currently in this workspace but absent from the set are released (workspace_id <- 0);
    // hosts in the set with workspace_id 0 are claimed (workspace_id <- entry_id); hosts in
    // the set that already belong to another workspace are left alone (the operator cannot
    // hijack a host from another workspace through this call).
    std::string_view setWorkspaceHosts(qint64 entry_id, const QSet<qint64>& desired_host_ids);

    QVector<Workspace::Access> workspaceAccessList(qint64 workspace_id) const;
    // Returns the set of workspace ids the given user has a workspace_access entry for.
    QSet<qint64> workspaceAccessListForUser(qint64 user_id) const;
    bool hasWorkspaceAccess(qint64 user_id, qint64 workspace_id) const;

    // Returns every host in the database (admin-only call site). [start_item, end_item] gives an
    // inclusive paging window; pass end_item <= 0 to disable paging.
    QVector<HostInfo> hosts(qint64 start_item, qint64 end_item) const;
    // Returns hosts in the given workspace and group with exact match on both columns.
    // [start_item, end_item] gives an inclusive paging window; pass end_item <= 0 to disable.
    QVector<HostInfo> hosts(
        qint64 workspace_id, qint64 group_id, qint64 start_item, qint64 end_item) const;

private:
    explicit Database(const QString& connection_name);
    static QString databaseDirectory();

    QString connection_name_;

    Q_DISABLE_COPY(Database)
};

#endif // ROUTER_DATABASE_H
