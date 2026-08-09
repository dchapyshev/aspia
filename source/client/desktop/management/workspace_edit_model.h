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

#ifndef CLIENT_DESKTOP_MANAGEMENT_WORKSPACE_EDIT_MODEL_H
#define CLIENT_DESKTOP_MANAGEMENT_WORKSPACE_EDIT_MODEL_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

// The complete edit state of a workspace dialog, with no UI and no networking - so every race
// the dialog is exposed to is unit-testable. The state is the server-side snapshots (refreshed
// on every refetch) plus the explicit edits of the operator on top of them; what is shown and
// saved is computed from both, so a refetch never merges anything: the edits survive it by
// construction, and a change made from another console is never silently overwritten - the
// save carries baseRevision() and the router rejects it with "conflict" when the snapshot went
// stale.
//
// Snapshots are written only by the apply*() calls fed from server replies; assembling a save
// request must not mutate the model (see the dialog-snapshot rule) - a failed save retries
// against the same state.
class WorkspaceEditModel
{
public:
    struct User
    {
        qint64 entry_id = 0;
        QString name;
    };

    struct WorkspaceInfo
    {
        qint64 entry_id  = 0;
        qint64 revision  = 0;
        QString name;
        QString comment;
        QSet<qint64> access_ids;
    };

    // entry_id == 0 means create mode; > 0 means modify mode.
    explicit WorkspaceEditModel(qint64 entry_id);

    qint64 entryId() const { return entry_id_; }
    bool isModifyMode() const { return entry_id_ > 0; }

    //----------------------------------------------------------------------------------------------
    // Server snapshots
    //----------------------------------------------------------------------------------------------

    // The workspace list reply. Picks the edited workspace out of it and collects the names of
    // the others for the uniqueness check. Returns false in modify mode when the edited
    // workspace is absent from the list: it was deleted from another console and the dialog
    // must close.
    bool applyWorkspaceList(const QList<WorkspaceInfo>& workspaces);

    // One page of the user list, the candidates the operator picks from.
    void applyUserPage(const QList<User>& users);

    // The name of one member, looked up one record at a time: the membership of a workspace is
    // shown whole, and the pages of the user list do not carry every member of it.
    void applyMemberUser(const User& user);

    // The record of a member is gone. Its entry is dropped, because the router refuses an access
    // entry for a user it cannot find.
    void applyMissingUser(qint64 user_id);

    bool isLoaded() const;

    // The members whose name is not known yet, for the lookups the dialog issues.
    QList<qint64> unresolvedMemberIds() const;

    // Snapshot values for the form fields.
    const QString& serverName() const { return server_name_; }
    const QString& serverComment() const { return server_comment_; }
    const QStringList& otherNames() const { return other_names_; }

    //----------------------------------------------------------------------------------------------
    // Operator intents
    //----------------------------------------------------------------------------------------------

    void grantUser(qint64 user_id);
    void revokeUser(qint64 user_id);

    //----------------------------------------------------------------------------------------------
    // Derived state
    //----------------------------------------------------------------------------------------------

    // The server snapshot plus the edits of the operator. A user deleted meanwhile is dropped:
    // the router would reject an entry for a user it cannot find.
    QSet<qint64> effectiveAccessIds() const;

    // The membership of the workspace, whole. A member whose name has not arrived yet is listed
    // by its id alone.
    QList<User> memberUsers() const;

    // The candidates of the current page: the users of it that are not members.
    QList<User> availableUsers() const;

    //----------------------------------------------------------------------------------------------
    // Save
    //----------------------------------------------------------------------------------------------

    // The complete membership the workspace is to have.
    QList<qint64> accessUserIdsForSave() const;

    // The revision the edit is based on. A save built on a stale one is rejected by the router
    // with "conflict", so a change made from another console is never silently overwritten.
    qint64 baseRevision() const { return base_revision_; }

private:
    // The record of a user the dialog has seen, from a lookup or from the current page.
    const User* knownUser(qint64 user_id) const;

    const qint64 entry_id_;

    QHash<qint64, User> member_users_; // Names of the members, one lookup each.
    QList<User> page_users_;           // The page of candidates the dialog shows.
    QSet<qint64> server_access_ids_;
    QSet<qint64> added_user_ids_;
    QSet<qint64> removed_user_ids_;
    QSet<qint64> missing_user_ids_;

    QString server_name_;
    QString server_comment_;
    QStringList other_names_;

    qint64 base_revision_ = 0;

    bool workspaces_loaded_ = false;
    bool users_loaded_ = false;
};

#endif // CLIENT_DESKTOP_MANAGEMENT_WORKSPACE_EDIT_MODEL_H
