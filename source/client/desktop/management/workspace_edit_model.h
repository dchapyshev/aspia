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

    struct HostInfo
    {
        quint64 host_id      = 0;
        qint64 workspace_id  = 0; // 0 - unassigned.
        QString computer_name;
    };

    struct Host
    {
        quint64 host_id = 0;
        QString computer_name;
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
    //
    // The revision that arrives here is only parked: it is committed by applyHostList() of the
    // same refetch cycle (see baseRevision()).
    bool applyWorkspaceList(const QList<WorkspaceInfo>& workspaces);

    void applyUserList(const QList<User>& users);

    // The host list reply. Keeps only the hosts this dialog may manage - the unassigned ones
    // and the hosts of the edited workspace - and commits the revision parked by the paired
    // applyWorkspaceList() call.
    void applyHostList(const QList<HostInfo>& hosts);

    bool isLoaded() const;

    // Snapshot values for the form fields.
    const QString& serverName() const { return server_name_; }
    const QString& serverComment() const { return server_comment_; }
    const QStringList& otherNames() const { return other_names_; }

    //----------------------------------------------------------------------------------------------
    // Operator intents
    //----------------------------------------------------------------------------------------------

    void grantUser(qint64 user_id);
    void revokeUser(qint64 user_id);
    void claimHost(quint64 host_id);
    void releaseHost(quint64 host_id);

    //----------------------------------------------------------------------------------------------
    // Derived state
    //----------------------------------------------------------------------------------------------

    // The server snapshot plus the edits of the operator. Administrators with a key pair are
    // always included: the router accepts a workspace only with every one of them present. A
    // user deleted meanwhile is dropped: the router would reject an entry for a user it cannot
    // find.
    QSet<qint64> effectiveAccessIds() const;

    // Same model. A host that disappeared (deleted, or claimed by another workspace meanwhile)
    // is dropped: it is not ours to keep or to release.
    QSet<quint64> effectiveHostIds() const;

    // Users partitioned for display. availableUsers() excludes users without a key pair:
    // nobody can seal the group key for them, the router would reject the grant.
    QList<User> memberUsers() const;
    QList<User> availableUsers() const;
    QList<Host> hostsInWorkspace() const;
    QList<Host> availableHosts() const;

    // An administrator with a key pair cannot be revoked (the router requires every one of
    // them in the access list).
    // Whether the release warning applies: releasing a host the server has in this workspace
    // irreversibly drops the note it carried within it.
    bool isServerHost(quint64 host_id) const { return server_host_ids_.contains(host_id); }

    //----------------------------------------------------------------------------------------------
    // Save
    //----------------------------------------------------------------------------------------------

    // The complete membership the workspace is to have.
    QList<qint64> accessUserIdsForSave() const;

    QList<quint64> hostIdsForSave() const;

    // The revision the edit is based on. Committed only together with the host snapshot of the
    // same refetch cycle: the reply order on the channel guarantees the workspace reply comes
    // first, so a save issued between the two replies pairs the OLD revision with the OLD host
    // snapshot - and a stale revision is rejected by the router with "conflict". Without the
    // pairing that save would combine a fresh revision with a stale host set and silently
    // release a host granted from another console.
    qint64 baseRevision() const { return base_revision_; }

private:
    const qint64 entry_id_;

    QHash<qint64, User> users_;
    QSet<qint64> server_access_ids_;
    QSet<qint64> added_user_ids_;
    QSet<qint64> removed_user_ids_;

    QHash<quint64, Host> hosts_; // Unassigned hosts and the hosts of this workspace.
    QSet<quint64> server_host_ids_;
    QSet<quint64> added_host_ids_;
    QSet<quint64> removed_host_ids_;

    QString server_name_;
    QString server_comment_;
    QStringList other_names_;

    qint64 pending_revision_ = 0;
    qint64 base_revision_ = 0;

    bool workspaces_loaded_ = false;
    bool users_loaded_ = false;
    bool hosts_loaded_ = false;
};

#endif // CLIENT_DESKTOP_MANAGEMENT_WORKSPACE_EDIT_MODEL_H
