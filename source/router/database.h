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

namespace proto::router {
class Host;
class HostFilter;
class HostList;
class HostSearchResult;
} // namespace proto::router

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

private:
    friend class DatabaseStore;

    // Every list query has to name the page it wants. Whatever comes out of one goes into a single
    // reply, and a reply the channel cannot carry is not sent at all but ends the session, so the
    // size of an answer must never follow the size of the database. A request that names no page
    // has a count of zero and is refused along with one that asks for more than the cap.
    static bool isPageValid(qint64 offset, qint64 count, int max_count);

    bool openDatabase();

    mutable SqlDatabase db_;

    Q_DISABLE_COPY_MOVE(Database)
};

#endif // ROUTER_DATABASE_H
