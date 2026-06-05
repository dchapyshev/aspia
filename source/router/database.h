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

#include <set>
#include <string_view>
#include <unordered_map>

#include "base/peer/host_id.h"
#include "base/peer/router_user.h"
#include "base/sql/sql_database.h"
#include "router/workspace.h"

namespace proto::router {
class GroupList;
class Host;
class HostList;
class HostSearchResult;
class WorkspaceList;
} // namespace proto::router

// A node in a workspace's host-group tree. Root nodes have parent_id == 0. Names are not
// required to be unique within a parent: two siblings with the same name are allowed and the
// client disambiguates by entry_id. Cycle protection on re-parent walks parent links upward
// from the proposed new parent and refuses the move if it reaches the node being moved.
struct Group
{
    qint64 entry_id  = 0;
    qint64 parent_id = 0; // 0 means the group sits at the workspace root.
    QString name;
    QByteArray comment;   // AEAD-encrypted with the workspace GK.
};

// Metadata for a client device token returned to admin callers. Intentionally omits the token
// hash and any other material that could identify the token outside the router.
struct DeviceToken
{
    qint64 token_id     = 0; // client_device_tokens.token_id. Opaque to admins.
    qint64 created_at   = 0; // Unix seconds.
    qint64 last_used_at = 0; // Unix seconds.
    QString address;         // Address of the session at last use (or issue, if untouched).
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

    //----------------------------------------------------------------------------------------------
    // Users
    //----------------------------------------------------------------------------------------------

    QList<RouterUser> userList() const;
    bool addUser(const RouterUser& user);
    bool modifyUser(const RouterUser& user);
    bool removeUser(qint64 entry_id);
    RouterUser findUser(const QString& username) const;
    RouterUser findUser(qint64 entry_id) const;

    //----------------------------------------------------------------------------------------------
    // TOTP per-user state
    //----------------------------------------------------------------------------------------------

    // Commits the user's TOTP enrollment: writes the (already-encrypted) shared secret and
    // seeds otp_counter with the step that produced the confirmation code. A non-empty secret
    // column is what activates 2FA for the user. Replaces any pre-existing value (used by the
    // admin "reset TOTP" action followed by a fresh self-enrollment).
    bool setUserOtp(qint64 user_id, const QByteArray& encrypted_secret, quint64 counter);

    // Resets the user's TOTP state - clears the secret and zeroes the replay counter. The
    // next login triggers self-enrollment.
    bool clearUserOtp(qint64 user_id);

    // Stores the most recently consumed TOTP step. Must be called after every successful
    // verification so subsequent submissions of the same code are rejected.
    bool updateUserOtpCounter(qint64 user_id, quint64 counter);

    //----------------------------------------------------------------------------------------------
    // Client device tokens (bearer "remember this device" credentials issued during client sessions)
    //----------------------------------------------------------------------------------------------

    // Issues a new client device token for |user_id|. |address| is stored as the address of
    // the session that requested the issue (shown to admins). On success writes the freshly
    // generated opaque token into |token| and, when not null, the router-side row id into
    // |token_id|. Returns false on database error.
    bool issueClientDeviceToken(
        qint64 user_id, const QString& address, QByteArray* token, qint64* token_id = nullptr);

    // Looks up a token by its opaque value. On success writes the owner user_id into |user_id|
    // and, when not null, the router-side row id into |token_id|. Returns false if the row is
    // absent.
    bool findClientDeviceToken(
        const QByteArray& token, qint64* user_id, qint64* token_id = nullptr) const;

    // Updates the token's last_used_at timestamp and last seen |address|. Called after a
    // successful token lookup.
    bool touchClientDeviceToken(const QByteArray& token, const QString& address);

    // Removes a single token by its router-side row id, but only if it belongs to
    // |user_id|. The user_id check is defense in depth - the admin channel is already
    // privileged, but the extra predicate prevents a malformed request from touching another
    // user's row. Returns false on database error or when no row matched.
    bool revokeClientDeviceToken(qint64 user_id, qint64 token_id);

    // Removes every device token of |user_id| in a single statement. Returns false on database
    // error.
    bool revokeUserClientDeviceTokens(qint64 user_id);

    // Lists all device tokens owned by |user_id|. The router never exposes token material to
    // admins - only the opaque numeric id and timestamp metadata. Returns an empty list on
    // error or when the user has none.
    QList<DeviceToken> listClientDeviceTokens(qint64 user_id) const;

    //----------------------------------------------------------------------------------------------
    // Hosts
    //----------------------------------------------------------------------------------------------

    // Identity and telemetry. The hosts table is the durable identity table; rows are
    // never deleted by cascade.
    std::string_view hostId(std::string_view key_hash, HostId* host_id) const;
    bool addHost(std::string_view key_hash);

    // Called on every host connection to refresh the host's last-seen metadata.
    bool updateHostInfo(HostId host_id, std::string_view computer_name, std::string_view cpu_arch,
        const QString& version, std::string_view os_name, const QString& address);

    // Returns the workspace_id of the given host, or 0 if the host is not assigned to a
    // workspace. Returns -1 if the host_id is unknown. Used to validate user access before
    // edits.
    qint64 hostWorkspaceId(HostId host_id) const;

    // Updates the admin/manager-editable fields of a host (display_name plain, the other three
    // AEAD-encrypted with the workspace GK by the caller). group_id == 0 places the host at the
    // workspace root; > 0 moves it under the given group (caller must validate group ownership).
    // Also bumps last_modify.
    bool modifyHost(HostId host_id, qint64 group_id, std::string_view display_name,
        std::string_view comment, std::string_view user_name, std::string_view password);

    // Appends every host in the database (admin-only call site) to |out| and sets its error_code,
    // reading rows straight into the protobuf message. [start_item, end_item] gives an inclusive
    // paging window; pass end_item <= 0 to disable paging. The Host.online field is left unset - it
    // reflects runtime session state the database does not track and is filled by the caller.
    void hosts(qint64 start_item, qint64 end_item, proto::router::HostList* out) const;

    // Appends hosts in the given workspace and group (exact match on both columns) to |out| and
    // sets its error_code. [start_item, end_item] gives an inclusive paging window; pass
    // end_item <= 0 to disable.
    void hosts(qint64 workspace_id, qint64 group_id, qint64 start_item, qint64 end_item,
        proto::router::HostList* out) const;

    // Total host count in the same scope as the matching hosts() overload. Used by the client
    // to drive pagination UI without fetching the full list.
    qint64 hostCount() const;
    qint64 hostCount(qint64 workspace_id, qint64 group_id) const;

    // Substring search over |display_name| (case-insensitive) and the decimal host_id, restricted
    // to the given workspaces. |workspace_ids| must already be the set the user is allowed to see;
    // an empty list yields no results. Matches are appended to |out| and its error_code is set;
    // Host.online is left unset.
    void searchHosts(const QString& query, const std::set<qint64>& workspace_ids,
        proto::router::HostSearchResult* out) const;

    // Host removal: hosts_remove queue. Schedule moves the row from hosts to hosts_remove, the
    // host_id is then kept until the host process acknowledges the removal command.
    bool scheduleHostRemoval(HostId host_id);
    bool hasPendingHostRemoval(HostId host_id) const;
    bool finalizeHostRemoval(HostId host_id);

    //----------------------------------------------------------------------------------------------
    // Workspaces
    //----------------------------------------------------------------------------------------------

    // Appends the workspaces visible to |user_id| (those it has an access entry for) to |out|,
    // reading rows straight into the protobuf message. workspace_id == 0 returns all visible
    // workspaces; > 0 narrows to that single one. Both set |out|'s error_code.
    //
    // WithAllAccess attaches every member's access entry to each workspace (for admin sessions
    // managing membership) and fetches them in a single query (no per-workspace round trip).
    // WithOwnAccess attaches only |user_id|'s own access entry (the wrapped_gk it needs to open
    // the workspace GK), which the visibility join already yields - so it runs a single query.
    void workspaceListWithAllAccess(qint64 user_id, qint64 workspace_id,
        proto::router::WorkspaceList* out) const;
    void workspaceListWithOwnAccess(qint64 user_id, qint64 workspace_id,
        proto::router::WorkspaceList* out) const;
    Workspace findWorkspace(qint64 entry_id) const;

    // Initial access list may be empty - in that case the workspace has no GK yet; it will
    // be generated when the first access is granted via modifyWorkspace(). The comment is
    // stored as opaque bytes (AEAD-encrypted with the workspace GK on the client).
    std::string_view addWorkspace(std::string_view name, std::string_view comment,
        const QList<Workspace::Access>& initial_access, qint64* entry_id);

    // Updates name/comment and synchronizes access in a single transaction. desired_access is
    // the complete final access list: user_ids missing from it are revoked, user_ids absent
    // from the current DB record are inserted with the supplied wrapped_gk, and user_ids
    // already present preserve their existing wrapped_gk (the value in desired_access is
    // ignored).
    std::string_view modifyWorkspace(qint64 entry_id, std::string_view name, std::string_view comment,
        const QList<Workspace::Access>& desired_access);
    std::string_view removeWorkspace(qint64 entry_id);

    // Assigns hosts to the given workspace. desired_host_ids is the complete final set: hosts
    // currently in this workspace but absent from the set are released (workspace_id <- 0);
    // hosts in the set with workspace_id 0 are claimed (workspace_id <- entry_id); hosts in
    // the set that already belong to another workspace are left alone (the operator cannot
    // hijack a host from another workspace through this call).
    std::string_view setWorkspaceHosts(qint64 entry_id, const std::set<HostId>& desired_host_ids);

    // Returns the set of workspace ids the given user has a workspace_access entry for.
    std::set<qint64> workspaceAccessIdsForUser(qint64 user_id) const;

    // Returns {workspace_id, wrapped_gk} for every workspace the user has access to.
    QList<Workspace::Access> workspaceAccessListForUser(qint64 user_id) const;

    bool hasWorkspaceAccess(qint64 user_id, qint64 workspace_id) const;

    // Reconciles the user's workspace_access rows after a key-pair rotation. Each existing
    // membership is updated with the matching re-sealed wrapped_gk from |wrapped_keys| (keyed by
    // workspace id); memberships without an entry are removed, since their stored wrapped_gk no
    // longer opens with the new key pair. Does not grant access to workspaces the user was not
    // already a member of.
    bool setWorkspaceKeysForUser(qint64 user_id, const std::unordered_map<qint64, QByteArray>& wrapped_keys);

    //----------------------------------------------------------------------------------------------
    // Hosts Groups
    //----------------------------------------------------------------------------------------------

    // Appends every group in the given workspace (unspecified order) to |out| and sets its
    // error_code, reading rows straight into the protobuf message. The client builds a tree by
    // indexing on entry_id and linking via parent_id; display ordering is the client's job.
    void groupList(qint64 workspace_id, proto::router::GroupList* out) const;

    // Returns direct children of parent_id within workspace_id. parent_id == 0 returns root
    // groups (parent_id IS NULL in the table).
    QList<Group> groupChildren(qint64 workspace_id, qint64 parent_id) const;

    // Returns the group with the given entry_id from workspace_id. Returns an empty Group
    // (entry_id == 0) if no such row exists in this workspace.
    Group findGroup(qint64 workspace_id, qint64 entry_id) const;

    // Inserts a new group. parent_id == 0 places it at the workspace root; otherwise parent_id
    // must reference an existing group within the same workspace. On success *entry_id is set
    // to the new id.
    std::string_view addGroup(qint64 workspace_id, qint64 parent_id, std::string_view name,
        std::string_view comment, qint64* entry_id);

    // Renames and/or re-parents a group. new_parent_id must point to a group in the same
    // workspace and must not be the group itself or one of its descendants. The cycle check
    // runs a recursive CTE that walks parent links upward from new_parent_id; if entry_id
    // appears anywhere in that chain the move is refused.
    std::string_view modifyGroup(qint64 workspace_id, qint64 entry_id, qint64 new_parent_id,
        std::string_view name, std::string_view comment);

    // Deletes the group and all its descendants.
    std::string_view removeGroup(qint64 workspace_id, qint64 entry_id);

private:
    Database() = default;

    bool openDatabase();

    mutable SqlDatabase db_;
    bool valid_ = false;

    Q_DISABLE_COPY_MOVE(Database)
};

#endif // ROUTER_DATABASE_H
