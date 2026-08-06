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

// Invariants the write paths maintain, each enforced inside the mutating transaction. A change
// to any user/workspace path must be checked against this list.
// I1. users.sessions is written only by the INSERT of addUser: the access level of a user never
//     changes after creation.
// I2. Every administrator with a key pair has a workspace_access row for every workspace
//     (grantMissingWorkspaceAccess on the user paths, checkAccessCoversAdmins on the workspace
//     paths). One deliberate exception: a workspace the grantor of a new administrator cannot
//     see is skipped (nobody can seal its key), leaving a temporary hole that the next save of
//     that workspace closes via checkAccessCoversAdmins.
// I3. Every stored wrapped_gk is sealed to the current public key of its user - the router
//     cannot unseal or reseal, so it verifies the seal target instead (checkAccessSealTarget on
//     the workspace paths, the keys_usable guard in modifyUser) and rejects what the user could
//     never decrypt.
// I4. A workspace modification applies only on top of the state the client saw
//     (workspaces.revision): a save built from a stale snapshot is rejected with kErrorConflict,
//     never applied over the concurrent change. Every change of the membership of a workspace
//     moves its revision: modifyWorkspace itself, the access rows granted by
//     grantMissingWorkspaceAccess and the rows removed by the cascade of removeUser.
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

    // Fills |users| with every user record. Returns false on a database error - a partial list
    // never passes for a complete one.
    bool userList(std::vector<RouterUser>* users) const;

    // Adds a user record. An administrator has access to every workspace, so for an administrator
    // an access entry is created for each existing workspace from |wrapped_keys| (for any other
    // user |wrapped_keys| is ignored); |grantor_user_id| is the user that sent the request (see
    // grantMissingWorkspaceAccess).
    // Returns a proto::router error code.
    std::string_view addUser(
        const RouterUser& user, const std::unordered_map<qint64, QByteArray>& wrapped_keys = {},
        qint64 grantor_user_id = 0);

    // Updates a user record. The access level is set at the creation of the user and never changes
    // afterwards, so the session mask of |user| is ignored. A request with empty salt/verifier
    // changes only the flags: the stored credentials are kept, so a snapshot taken before a
    // concurrent password change cannot roll that change back. If the change rotates the key
    // material (salt, verifier or public key differ from the stored ones), every device token is
    // revoked and the workspace keys are re-wrapped from |wrapped_keys| atomically; |wrapped_keys|
    // must then cover every workspace the user can access or the change is rejected
    // (kErrorConflict - the sender's workspace list was stale) without modifying anything. When
    // the key material is unchanged |wrapped_keys| is not used for the re-wrap, but an
    // administrator is still granted the workspaces they have no access entry for (see
    // grantMissingWorkspaceAccess) - only while the stored public key is not empty and still
    // matches the snapshot in |user| that the keys were sealed to. If |password_changed| is not
    // null it is set to whether the rotation happened (the authoritative check against the
    // stored record).
    // Returns a proto::router error code.
    std::string_view modifyUser(
        const RouterUser& user, const std::unordered_map<qint64, QByteArray>& wrapped_keys = {},
        qint64 grantor_user_id = 0, bool* password_changed = nullptr);

    // Removes a user; its workspace_access rows go with it by cascade, and the revision of every
    // affected workspace is bumped in the same transaction (see I4).
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

    // Returns the workspace_id of the given host, or 0 if the host is not assigned to a
    // workspace. Returns -1 if the host_id is unknown. |ok| (optional) is set to false when the
    // answer could not be determined - a database error must not pass for a missing host.
    // Used to validate user access before edits.
    qint64 hostWorkspaceId(HostId host_id, bool* ok = nullptr) const;

    // Updates the admin/manager-editable fields of a host (display_name plain, the other three
    // AEAD-encrypted with the workspace GK by the caller). group_id == 0 places the host at the
    // workspace root; > 0 moves it under the given group (caller must validate group ownership).
    // Also bumps last_modify.
    bool modifyHost(HostId host_id, qint64 group_id, std::string_view display_name,
        std::string_view comment, std::string_view user_name, std::string_view password);

    // Appends every host in the database (admin-only call site) to |out| and sets its error_code,
    // reading rows straight into the protobuf message. |offset| and |count| give the requested
    // page; it is mandatory, so a zero count is refused along with a negative offset or a count
    // over the cap. The Host.online field is left unset - it reflects runtime session state the
    // database does not track and is filled by the caller.
    void hosts(qint64 offset, qint64 count, proto::router::HostList* out) const;

    // Appends hosts in the given workspace and group (exact match on both columns) to |out| and
    // sets its error_code. |offset| and |count| give the page; it is mandatory and bounded the
    // same way as in the overload above.
    void hosts(qint64 workspace_id, qint64 group_id, qint64 offset, qint64 count,
        proto::router::HostList* out) const;

    // Total host count in the same scope as the matching hosts() overload. Used by the client
    // to drive pagination UI without fetching the full list. |ok| (optional) is set to false on
    // a database error - a zero count from a failed query would truncate the pagination.
    qint64 hostCount(bool* ok = nullptr) const;
    qint64 hostCount(qint64 workspace_id, qint64 group_id, bool* ok = nullptr) const;

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

    // The initial access list must contain every administrator that has a public key: an
    // administrator has access to every workspace, and an incomplete list is rejected
    // rather than completed - the GK only arrives here sealed, so the router cannot create
    // the missing entries itself (see checkAccessCoversAdmins). Every entry must carry the
    // public key its wrapped_gk is sealed to; it is checked against the stored key of the
    // user (see checkAccessSealTarget). The comment is stored as opaque bytes
    // (AEAD-encrypted with the workspace GK on the client). The host assignments are applied
    // in the same transaction (see syncWorkspaceHosts), so on any error the workspace is not
    // created at all.
    std::string_view addWorkspace(std::string_view name, std::string_view comment,
        const std::vector<Workspace::Access>& initial_access, const std::set<HostId>& desired_host_ids,
        qint64* entry_id);

    // Updates name/comment and synchronizes access and host assignments in a single
    // transaction. base_revision is the revision the client based its edit on; a mismatch with
    // the stored value is rejected (kErrorConflict) so a save built from a stale snapshot can
    // never silently overwrite a concurrent change, and on success the stored revision is
    // incremented. desired_access is the complete final access list: user_ids missing from it
    // are revoked, user_ids absent from the current DB record are inserted with the supplied
    // wrapped_gk (after checkAccessSealTarget on their public_key), and user_ids already
    // present preserve their existing wrapped_gk (the value in desired_access is ignored).
    // desired_host_ids is the complete final set of the hosts (see syncWorkspaceHosts).
    std::string_view modifyWorkspace(qint64 entry_id, qint64 base_revision,
        std::string_view name, std::string_view comment,
        const std::vector<Workspace::Access>& desired_access, const std::set<HostId>& desired_host_ids);

    // Deletes a workspace, releases its hosts (workspace_id <- 0, group_id <- 0), and clears
    // host fields encrypted with the workspace GK. Deliberately takes no base_revision: a delete
    // is an intent about the whole entity, not about a particular state of it, so it applies
    // regardless of concurrent edits (unlike modifyWorkspace, see I4).
    std::string_view removeWorkspace(qint64 entry_id);

    // Fills |workspace_ids| with the ids of every workspace the user has a workspace_access
    // entry for. Returns false on a database error.
    bool workspaceAccessIdsForUser(qint64 user_id, std::set<qint64>* workspace_ids) const;

    // Fills |access_list| with {workspace_id, wrapped_gk} for every workspace the user has
    // access to. Returns false on a database error - a partial key set would look to the user
    // like revoked access.
    bool workspaceAccessListForUser(qint64 user_id, std::vector<Workspace::Access>* access_list) const;

    // Returns whether the user has an access entry for the workspace; false also on a database
    // error (fail-closed for the authorization checks). |ok| (optional) is set to false when
    // the answer could not be determined - a caller whose skip-or-reject decision depends on
    // the answer must not mistake an error for "no access".
    bool hasWorkspaceAccess(qint64 user_id, qint64 workspace_id, bool* ok = nullptr) const;

    //----------------------------------------------------------------------------------------------
    // Hosts Groups
    //----------------------------------------------------------------------------------------------

    // Appends every group in the given workspace (unspecified order) to |out| and sets its
    // error_code, reading rows straight into the protobuf message. The client builds a tree by
    // indexing on entry_id and linking via parent_id; display ordering is the client's job.
    void groupList(qint64 workspace_id, proto::router::GroupList* out) const;

    // Returns the group with the given entry_id from workspace_id. Returns an empty Group
    // (entry_id == 0) if no such row exists in this workspace. |ok| (optional) is set to false
    // when the answer could not be determined - a database error must not pass for a missing
    // group.
    Group findGroup(qint64 workspace_id, qint64 entry_id, bool* ok = nullptr) const;

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
    bool openDatabase();

    // Creates access entries for every workspace the user has none for, taking the sealed group
    // key of each from |wrapped_keys|, and bumps the revision of each granted workspace (see I4).
    // A workspace whose key is not there is skipped if |grantor_user_id| (the user that sent the
    // request) has no access to it either - then nobody can seal its key for the user. If the
    // grantor does have access, the request was built from an out of date list of the workspaces
    // and is rejected (kErrorConflict - the sender refetches and retries).
    // Must be called inside a transaction. Returns a proto::router error code.
    std::string_view grantMissingWorkspaceAccess(
        qint64 user_id, qint64 grantor_user_id,
        const std::unordered_map<qint64, QByteArray>& wrapped_keys);

    // Checks that |access_user_ids| contains every administrator: an administrator has access to
    // every workspace. The group key can be sealed for a user only by a client that has it, so an
    // incomplete list is rejected (kErrorConflict - the usual cause is an administrator created
    // after the sender took its user list, and a refetch resolves it) instead of being completed
    // here. Must be called inside a transaction. Returns a proto::router error code.
    std::string_view checkAccessCoversAdmins(const std::set<qint64>& access_user_ids);

    // Checks that a new access entry can be stored: |public_key| (the key the sender sealed the
    // wrapped_gk to) must match the current stored key of the user. A mismatch means the sender
    // sealed to an out of date snapshot - the entry could never be unsealed - and is rejected
    // with kErrorConflict, so the sender refetches the users and reseals. An unknown user is a
    // conflict too (deleted after the snapshot); a user without a key pair is rejected with
    // kErrorInvalidData - nobody can seal for it at all.
    // Must be called inside a transaction. Returns a proto::router error code.
    std::string_view checkAccessSealTarget(qint64 user_id, std::string_view public_key);

    // Assigns hosts to the given workspace. desired_host_ids is the complete final set: hosts
    // currently in this workspace but absent from the set are released (workspace_id <- 0);
    // hosts in the set with workspace_id 0 are claimed (workspace_id <- entry_id). Every
    // requested host must exist and be either unassigned or already in this workspace; a host
    // that is gone or was claimed by another workspace is rejected with kErrorConflict (the
    // sender lost a race and resolves it by refetching), so OK means the final set was applied.
    // Must be called inside a transaction. Returns a proto::router error code.
    std::string_view syncWorkspaceHosts(qint64 entry_id, const std::set<HostId>& desired_host_ids);

    mutable SqlDatabase db_;

    Q_DISABLE_COPY_MOVE(Database)
};

#endif // ROUTER_DATABASE_H
