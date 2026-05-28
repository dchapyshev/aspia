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
    quint64 host_id     = 0; // hosts.id - the dialable host identifier.
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

// A node in the per-workspace host group tree (groups_<workspace_id> row). Root nodes have
// parent_id == 0. Names are not required to be unique within a parent: two siblings with the
// same name are allowed and the client disambiguates by entry_id. The tree is a plain adjacency
// list: cycle protection on move uses a recursive CTE that walks parent links from the proposed
// new parent and refuses the move if it reaches the node being moved.
struct Group
{
    qint64 entry_id  = 0;
    qint64 parent_id = 0; // 0 means the group sits at the workspace root.
    QString name;
    QByteArray comment;   // AEAD-encrypted with the workspace GK.
};

class Database
{
public:
    ~Database() = default;

    // Returns the per-thread cached Database. First call on a given thread opens the
    // connection and ensures the schema; subsequent calls are O(1). Connection lives until
    // the thread exits.
    static Database& instance();

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
    bool updateHostInfo(HostId host_id, const QString& computer_name, const QString& cpu_arch,
        const QString& version, const QString& os_name, const QString& address);

    // Returns the workspace_id of the given host, or 0 if the host is not assigned to a
    // workspace. Returns -1 if the host_id is unknown. Used to validate user access before
    // edits.
    qint64 hostWorkspaceId(HostId host_id) const;

    // Updates the admin/manager-editable fields of a host (display_name plain, the other three
    // AEAD-encrypted with the workspace GK by the caller). Also bumps last_modify.
    bool modifyHost(HostId host_id, const QString& display_name, const QByteArray& comment,
        const QByteArray& user_name, const QByteArray& password);

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
    std::string_view addWorkspace(const QString& name, const QByteArray& comment,
        const QVector<Workspace::Access>& initial_access, qint64* entry_id);

    // Updates name/comment and synchronizes access in a single transaction. desired_access is
    // the complete final access list: user_ids missing from it are revoked, user_ids absent
    // from the current DB record are inserted with the supplied wrapped_gk, and user_ids
    // already present preserve their existing wrapped_gk (the value in desired_access is
    // ignored).
    std::string_view modifyWorkspace(qint64 entry_id, const QString& name, const QByteArray& comment,
        const QVector<Workspace::Access>& desired_access);
    std::string_view removeWorkspace(qint64 entry_id);

    // Assigns hosts to the given workspace. desired_host_ids is the complete final set: hosts
    // currently in this workspace but absent from the set are released (workspace_id <- 0);
    // hosts in the set with workspace_id 0 are claimed (workspace_id <- entry_id); hosts in
    // the set that already belong to another workspace are left alone (the operator cannot
    // hijack a host from another workspace through this call).
    std::string_view setWorkspaceHosts(qint64 entry_id, const QSet<HostId>& desired_host_ids);

    // Returns the entire group tree of the given workspace, ordered by parent_id then name.
    // The caller can build a tree by indexing on entry_id and linking via parent_id.
    QVector<Group> groupList(qint64 workspace_id) const;

    // Returns direct children of parent_id within workspace_id. parent_id == 0 returns root
    // groups (parent_id IS NULL in the table).
    QVector<Group> groupChildren(qint64 workspace_id, qint64 parent_id) const;

    // Returns the group with the given entry_id from workspace_id. Returns an empty Group
    // (entry_id == 0) if the row is missing or workspace_id has no groups_<W> table.
    Group findGroup(qint64 workspace_id, qint64 entry_id) const;

    // Inserts a new group. parent_id == 0 places it at the workspace root; otherwise parent_id
    // must reference an existing row in groups_<workspace_id>. The path column is computed
    // server-side from the parent's path. On success *entry_id is set to the new id.
    std::string_view addGroup(qint64 workspace_id, qint64 parent_id, const QString& name,
        const QByteArray& comment, qint64* entry_id);

    // Renames and/or re-parents a group. new_parent_id must point to a group in the same
    // workspace and must not be the group itself or one of its descendants. The cycle check
    // runs a recursive CTE that walks parent links upward from new_parent_id; if entry_id
    // appears anywhere in that chain the move is refused.
    std::string_view modifyGroup(qint64 workspace_id, qint64 entry_id, qint64 new_parent_id,
        const QString& name, const QByteArray& comment);

    // Deletes the group and all its descendants. Descendants are removed by the foreign-key
    // cascade on parent_id; hosts whose group_id pointed into the deleted subtree are detached
    // to the workspace root by the foreign-key SET NULL on hosts_<W>.group_id.
    std::string_view removeGroup(qint64 workspace_id, qint64 entry_id);

    QVector<Workspace::Access> workspaceAccessList(qint64 workspace_id) const;

    // Returns the set of workspace ids the given user has a workspace_access entry for.
    QSet<qint64> workspaceAccessListForUser(qint64 user_id) const;
    bool hasWorkspaceAccess(qint64 user_id, qint64 workspace_id) const;

    // Returns every host in the database (admin-only call site). [start_item, end_item] gives an
    // inclusive paging window; pass end_item <= 0 to disable paging.
    QVector<HostInfo> hosts(qint64 start_item, qint64 end_item) const;

    // Returns hosts in the given workspace and group with exact match on both columns.
    // [start_item, end_item] gives an inclusive paging window; pass end_item <= 0 to disable.
    QVector<HostInfo> hosts(qint64 workspace_id, qint64 group_id, qint64 start_item, qint64 end_item) const;

    // Total host count in the same scope as the matching hosts() overload. Used by the client
    // to drive pagination UI without fetching the full list.
    qint64 hostCount() const;
    qint64 hostCount(qint64 workspace_id, qint64 group_id) const;

private:
    Database() = default;

    bool openDatabase();

    bool valid_ = false;

    Q_DISABLE_COPY_MOVE(Database)
};

#endif // ROUTER_DATABASE_H
