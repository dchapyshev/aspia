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

#include "client/desktop/management/workspace_edit_model.h"

//--------------------------------------------------------------------------------------------------
WorkspaceEditModel::WorkspaceEditModel(qint64 entry_id)
    : entry_id_(entry_id)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool WorkspaceEditModel::applyWorkspaceList(const QList<WorkspaceInfo>& workspaces)
{
    other_names_.clear();

    bool editing_found = false;
    for (const WorkspaceInfo& workspace : workspaces)
    {
        if (isModifyMode() && workspace.entry_id == entry_id_)
        {
            editing_found = true;
            server_name_ = workspace.name;
            server_comment_ = workspace.comment;
            server_access_ids_ = workspace.access_ids;
            pending_revision_ = workspace.revision;
        }
        else
        {
            // The own name is excluded so the uniqueness check does not flag an unchanged name.
            other_names_.append(workspace.name);
        }
    }

    workspaces_loaded_ = true;
    return !isModifyMode() || editing_found;
}

//--------------------------------------------------------------------------------------------------
void WorkspaceEditModel::applyUserList(const QList<User>& users)
{
    users_.clear();
    for (const User& user : users)
        users_.insert(user.entry_id, user);

    users_loaded_ = true;
}

//--------------------------------------------------------------------------------------------------
void WorkspaceEditModel::applyHostList(const QList<HostInfo>& hosts)
{
    hosts_.clear();
    server_host_ids_.clear();

    for (const HostInfo& host : hosts)
    {
        // Hosts of other workspaces are not visible in this dialog. Only the unassigned ones
        // (available to add) and the ones already in the workspace being edited.
        const bool ours = isModifyMode() && host.workspace_id == entry_id_;
        if (host.workspace_id != 0 && !ours)
            continue;

        Host& entry = hosts_[host.host_id];
        entry.host_id = host.host_id;
        entry.computer_name = host.computer_name;

        if (ours)
            server_host_ids_.insert(host.host_id);
    }

    // Commit the revision parked by the workspace reply of this refetch cycle: from here on
    // the revision and the host snapshot describe the same server state (see baseRevision()).
    base_revision_ = pending_revision_;

    hosts_loaded_ = true;
}

//--------------------------------------------------------------------------------------------------
bool WorkspaceEditModel::isLoaded() const
{
    return workspaces_loaded_ && users_loaded_ && hosts_loaded_;
}

//--------------------------------------------------------------------------------------------------
void WorkspaceEditModel::grantUser(qint64 user_id)
{
    removed_user_ids_.remove(user_id);
    added_user_ids_.insert(user_id);
}

//--------------------------------------------------------------------------------------------------
void WorkspaceEditModel::revokeUser(qint64 user_id)
{
    added_user_ids_.remove(user_id);
    removed_user_ids_.insert(user_id);
}

//--------------------------------------------------------------------------------------------------
void WorkspaceEditModel::claimHost(quint64 host_id)
{
    removed_host_ids_.remove(host_id);
    added_host_ids_.insert(host_id);
}

//--------------------------------------------------------------------------------------------------
void WorkspaceEditModel::releaseHost(quint64 host_id)
{
    added_host_ids_.remove(host_id);
    removed_host_ids_.insert(host_id);
}

//--------------------------------------------------------------------------------------------------
QSet<qint64> WorkspaceEditModel::effectiveAccessIds() const
{
    QSet<qint64> ids = server_access_ids_;

    ids.unite(added_user_ids_);
    ids.subtract(removed_user_ids_);

    const auto is_gone = [this](qint64 user_id) { return !users_.contains(user_id); };
    ids.removeIf(is_gone);
    return ids;
}

//--------------------------------------------------------------------------------------------------
QSet<quint64> WorkspaceEditModel::effectiveHostIds() const
{
    QSet<quint64> ids = server_host_ids_;
    ids.unite(added_host_ids_);
    ids.subtract(removed_host_ids_);

    const auto is_gone = [this](quint64 host_id) { return !hosts_.contains(host_id); };
    ids.removeIf(is_gone);
    return ids;
}

//--------------------------------------------------------------------------------------------------
QList<WorkspaceEditModel::User> WorkspaceEditModel::memberUsers() const
{
    const QSet<qint64> access_ids = effectiveAccessIds();

    QList<User> members;
    for (const User& user : users_)
    {
        if (access_ids.contains(user.entry_id))
            members.append(user);
    }
    return members;
}

//--------------------------------------------------------------------------------------------------
QList<WorkspaceEditModel::User> WorkspaceEditModel::availableUsers() const
{
    const QSet<qint64> access_ids = effectiveAccessIds();

    QList<User> available;
    for (const User& user : users_)
    {
        if (!access_ids.contains(user.entry_id))
            available.append(user);
    }
    return available;
}

//--------------------------------------------------------------------------------------------------
QList<WorkspaceEditModel::Host> WorkspaceEditModel::hostsInWorkspace() const
{
    const QSet<quint64> host_ids = effectiveHostIds();

    QList<Host> members;
    for (const Host& host : hosts_)
    {
        if (host_ids.contains(host.host_id))
            members.append(host);
    }
    return members;
}

//--------------------------------------------------------------------------------------------------
QList<WorkspaceEditModel::Host> WorkspaceEditModel::availableHosts() const
{
    const QSet<quint64> host_ids = effectiveHostIds();

    QList<Host> available;
    for (const Host& host : hosts_)
    {
        if (!host_ids.contains(host.host_id))
            available.append(host);
    }
    return available;
}

//--------------------------------------------------------------------------------------------------
QList<qint64> WorkspaceEditModel::accessUserIdsForSave() const
{
    const QSet<qint64> access_ids = effectiveAccessIds();
    return QList<qint64>(access_ids.begin(), access_ids.end());
}

//--------------------------------------------------------------------------------------------------
QList<quint64> WorkspaceEditModel::hostIdsForSave() const
{
    const QSet<quint64> host_ids = effectiveHostIds();
    return QList<quint64>(host_ids.begin(), host_ids.end());
}
