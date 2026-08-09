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
            base_revision_ = workspace.revision;
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
void WorkspaceEditModel::applyUserPage(const QList<User>& users)
{
    page_users_ = users;
    users_loaded_ = true;
}

//--------------------------------------------------------------------------------------------------
void WorkspaceEditModel::applyMemberUser(const User& user)
{
    missing_user_ids_.remove(user.entry_id);
    member_users_.insert(user.entry_id, user);
}

//--------------------------------------------------------------------------------------------------
void WorkspaceEditModel::applyMissingUser(qint64 user_id)
{
    member_users_.remove(user_id);
    missing_user_ids_.insert(user_id);
}

//--------------------------------------------------------------------------------------------------
bool WorkspaceEditModel::isLoaded() const
{
    return workspaces_loaded_ && users_loaded_;
}

//--------------------------------------------------------------------------------------------------
QList<qint64> WorkspaceEditModel::unresolvedMemberIds() const
{
    QList<qint64> ids;
    for (qint64 user_id : effectiveAccessIds())
    {
        if (!knownUser(user_id))
            ids.append(user_id);
    }
    return ids;
}

//--------------------------------------------------------------------------------------------------
const WorkspaceEditModel::User* WorkspaceEditModel::knownUser(qint64 user_id) const
{
    const auto it = member_users_.constFind(user_id);
    if (it != member_users_.constEnd())
        return &(*it);

    // A user granted from the page of candidates is known by name already, so it is shown by it
    // right away instead of waiting for a lookup that would answer what is on the screen.
    for (const User& user : page_users_)
    {
        if (user.entry_id == user_id)
            return &user;
    }

    return nullptr;
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
QSet<qint64> WorkspaceEditModel::effectiveAccessIds() const
{
    QSet<qint64> ids = server_access_ids_;

    ids.unite(added_user_ids_);
    ids.subtract(removed_user_ids_);
    ids.subtract(missing_user_ids_);
    return ids;
}

//--------------------------------------------------------------------------------------------------
QList<WorkspaceEditModel::User> WorkspaceEditModel::memberUsers() const
{
    QList<User> members;
    for (qint64 user_id : effectiveAccessIds())
    {
        if (const User* known = knownUser(user_id))
        {
            members.append(*known);
        }
        else
        {
            User& member = members.emplaceBack();
            member.entry_id = user_id;
        }
    }
    return members;
}

//--------------------------------------------------------------------------------------------------
QList<WorkspaceEditModel::User> WorkspaceEditModel::availableUsers() const
{
    const QSet<qint64> access_ids = effectiveAccessIds();

    QList<User> available;
    for (const User& user : page_users_)
    {
        if (!access_ids.contains(user.entry_id))
            available.append(user);
    }
    return available;
}

//--------------------------------------------------------------------------------------------------
QList<qint64> WorkspaceEditModel::accessUserIdsForSave() const
{
    const QSet<qint64> access_ids = effectiveAccessIds();
    return QList<qint64>(access_ids.begin(), access_ids.end());
}
