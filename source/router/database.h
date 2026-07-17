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
#include <vector>

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
    std::string name;
    std::string comment;  // AEAD-encrypted with the workspace GK.
};

// Metadata for a client device token returned to admin callers. Intentionally omits the token
// hash and any other material that could identify the token outside the router.
struct DeviceToken
{
    qint64 token_id     = 0; // client_device_tokens.token_id. Opaque to admins.
    qint64 created_at   = 0; // Unix seconds.
    qint64 last_used_at = 0; // Unix seconds.
    std::string address;     // Address of the session at last use (or issue, if untouched).
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

    // Updates a user record. If the change rotates the password (salt/verifier differ from the
    // stored ones), every device token is revoked and the workspace keys are re-wrapped from
    // |wrapped_keys| atomically; |wrapped_keys| must then cover every workspace the user can
    // access or the change is rejected (kErrorInvalidData) without modifying anything. When the
    // password is unchanged |wrapped_keys| is ignored. If |password_changed| is not null it is set
    // to whether the password was rotated (the authoritative check against the stored record).
    // Returns a proto::router error code.
    std::string_view modifyUser(
        const RouterUser& user, const std::unordered_map<qint64, QByteArray>& wrapped_keys = {},
        bool* password_changed = nullptr);
    std::string_view removeUser(qint64 entry_id);
    RouterUser findUser(const QString& username) const;
    RouterUser findUser(qint64 entry_id) const;

    //----------------------------------------------------------------------------------------------
    // TOTP per-user state
    //----------------------------------------------------------------------------------------------

    // Commits the user's TOTP enrollment: writes the (already-encrypted) shared secret and
    // seeds otp_counter with the step that produced the confirmation code. A non-empty secret
    // column is what activates 2FA for the user. Succeeds only while the user still has no
    // confirmed secret, so parallel enrollment attempts cannot overwrite each other.
    bool setUserOtp(qint64 user_id, const QByteArray& encrypted_secret, quint64 counter);

    // Resets the user's TOTP state - clears the secret and zeroes the replay counter. The
    // next login triggers self-enrollment. Returns kErrorNotFound if the user row is absent.
    std::string_view clearUserOtp(qint64 user_id);

    // Atomically consumes a TOTP step. Succeeds only if |counter| is newer than the value
    // currently stored in the database, so parallel sessions cannot accept the same code.
    bool consumeUserOtpCounter(qint64 user_id, quint64 counter);

    //----------------------------------------------------------------------------------------------
    // Client device tokens (bearer "remember this device" credentials issued during client sessions)
    //----------------------------------------------------------------------------------------------

    // Issues a new client device token for |user_id|. |address| is stored as the address of
    // the session that requested the issue (shown to admins). On success writes the freshly
    // generated opaque token into |token| and, when not null, the router-side row id into
    // |token_id|. Returns false on database error.
    bool issueClientDeviceToken(
        qint64 user_id, std::string_view address, std::string* token, qint64* token_id = nullptr);

    // Looks up a token by its opaque value. On success writes the owner user_id into |user_id|
    // and, when not null, the router-side row id into |token_id|. Returns false if the row is
    // absent.
    bool findClientDeviceToken(std::string_view token, qint64* user_id, qint64* token_id = nullptr) const;

    // Updates the token's last_used_at timestamp and last seen |address|. Called after a
    // successful token lookup.
    bool touchClientDeviceToken(std::string_view token, std::string_view address);

    // Removes the given tokens by their router-side row ids, but only those belonging to
    // |user_id|. The user_id check is defense in depth - the admin channel is already
    // privileged, but the extra predicate prevents a malformed request from touching another
    // user's row. All deletes run in one transaction: if any token id does not match a row the
    // whole batch is rolled back. Returns kErrorNotFound if a token is missing, kErrorInternalError
    // on database failure, kErrorOk when every token was revoked (or the list was empty).
    std::string_view revokeClientDeviceTokens(qint64 user_id, const QList<qint64>& token_ids);

    // Removes every device token of |user_id| in a single statement. Succeeds for an existing user
    // with no tokens, but returns kErrorNotFound if the user row is absent.
    std::string_view revokeUserClientDeviceTokens(qint64 user_id);

    // Lists all device tokens owned by |user_id|. The router never exposes token material to
    // admins - only the opaque numeric id and timestamp metadata. Returns an empty list on
    // error or when the user has none.
    std::vector<DeviceToken> listClientDeviceTokens(qint64 user_id) const;

    //----------------------------------------------------------------------------------------------
    // Hosts
    //----------------------------------------------------------------------------------------------

    // Identity and telemetry. The hosts table is the durable identity table; rows are
    // never deleted by cascade. A row is created at approval time with the key hash and the
    // hardware id reported by the host; both are mandatory.
    std::string_view hostId(std::string_view key_hash, HostId* host_id) const;
    bool addHost(std::string_view key_hash, std::string_view hwid);

    // Called on every host connection to refresh the host's last-seen metadata.
    bool updateHostInfo(HostId host_id, std::string_view computer_name, std::string_view cpu_arch,
        const QString& version, std::string_view os_name, std::string_view address);

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
    // paging window; leaving both endpoints at 0 requests the unpaged mode. Any other range
    // must be valid and bounded. The Host.online field is left unset - it reflects runtime session
    // state the database does not track and is filled by the caller.
    void hosts(qint64 start_item, qint64 end_item, proto::router::HostList* out) const;

    // Appends hosts in the given workspace and group (exact match on both columns) to |out| and
    // sets its error_code. [start_item, end_item] gives an inclusive paging window; leaving both
    // endpoints at 0 requests the unpaged mode. Any other range must be valid and bounded.
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

    // Deletes a workspace, releases its hosts (workspace_id <- 0, group_id <- 0), and clears
    // host fields encrypted with the workspace GK.
    std::string_view removeWorkspace(qint64 entry_id);

    // Assigns hosts to the given workspace. desired_host_ids is the complete final set: hosts
    // currently in this workspace but absent from the set are released (workspace_id <- 0);
    // hosts in the set with workspace_id 0 are claimed (workspace_id <- entry_id). Every
    // requested host must exist and be either unassigned or already in this workspace; hosts
    // from another workspace are rejected so OK means the final set was applied.
    std::string_view setWorkspaceHosts(qint64 entry_id, const std::set<HostId>& desired_host_ids);

    // Returns the set of workspace ids the given user has a workspace_access entry for.
    std::set<qint64> workspaceAccessIdsForUser(qint64 user_id) const;

    // Returns {workspace_id, wrapped_gk} for every workspace the user has access to.
    std::vector<Workspace::Access> workspaceAccessListForUser(qint64 user_id) const;

    bool hasWorkspaceAccess(qint64 user_id, qint64 workspace_id) const;

    //----------------------------------------------------------------------------------------------
    // Hosts Groups
    //----------------------------------------------------------------------------------------------

    // Appends every group in the given workspace (unspecified order) to |out| and sets its
    // error_code, reading rows straight into the protobuf message. The client builds a tree by
    // indexing on entry_id and linking via parent_id; display ordering is the client's job.
    void groupList(qint64 workspace_id, proto::router::GroupList* out) const;

    // Returns direct children of parent_id within workspace_id. parent_id == 0 returns root
    // groups (parent_id IS NULL in the table).
    std::vector<Group> groupChildren(qint64 workspace_id, qint64 parent_id) const;

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

    // Deletes the group and all its descendants, moving hosts from that subtree to the
    // workspace root (group_id <- 0).
    std::string_view removeGroup(qint64 workspace_id, qint64 entry_id);

private:
    Database() = default;

    bool openDatabase();

    mutable SqlDatabase db_;

    Q_DISABLE_COPY_MOVE(Database)
};

#endif // ROUTER_DATABASE_H
