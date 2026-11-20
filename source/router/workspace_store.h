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

#ifndef ROUTER_WORKSPACE_STORE_H
#define ROUTER_WORKSPACE_STORE_H

#include "router/database_store.h"
#include "router/workspace.h"

namespace proto::router {
class WorkspaceList;
} // namespace proto::router

class WorkspaceStore final : public DatabaseStore
{
public:
    explicit WorkspaceStore(Database& db);

    // Append workspaces to |out|, reading rows straight into the protobuf message.
    // workspace_id == 0 returns every workspace of the scope; > 0 narrows to that single one.
    // Both set |out|'s error_code.
    //
    // ForAdmin lists every workspace of the router with the membership of each attached (for the
    // admin sessions managing it), fetched in a single query (no per-workspace round trip).
    // ForUser lists the workspaces |user_id| is a member of, without the membership of anybody.
    void listForAdmin(qint64 workspace_id, proto::router::WorkspaceList* out) const;
    void listForUser(qint64 user_id, qint64 workspace_id, proto::router::WorkspaceList* out) const;
    Workspace find(qint64 entry_id) const;

    // The initial access list holds the user_ids of the members of the workspace; an entry for a
    // user that is gone is rejected (kErrorConflict - the sender refetches, see checkAccessUser).
    // The hosts of a workspace are claimed one by one (see modifyHost), so a creation never
    // touches them.
    std::string_view add(std::string_view name, std::string_view comment,
        const std::vector<qint64>& initial_access, qint64* entry_id);

    // Updates name/comment and synchronizes the access list in a single transaction.
    // base_revision is the revision the client based its edit on; a mismatch with the stored
    // value is rejected (kErrorConflict) so a save built from a stale snapshot can never silently
    // overwrite a concurrent change, and on success the stored revision is incremented.
    // desired_access is the complete final access list: user_ids missing from it are revoked and
    // user_ids absent from the current DB record are inserted.
    std::string_view modify(qint64 entry_id, qint64 base_revision,
        std::string_view name, std::string_view comment, const std::vector<qint64>& desired_access);

    // Deletes a workspace, releases its hosts (workspace_id <- 0, group_id <- 0) and drops the
    // note each of them carried within it. Deliberately takes no base_revision: a delete is an
    // intent about the whole entity, not about a particular state of it, so it applies
    // regardless of concurrent edits (unlike modify).
    std::string_view remove(qint64 entry_id);

    // Fills |workspace_ids| with the ids of every workspace of the router. Returns false on a
    // database error.
    bool ids(std::set<qint64>* workspace_ids) const;

    // Fills |workspace_ids| with the ids of every workspace the user has a workspace_access
    // entry for. Returns false on a database error.
    bool accessIdsForUser(qint64 user_id, std::set<qint64>* workspace_ids) const;

    // Returns kErrorOk when the user has an access entry for the workspace, kErrorAccessDenied
    // when it has none and kErrorInternalError when the answer could not be read - a caller whose
    // skip-or-reject decision depends on the answer must not mistake an error for "no access".
    std::string_view checkAccess(qint64 user_id, qint64 workspace_id) const;

private:
    // Checks that a new access entry targets a user that still exists. An unknown user means the
    // record was deleted after the sender took its snapshot, so it answers kErrorConflict and the
    // sender refetches. Must be called inside a transaction. Returns a proto::router error code.
    std::string_view checkAccessUser(qint64 user_id);
};

#endif // ROUTER_WORKSPACE_STORE_H
