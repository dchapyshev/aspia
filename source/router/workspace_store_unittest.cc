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

#include "router/workspace_store.h"

#include "router/router_test_base.h"

class WorkspaceStoreTest : public RouterTestBase
{
protected:
    qint64 workspaceRevision(qint64 workspace_id)
    {
        proto::router::WorkspaceList list;
        workspaces_.listForAdmin(workspace_id, &list);
        if (list.error_code() != proto::router::kErrorOk || list.workspace_size() != 1)
            return -1;
        return list.workspace(0).revision();
    }
};

//--------------------------------------------------------------------------------------------------
// An administrator sees every workspace by its session type, so a workspace can be created
// without one - and without any member at all.
TEST_F(WorkspaceStoreTest, WorkspaceNeedsNoAdminInItsAccessList)
{
    qint64 entry_id = -1;
    ASSERT_EQ(workspaces_.add("alpha", std::string_view(), {}, &entry_id), proto::router::kErrorOk);
    ASSERT_GT(entry_id, 0);

    proto::router::WorkspaceList list;
    workspaces_.listForAdmin(0, &list);
    ASSERT_EQ(list.workspace_size(), 1);
    EXPECT_EQ(list.workspace(0).user_id_size(), 0);
}

//--------------------------------------------------------------------------------------------------
// A duplicate name is answered before anything is created.
TEST_F(WorkspaceStoreTest, DuplicateWorkspaceNameIsAlreadyExists)
{
    ASSERT_GT(addWorkspace("alpha"), 0);

    qint64 entry_id = -1;
    EXPECT_EQ(workspaces_.add("alpha", std::string_view(), {}, &entry_id), proto::router::kErrorAlreadyExists);
}

//--------------------------------------------------------------------------------------------------
// A modification based on a stale revision is rejected; a successful one increments the
// stored revision.
TEST_F(WorkspaceStoreTest, RevisionGuardsConcurrentModification)
{
    const qint64 workspace_id = addWorkspace("alpha");
    ASSERT_GT(workspace_id, 0);
    ASSERT_EQ(workspaceRevision(workspace_id), 1);

    EXPECT_EQ(workspaces_.modify(workspace_id, 1, "beta", std::string_view(),
                                 {admin_.entry_id}),
              proto::router::kErrorOk);
    EXPECT_EQ(workspaceRevision(workspace_id), 2);

    // A save built on the old snapshot must not overwrite the change above.
    EXPECT_EQ(workspaces_.modify(workspace_id, 1, "gamma", std::string_view(),
                                 {admin_.entry_id}),
              proto::router::kErrorConflict);
    EXPECT_EQ(workspaces_.find(workspace_id).name, "beta");
}

//--------------------------------------------------------------------------------------------------
TEST_F(WorkspaceStoreTest, ModifyWorkspaceGrantsAndRevokes)
{
    RouterUser client = makeUser("client", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_EQ(users_.add(client), proto::router::kErrorOk);
    client = findUser("client");

    const qint64 workspace_id = addWorkspace("alpha");
    ASSERT_GT(workspace_id, 0);

    ASSERT_EQ(workspaces_.modify(workspace_id, 1, "alpha", std::string_view(),
                                 {admin_.entry_id, client.entry_id}),
              proto::router::kErrorOk);

    std::set<qint64> workspace_ids;
    ASSERT_TRUE(workspaces_.accessIdsForUser(client.entry_id, &workspace_ids));
    EXPECT_TRUE(workspace_ids.contains(workspace_id));

    ASSERT_EQ(workspaces_.modify(workspace_id, 2, "alpha", std::string_view(),
                                 {admin_.entry_id}),
              proto::router::kErrorOk);

    ASSERT_TRUE(workspaces_.accessIdsForUser(client.entry_id, &workspace_ids));
    EXPECT_TRUE(workspace_ids.empty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(WorkspaceStoreTest, RemoveWorkspaceReleasesHostsAndAccess)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const qint64 workspace_id = addWorkspace("alpha");
    ASSERT_GT(workspace_id, 0);
    ASSERT_EQ(db_.modifyHost(host_id, findHost(host_id).revision(), workspace_id, 0, "display",
                             "note"),
              proto::router::kErrorOk);

    ASSERT_EQ(workspaces_.remove(workspace_id), proto::router::kErrorOk);

    std::set<qint64> workspace_ids;
    ASSERT_TRUE(workspaces_.accessIdsForUser(admin_.entry_id, &workspace_ids));
    EXPECT_TRUE(workspace_ids.empty());

    // The host is out of the workspace, its note is its own and survives the removal.
    const proto::router::Host host = findHost(host_id);
    EXPECT_EQ(host.workspace_id(), 0);
    EXPECT_EQ(host.comment(), "note");
}

//--------------------------------------------------------------------------------------------------
// An access entry for a user that is gone is how a concurrent delete looks: the sender still
// saw the user in its snapshot, so the answer is a conflict resolved by refetching.
TEST_F(WorkspaceStoreTest, EntryForDeletedUserIsConflict)
{
    RouterUser client = makeUser("client", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_EQ(users_.add(client), proto::router::kErrorOk);
    client = findUser("client");
    ASSERT_EQ(users_.remove(client.entry_id), proto::router::kErrorOk);

    qint64 entry_id = -1;
    EXPECT_EQ(workspaces_.add("alpha", std::string_view(),
                              {admin_.entry_id, client.entry_id}, &entry_id),
              proto::router::kErrorConflict);

    // The transaction rolled back: the name is still free.
    EXPECT_GT(addWorkspace("alpha"), 0);
}
