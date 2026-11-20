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

#ifndef ROUTER_STORAGE_GROUP_STORE_H
#define ROUTER_STORAGE_GROUP_STORE_H

#include "router/storage/database_store.h"

namespace proto::router {
class GroupList;
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

class GroupStore final : public DatabaseStore
{
public:
    explicit GroupStore(Database& db);

    // Appends every group in the given workspace (unspecified order) to |out| and sets its
    // error_code, reading rows straight into the protobuf message. The client builds a tree by
    // indexing on entry_id and linking via parent_id; display ordering is the client's job.
    void list(qint64 workspace_id, proto::router::GroupList* out) const;

    // Reads the group with the given entry_id from workspace_id into |group|. Returns
    // kErrorNotFound when no such row exists in this workspace (an out of range id included) and
    // kErrorInternalError when the answer could not be read - a database error must not pass for
    // a missing group.
    std::string_view find(qint64 workspace_id, qint64 entry_id, Group* group) const;

    // Inserts a new group. parent_id == 0 places it at the workspace root; otherwise parent_id
    // must reference an existing group within the same workspace. On success *entry_id is set
    // to the new id.
    std::string_view add(qint64 workspace_id, qint64 parent_id, std::string_view name,
        std::string_view comment, qint64* entry_id);

    // Renames and/or re-parents a group. new_parent_id must point to a group in the same
    // workspace and must not be the group itself or one of its descendants. The cycle check
    // runs a recursive CTE that walks parent links upward from new_parent_id; if entry_id
    // appears anywhere in that chain the move is refused. |base_revision| is the revision the
    // edit was built on: a stale one answers kErrorConflict.
    std::string_view modify(qint64 workspace_id, qint64 entry_id, qint64 base_revision,
        qint64 new_parent_id, std::string_view name, std::string_view comment);

    // Deletes the group and all its descendants, moving hosts from that subtree to the
    // workspace root (group_id <- 0).
    std::string_view remove(qint64 workspace_id, qint64 entry_id);
};

#endif // ROUTER_STORAGE_GROUP_STORE_H
