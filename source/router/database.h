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
class Host;
class HostFilter;
class HostList;
class HostSearchResult;
class WorkspaceList;
} // namespace proto::router

// Invariants the write paths maintain, each enforced inside the mutating transaction. A change
// to any user/workspace path must be checked against this list.
// I1. users.sessions always holds the full session mask of the access level
//     (RouterUser::expandSessionTypes): addUser and modifyUser expand the single level a client
//     names, and a modify request without a level keeps the stored mask.
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

    // Updates a user record. Empty salt/verifier keep the stored credentials, a zero session mask
    // keeps the stored level, an empty address keeps the stored one. A password rotation revokes
    // every device token of the user. |password_changed| and |sessions_changed|, if not null,
    // report whether the rotation and the level change happened. Returns a proto::router error code.
    std::string_view modifyUser(const RouterUser& user, bool* password_changed = nullptr,
                                bool* sessions_changed = nullptr);

    // Removes a user; its workspace_access rows go with it by cascade, and the revision of every
    // affected workspace is bumped in the same transaction (see I3).
    std::string_view removeUser(qint64 entry_id);

    // Reads a user record into |user|. Returns kErrorNotFound when no such user exists and
    // kErrorInternalError when the answer could not be read: a failed read must not pass for a
    // missing record, so the two are separate answers and neither can be dropped by accident.
    std::string_view findUser(const QString& username, RouterUser* user) const;
    std::string_view findUser(qint64 entry_id, RouterUser* user) const;
    std::string_view findUserByGuid(const QString& guid, RouterUser* user) const;

    //----------------------------------------------------------------------------------------------
    // TOTP per-user state
    //----------------------------------------------------------------------------------------------

    // Commits the user's TOTP enrollment: writes the shared secret and seeds otp_counter with
    // the step that produced the confirmation code. A non-empty secret column is what activates
    // 2FA for the user. Succeeds only while the user still has no confirmed secret, so parallel
    // enrollment attempts cannot overwrite each other.
    bool setUserOtp(qint64 user_id, const QByteArray& secret, quint64 counter);

    // Resets the user's TOTP state: clears the secret, zeroes the replay counter and drops every
    // device token of the user, all in one transaction. The tokens were issued against the secret
    // being cleared, so a state where they outlive it would let a device in without a code. The
    // next login triggers self-enrollment. Returns kErrorNotFound if the user row is absent.
    std::string_view resetUserOtp(qint64 user_id);

    // Atomically consumes a TOTP step. kErrorOk only if |counter| is newer than the value
    // currently stored in the database, so parallel sessions cannot accept the same code.
    // kErrorNotFound when no row matched: the step was consumed meanwhile or the user is gone.
    // kErrorInternalError when the database refused the write.
    std::string_view consumeUserOtpCounter(qint64 user_id, quint64 counter);

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

    // Replaces the telemetry the host reported last.
    bool updateHostTelemetry(HostId host_id, std::string_view telemetry);

    // Reads the telemetry the host reported last. Returns kErrorNotFound for an unknown host and
    // kErrorInternalError when the read failed.
    std::string_view hostTelemetry(HostId host_id, std::string* telemetry) const;

    // Answers whether the host is in the database: kErrorOk - it is, kErrorNotFound - it is not,
    // kErrorInternalError - the read failed. A failed read must not pass for a missing host,
    // because on kErrorNotFound the client drops the credentials it keeps for the host.
    std::string_view checkHostEntry(HostId host_id) const;

    // Reads the workspace of the given host into |workspace_id| (0 - the host is assigned to no
    // workspace). Returns kErrorNotFound for an unknown host and kErrorInternalError when the
    // answer could not be read. Used to validate user access before edits.
    std::string_view hostWorkspaceId(HostId host_id, qint64* workspace_id) const;

    // Updates the admin/manager-editable fields of a host. workspace_id is the workspace the host
    // ends up in: 0 releases it, and the group it carried within the workspace goes with it; the
    // note is the host's own and is stored as given whether the host is in a workspace or not.
    // A host another workspace holds is refused with kErrorConflict (the caller acted on
    // a stale snapshot), as is a move into a workspace that is gone. group_id == 0 places the host
    // at the workspace root; > 0 moves it under the given group (caller must validate group
    // ownership). Also bumps last_modify. |base_revision| is the revision the edit was built on:
    // a stale one answers kErrorConflict instead of silently overwriting a concurrent edit.
    // Returns a proto::router error code.
    std::string_view modifyHost(HostId host_id, qint64 base_revision, qint64 workspace_id,
        qint64 group_id, std::string_view display_name, std::string_view comment);

    // Appends every host in the database (admin-only call site) that passes |filter| to |out| and
    // sets its error_code, reading rows straight into the protobuf message. A filter with more
    // workspaces than kMaxHostFilterWorkspaces or with a negative one is refused.
    // |offset| and |count| give the requested page; it is mandatory, so a zero count is refused
    // along with a negative offset or a count over the cap. The Host.online field is left unset,
    // because it reflects runtime session state the database does not track. The caller fills it.
    void hosts(const proto::router::HostFilter& filter, qint64 offset, qint64 count,
        proto::router::HostList* out) const;

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
    std::string_view hostCount(const proto::router::HostFilter& filter, qint64* count) const;
    std::string_view hostCount(qint64 workspace_id, qint64 group_id, qint64* count) const;

    // Substring search over |display_name|, |comment| (case-insensitive) and the decimal host_id,
    // restricted to the given workspaces. |workspace_ids| must already be the set the user is
    // allowed to see; an empty list yields no results. The page is mandatory and bounded exactly
    // like the one of hosts(). Matches are appended to |out|, its total_count is set to the number
    // of matches in the whole scope and its error_code is set; Host.online is left unset.
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

private:
    friend class DatabaseStore;

    // Every list query has to name the page it wants. Whatever comes out of one goes into a single
    // reply, and a reply the channel cannot carry is not sent at all but ends the session, so the
    // size of an answer must never follow the size of the database. A request that names no page
    // has a count of zero and is refused along with one that asks for more than the cap.
    static bool isPageValid(qint64 offset, qint64 count, int max_count);

    bool openDatabase();

    // Checks that a new access entry targets a user that still exists. An unknown user means the
    // record was deleted after the sender took its snapshot, so it answers kErrorConflict and the
    // sender refetches. Must be called inside a transaction. Returns a proto::router error code.
    std::string_view checkAccessUser(qint64 user_id);

    mutable SqlDatabase db_;

    Q_DISABLE_COPY_MOVE(Database)
};

#endif // ROUTER_DATABASE_H
