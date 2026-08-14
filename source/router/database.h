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
    qint64 revision  = 0;
    qint64 entry_id  = 0;
    qint64 parent_id = 0; // 0 means the group sits at the workspace root.
    std::string name;
    std::string comment;
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

// Invariants the write paths maintain, each enforced inside the mutating transaction. A change
// to any user/workspace path must be checked against this list.
// I1. users.sessions is written only by the INSERT of addUser: the access level of a user never
//     changes after creation.
// I2. An administrator sees every workspace by its session type alone (workspaceListForAdmin),
//     so workspace_access holds the memberships of the regular users only.
// I3. A workspace modification applies only on top of the state the client saw
//     (workspaces.revision): a save built from a stale snapshot is rejected with kErrorConflict,
//     never applied over the concurrent change. Every change of the membership of a workspace
//     moves its revision: modifyWorkspace itself and the rows removed by the cascade of
//     removeUser.
class Database
{
public:
    // A default-constructed Database is not connected; open() (or instance(), which opens the
    // per-thread connection itself) must be called first.
    Database() = default;
    ~Database() = default;

    // Returns the per-thread cached Database. First call on a given thread opens the
    // connection and ensures the schema; subsequent calls are O(1). Connection lives until
    // the thread exits.
    static Database& instance();

    static QString filePath();

    // Opens (creating when absent) the database at |file_path| and ensures the schema.
    // instance() opens the per-thread connection at filePath(); this entry exists so the tests
    // can run every path against an isolated temporary database.
    bool open(const QString& file_path);

    bool isValid() const;

    //----------------------------------------------------------------------------------------------
    // Users
    //----------------------------------------------------------------------------------------------

    // Fills |users| with the requested page of the user list, ordered by entry id. The page is
    // mandatory, so a zero count is refused along with a negative offset or a count over the cap.
    // A database error yields an empty list - a partial one never passes for a complete page.
    // Returns a proto::router error code.
    std::string_view userList(qint64 offset, qint64 count, std::vector<RouterUser>* users) const;

    // Reads the total user count into |count|, for the pagination of the caller. Returns
    // kErrorInternalError when the answer could not be read - a zero count from a failed query
    // would truncate the pagination.
    std::string_view userCount(qint64* count) const;

    // Adds a user record. Returns a proto::router error code.
    std::string_view addUser(const RouterUser& user);

    // Updates a user record. The access level is set at the creation of the user and never changes
    // afterwards, so the session mask of |user| is ignored. A request with empty salt/verifier
    // changes only the flags: the stored credentials are kept, so a snapshot taken before a
    // concurrent password change cannot roll that change back. If the change rotates the key
    // material (salt, verifier or public key differ from the stored ones), every device token of
    // the user is revoked. If |password_changed| is not null it is set to whether the rotation
    // happened (the authoritative check against the stored record).
    // Returns a proto::router error code.
    std::string_view modifyUser(const RouterUser& user, bool* password_changed = nullptr);

    // Removes a user; its workspace_access rows go with it by cascade, and the revision of every
    // affected workspace is bumped in the same transaction (see I4).
    std::string_view removeUser(qint64 entry_id);

    // Reads a user record into |user|. Returns kErrorNotFound when no such user exists and
    // kErrorInternalError when the answer could not be read: a failed read must not pass for a
    // missing record, so the two are separate answers and neither can be dropped by accident.
    std::string_view findUser(const QString& username, RouterUser* user) const;
    std::string_view findUser(qint64 entry_id, RouterUser* user) const;

    //----------------------------------------------------------------------------------------------
    // TOTP per-user state
    //----------------------------------------------------------------------------------------------

    // Commits the user's TOTP enrollment: writes the shared secret and seeds otp_counter with
    // the step that produced the confirmation code. A non-empty secret column is what activates
    // 2FA for the user. Succeeds only while the user still has no confirmed secret, so parallel
    // enrollment attempts cannot overwrite each other.
    bool setUserOtp(qint64 user_id, const QByteArray& secret, quint64 counter);

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
    std::string_view revokeClientDeviceTokens(qint64 user_id, const std::vector<qint64>& token_ids);

    // Removes every device token of |user_id| in a single statement. Succeeds for an existing user
    // with no tokens, but returns kErrorNotFound if the user row is absent.
    std::string_view revokeUserClientDeviceTokens(qint64 user_id);

    // Fills |tokens| with all device tokens owned by |user_id|. The router never exposes token
    // material to admins - only the opaque numeric id and timestamp metadata. Returns false on
    // a database error.
    bool listClientDeviceTokens(qint64 user_id, std::vector<DeviceToken>* tokens) const;

    //----------------------------------------------------------------------------------------------
    // Hosts
    //----------------------------------------------------------------------------------------------

    // Identity and telemetry. The hosts table is the durable identity table; rows are
    // never deleted by cascade. A row is created at approval time with the key hash and the
    // hardware id reported by the host; both are mandatory.
    std::string_view hostId(std::string_view key_hash, HostId* host_id) const;
    bool addHost(std::string_view key_hash, std::string_view hwid);

    // Called on every host connection to refresh the host's last-seen metadata.
    bool updateHostInfo(HostId host_id, std::string_view hwid, std::string_view computer_name,
        std::string_view cpu_arch, std::string_view version, std::string_view os_name,
        std::string_view address);

    // Reads the workspace of the given host into |workspace_id| (0 - the host is assigned to no
    // workspace). Returns kErrorNotFound for an unknown host and kErrorInternalError when the
    // answer could not be read. Used to validate user access before edits.
    std::string_view hostWorkspaceId(HostId host_id, qint64* workspace_id) const;

    // Updates the admin/manager-editable fields of a host. workspace_id is the workspace the host
    // ends up in: 0 releases it, and the group and the note it carried within the workspace go
    // with it. A host another workspace holds is refused with kErrorConflict (the caller acted on
    // a stale snapshot), as is a move into a workspace that is gone. group_id == 0 places the host
    // at the workspace root; > 0 moves it under the given group (caller must validate group
    // ownership). Also bumps last_modify. |base_revision| is the revision the edit was built on:
    // a stale one answers kErrorConflict instead of silently overwriting a concurrent edit.
    // Returns a proto::router error code.
    std::string_view modifyHost(HostId host_id, qint64 base_revision, qint64 workspace_id,
        qint64 group_id, std::string_view display_name, std::string_view comment);

    // Appends every host in the database (admin-only call site) to |out| and sets its error_code,
    // reading rows straight into the protobuf message. |offset| and |count| give the requested
    // page; it is mandatory, so a zero count is refused along with a negative offset or a count
    // over the cap. The Host.online field is left unset - it reflects runtime session state the
    // database does not track and is filled by the caller.
    void hosts(qint64 offset, qint64 count, proto::router::HostList* out) const;

    // Appends hosts in the given workspace and group to |out| and sets its error_code. A negative
    // group_id takes every group of the workspace; otherwise the match on the column is exact.
    // |offset| and |count| give the page; it is mandatory and bounded the same way as in the
    // overload above.
    void hosts(qint64 workspace_id, qint64 group_id, qint64 offset, qint64 count,
        proto::router::HostList* out) const;

    // Reads into |count| the host count in the same scope as the matching hosts() overload. Used
    // by the client to drive pagination UI without fetching the full list. Returns
    // kErrorInternalError when the answer could not be read - a zero count from a failed query
    // would truncate the pagination.
    std::string_view hostCount(qint64* count) const;
    std::string_view hostCount(qint64 workspace_id, qint64 group_id, qint64* count) const;

    // Substring search over |display_name| (case-insensitive) and the decimal host_id, restricted
    // to the given workspaces. |workspace_ids| must already be the set the user is allowed to see;
    // an empty list yields no results. The page is mandatory and bounded exactly like the one of
    // hosts(). Matches are appended to |out|, its total_count is set to the number of matches in
    // the whole scope and its error_code is set; Host.online is left unset.
    void searchHosts(std::string_view query, const std::set<qint64>& workspace_ids,
        qint64 offset, qint64 count, proto::router::HostSearchResult* out) const;

    // Host removal: hosts_remove queue. Schedule moves the row from hosts to hosts_remove, the
    // host_id is then kept until the host process acknowledges the removal command.
    bool scheduleHostRemoval(HostId host_id);
    bool hasPendingHostRemoval(HostId host_id) const;
    bool finalizeHostRemoval(HostId host_id);

    // Drops the queued removals nobody ever acknowledged. A host that never comes back would keep
    // its row - and with it its id - forever; after the grace period the record is gone for good
    // and the same machine has to be approved anew. Returns false only on a database error.
    bool pruneExpiredHostRemovals();

    //----------------------------------------------------------------------------------------------
    // Workspaces
    //----------------------------------------------------------------------------------------------

    // Append workspaces to |out|, reading rows straight into the protobuf message.
    // workspace_id == 0 returns every workspace of the scope; > 0 narrows to that single one.
    // Both set |out|'s error_code.
    //
    // ForAdmin lists every workspace of the router with the membership of each attached (for the
    // admin sessions managing it), fetched in a single query (no per-workspace round trip).
    // ForUser lists the workspaces |user_id| is a member of, without the membership of anybody.
    void workspaceListForAdmin(qint64 workspace_id, proto::router::WorkspaceList* out) const;
    void workspaceListForUser(qint64 user_id, qint64 workspace_id,
        proto::router::WorkspaceList* out) const;
    Workspace findWorkspace(qint64 entry_id) const;

    // The initial access list holds the user_ids of the members of the workspace; an entry for a
    // user that is gone is rejected (kErrorConflict - the sender refetches, see checkAccessUser).
    // The hosts of a workspace are claimed one by one (see modifyHost), so a creation never
    // touches them.
    std::string_view addWorkspace(std::string_view name, std::string_view comment,
        const std::vector<qint64>& initial_access, qint64* entry_id);

    // Updates name/comment and synchronizes the access list in a single transaction.
    // base_revision is the revision the client based its edit on; a mismatch with the stored
    // value is rejected (kErrorConflict) so a save built from a stale snapshot can never silently
    // overwrite a concurrent change, and on success the stored revision is incremented.
    // desired_access is the complete final access list: user_ids missing from it are revoked and
    // user_ids absent from the current DB record are inserted.
    std::string_view modifyWorkspace(qint64 entry_id, qint64 base_revision,
        std::string_view name, std::string_view comment, const std::vector<qint64>& desired_access);

    // Deletes a workspace, releases its hosts (workspace_id <- 0, group_id <- 0) and drops the
    // note each of them carried within it. Deliberately takes no base_revision: a delete is an
    // intent about the whole entity, not about a particular state of it, so it applies
    // regardless of concurrent edits (unlike modifyWorkspace, see I3).
    std::string_view removeWorkspace(qint64 entry_id);

    // Fills |workspace_ids| with the ids of every workspace of the router. Returns false on a
    // database error.
    bool workspaceIds(std::set<qint64>* workspace_ids) const;

    // Fills |workspace_ids| with the ids of every workspace the user has a workspace_access
    // entry for. Returns false on a database error.
    bool workspaceAccessIdsForUser(qint64 user_id, std::set<qint64>* workspace_ids) const;

    // Returns kErrorOk when the user has an access entry for the workspace, kErrorAccessDenied
    // when it has none and kErrorInternalError when the answer could not be read - a caller whose
    // skip-or-reject decision depends on the answer must not mistake an error for "no access".
    std::string_view checkWorkspaceAccess(qint64 user_id, qint64 workspace_id) const;

    //----------------------------------------------------------------------------------------------
    // Hosts Groups
    //----------------------------------------------------------------------------------------------

    // Appends every group in the given workspace (unspecified order) to |out| and sets its
    // error_code, reading rows straight into the protobuf message. The client builds a tree by
    // indexing on entry_id and linking via parent_id; display ordering is the client's job.
    void groupList(qint64 workspace_id, proto::router::GroupList* out) const;

    // Reads the group with the given entry_id from workspace_id into |group|. Returns
    // kErrorNotFound when no such row exists in this workspace (an out of range id included) and
    // kErrorInternalError when the answer could not be read - a database error must not pass for
    // a missing group.
    std::string_view findGroup(qint64 workspace_id, qint64 entry_id, Group* group) const;

    // Inserts a new group. parent_id == 0 places it at the workspace root; otherwise parent_id
    // must reference an existing group within the same workspace. On success *entry_id is set
    // to the new id.
    std::string_view addGroup(qint64 workspace_id, qint64 parent_id, std::string_view name,
        std::string_view comment, qint64* entry_id);

    // Renames and/or re-parents a group. new_parent_id must point to a group in the same
    // workspace and must not be the group itself or one of its descendants. The cycle check
    // runs a recursive CTE that walks parent links upward from new_parent_id; if entry_id
    // appears anywhere in that chain the move is refused. |base_revision| is the revision the
    // edit was built on: a stale one answers kErrorConflict.
    std::string_view modifyGroup(qint64 workspace_id, qint64 entry_id, qint64 base_revision,
        qint64 new_parent_id, std::string_view name, std::string_view comment);

    // Deletes the group and all its descendants, moving hosts from that subtree to the
    // workspace root (group_id <- 0).
    std::string_view removeGroup(qint64 workspace_id, qint64 entry_id);

private:
    bool openDatabase();

    // Checks that a new access entry targets a user that still exists. An unknown user means the
    // record was deleted after the sender took its snapshot, so it answers kErrorConflict and the
    // sender refetches. Must be called inside a transaction. Returns a proto::router error code.
    std::string_view checkAccessUser(qint64 user_id);

    mutable SqlDatabase db_;

    Q_DISABLE_COPY_MOVE(Database)
};

#endif // ROUTER_DATABASE_H
