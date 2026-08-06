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

#include "router/handlers/group_request_handler.h"

#include "proto/router_manager.h"
#include "router/router_test_base.h"
#include "router/workers/client_worker.h"

// The host-group surface of the manager channel against a real database.
class GroupRequestHandlerTest : public RouterTestBase
{
protected:
    void SetUp() override
    {
        RouterTestBase::SetUp();

        gk_ = SecureByteArray(Random::byteArray(32));
        workspace_id_ = addWorkspace("alpha", gk_);
        ASSERT_GT(workspace_id_, 0);
    }

    RequestResult handle(const proto::router::GroupRequest& request)
    {
        return handleGroupRequest(db_, caller_, request);
    }

    proto::router::GroupRequest makeRequest(std::string_view command, qint64 workspace_id)
    {
        proto::router::GroupRequest request;
        request.set_command_name(std::string(command));
        request.set_workspace_id(workspace_id);
        return request;
    }

    qint64 addGroup(qint64 workspace_id, qint64 parent_id, const QString& name)
    {
        qint64 entry_id = -1;
        if (db_.addGroup(workspace_id, parent_id, name.toStdString(), std::string_view(),
                         &entry_id) != proto::router::kErrorOk)
        {
            return -1;
        }
        return entry_id;
    }

    QString groupName(qint64 workspace_id, qint64 entry_id)
    {
        return QString::fromStdString(db_.findGroup(workspace_id, entry_id).name);
    }

    int groupCount(qint64 workspace_id)
    {
        proto::router::GroupList list;
        db_.groupList(workspace_id, &list);
        if (list.error_code() != proto::router::kErrorOk)
            return -1;
        return list.group_size();
    }

    proto::router::GroupList groupList(qint64 workspace_id)
    {
        proto::router::GroupListRequest request;
        request.set_workspace_id(workspace_id);

        proto::router::GroupList out;
        handleGroupList(db_, caller_, request, &out);
        return out;
    }

    SecureByteArray gk_;
    qint64 workspace_id_ = 0;
};

//--------------------------------------------------------------------------------------------------
TEST_F(GroupRequestHandlerTest, AddCreatesGroupAndNotifies)
{
    proto::router::GroupRequest request =
        makeRequest(proto::router::kCommandGroupAdd, workspace_id_);
    request.mutable_group()->set_name("servers");

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_GT(result.entry_id, 0);
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_GROUPS));
    EXPECT_EQ(groupName(workspace_id_, result.entry_id), "servers");
}

//--------------------------------------------------------------------------------------------------
// The group fields are encrypted with the group key of the workspace, which only its members hold:
// a non-member has nothing to encrypt with and no business editing the tree.
TEST_F(GroupRequestHandlerTest, NonMemberIsDenied)
{
    const RouterUser client = addUser("client",
                                      proto::router::SESSION_TYPE_CLIENT);
    ASSERT_TRUE(client.isValid());

    caller_.user_id = client.entry_id;
    caller_.name = client.name.toStdString();

    proto::router::GroupRequest request =
        makeRequest(proto::router::kCommandGroupAdd, workspace_id_);
    request.mutable_group()->set_name("servers");

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorAccessDenied);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(groupCount(workspace_id_), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(GroupRequestHandlerTest, InvalidWorkspaceIdIsInvalidRequest)
{
    proto::router::GroupRequest request = makeRequest(proto::router::kCommandGroupAdd, 0);
    request.mutable_group()->set_name("servers");

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidRequest);
    EXPECT_EQ(result.notify_flags, 0u);
}

//--------------------------------------------------------------------------------------------------
// A workspace the sender is not a member of answers "access denied" and not "no such workspace":
// the existence of another workspace is not the sender's business either.
TEST_F(GroupRequestHandlerTest, UnknownWorkspaceIsDenied)
{
    proto::router::GroupRequest request = makeRequest(proto::router::kCommandGroupAdd, 12345);
    request.mutable_group()->set_name("servers");

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorAccessDenied);
}

//--------------------------------------------------------------------------------------------------
TEST_F(GroupRequestHandlerTest, AddRejectsEmptyName)
{
    proto::router::GroupRequest request =
        makeRequest(proto::router::kCommandGroupAdd, workspace_id_);
    request.mutable_group()->set_name("   ");

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(groupCount(workspace_id_), 0);
}

//--------------------------------------------------------------------------------------------------
// Everything stored here comes back in the group list of the workspace, and a reply that outgrows
// the message limit of the channel is not sent but ends the session. So the fields are bounded on
// the way in, or one entry would break every client that opens the workspace, over and over,
// because the entry stays in the database.
TEST_F(GroupRequestHandlerTest, AddRejectsOversizedName)
{
    proto::router::GroupRequest request =
        makeRequest(proto::router::kCommandGroupAdd, workspace_id_);
    request.mutable_group()->set_name(std::string(proto::router::kMaxEntryNameLength + 1, 'n'));

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(groupCount(workspace_id_), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(GroupRequestHandlerTest, AddRejectsOversizedComment)
{
    proto::router::GroupRequest request =
        makeRequest(proto::router::kCommandGroupAdd, workspace_id_);
    request.mutable_group()->set_name("servers");
    request.mutable_group()->set_comment(std::string(proto::router::kMaxCommentLength + 1, 'c'));

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(groupCount(workspace_id_), 0);
}

//--------------------------------------------------------------------------------------------------
// The bound is a maximum and not a step below one, so an entry that sits exactly on it is stored.
TEST_F(GroupRequestHandlerTest, AddAcceptsFieldsAtTheLimit)
{
    proto::router::GroupRequest request =
        makeRequest(proto::router::kCommandGroupAdd, workspace_id_);
    request.mutable_group()->set_name(std::string(proto::router::kMaxEntryNameLength, 'n'));
    request.mutable_group()->set_comment(std::string(proto::router::kMaxCommentLength, 'c'));

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(groupCount(workspace_id_), 1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(GroupRequestHandlerTest, ModifyRejectsOversizedFields)
{
    const qint64 group_id = addGroup(workspace_id_, 0, "servers");
    ASSERT_GT(group_id, 0);

    proto::router::GroupRequest name_request =
        makeRequest(proto::router::kCommandGroupModify, workspace_id_);
    name_request.mutable_group()->set_entry_id(group_id);
    name_request.mutable_group()->set_name(std::string(proto::router::kMaxEntryNameLength + 1, 'n'));

    EXPECT_EQ(handle(name_request).error_code, proto::router::kErrorInvalidData);

    proto::router::GroupRequest comment_request =
        makeRequest(proto::router::kCommandGroupModify, workspace_id_);
    comment_request.mutable_group()->set_entry_id(group_id);
    comment_request.mutable_group()->set_name("servers");
    comment_request.mutable_group()->set_comment(std::string(proto::router::kMaxCommentLength + 1, 'c'));

    EXPECT_EQ(handle(comment_request).error_code, proto::router::kErrorInvalidData);

    EXPECT_EQ(groupName(workspace_id_, group_id), "servers");
    EXPECT_TRUE(db_.findGroup(workspace_id_, group_id).comment.empty());
}

//--------------------------------------------------------------------------------------------------
// parent_id must point into the same workspace: a link across the boundary would put a group into
// a tree encrypted with a different group key.
TEST_F(GroupRequestHandlerTest, AddRejectsParentFromAnotherWorkspace)
{
    const qint64 other_id = addWorkspace("beta", gk_);
    ASSERT_GT(other_id, 0);

    const qint64 foreign_group = addGroup(other_id, 0, "foreign");
    ASSERT_GT(foreign_group, 0);

    proto::router::GroupRequest request =
        makeRequest(proto::router::kCommandGroupAdd, workspace_id_);
    request.mutable_group()->set_name("servers");
    request.mutable_group()->set_parent_id(foreign_group);

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(groupCount(workspace_id_), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(GroupRequestHandlerTest, ModifyRenamesAndNotifies)
{
    const qint64 group_id = addGroup(workspace_id_, 0, "servers");
    ASSERT_GT(group_id, 0);

    proto::router::GroupRequest request =
        makeRequest(proto::router::kCommandGroupModify, workspace_id_);
    request.mutable_group()->set_entry_id(group_id);
    request.mutable_group()->set_name("workstations");

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_GROUPS));
    EXPECT_EQ(groupName(workspace_id_, group_id), "workstations");
}

//--------------------------------------------------------------------------------------------------
// Moving a group under its own descendant would cut the subtree off the tree entirely.
TEST_F(GroupRequestHandlerTest, ModifyRejectsCycle)
{
    const qint64 parent_id = addGroup(workspace_id_, 0, "parent");
    ASSERT_GT(parent_id, 0);
    const qint64 child_id = addGroup(workspace_id_, parent_id, "child");
    ASSERT_GT(child_id, 0);

    proto::router::GroupRequest request =
        makeRequest(proto::router::kCommandGroupModify, workspace_id_);
    request.mutable_group()->set_entry_id(parent_id);
    request.mutable_group()->set_parent_id(child_id);
    request.mutable_group()->set_name("parent");

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(result.notify_flags, 0u);
}

//--------------------------------------------------------------------------------------------------
// The workspace of the request is what scopes the edit: a group of another workspace is simply not
// there, even for an administrator that has access to both.
TEST_F(GroupRequestHandlerTest, ModifyOfForeignGroupIsNotFound)
{
    const qint64 other_id = addWorkspace("beta", gk_);
    ASSERT_GT(other_id, 0);

    const qint64 foreign_group = addGroup(other_id, 0, "foreign");
    ASSERT_GT(foreign_group, 0);

    proto::router::GroupRequest request =
        makeRequest(proto::router::kCommandGroupModify, workspace_id_);
    request.mutable_group()->set_entry_id(foreign_group);
    request.mutable_group()->set_name("stolen");

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(groupName(other_id, foreign_group), "foreign");
}

//--------------------------------------------------------------------------------------------------
// The whole subtree goes, and the hosts that pointed into it are detached to the workspace root -
// so the host lists are stale too.
TEST_F(GroupRequestHandlerTest, DeleteDropsSubtreeAndDetachesHosts)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const qint64 parent_id = addGroup(workspace_id_, 0, "parent");
    ASSERT_GT(parent_id, 0);
    const qint64 child_id = addGroup(workspace_id_, parent_id, "child");
    ASSERT_GT(child_id, 0);

    ASSERT_EQ(db_.modifyWorkspace(workspace_id_, 1, "alpha", std::string_view(),
                                  {accessEntry(admin_, gk_)}, {host_id}),
              proto::router::kErrorOk);
    ASSERT_TRUE(db_.modifyHost(host_id, child_id, "host", std::string_view(), std::string_view(),
                               std::string_view()));

    proto::router::GroupRequest request =
        makeRequest(proto::router::kCommandGroupDelete, workspace_id_);
    request.mutable_group()->set_entry_id(parent_id);

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.notify_flags,
              quint32(ClientWorker::NOTIFY_GROUPS | ClientWorker::NOTIFY_HOSTS));
    EXPECT_EQ(groupCount(workspace_id_), 0);
    EXPECT_EQ(findHost(host_id).group_id(), 0);
    EXPECT_EQ(findHost(host_id).workspace_id(), workspace_id_);
}

//--------------------------------------------------------------------------------------------------
TEST_F(GroupRequestHandlerTest, DeleteOfUnknownGroupIsNotFound)
{
    proto::router::GroupRequest request =
        makeRequest(proto::router::kCommandGroupDelete, workspace_id_);
    request.mutable_group()->set_entry_id(12345);

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(result.notify_flags, 0u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(GroupRequestHandlerTest, UnknownCommandIsInvalidRequest)
{
    proto::router::GroupRequest request = makeRequest("group_frobnicate", workspace_id_);
    request.mutable_group()->set_entry_id(1);

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidRequest);
    EXPECT_EQ(result.notify_flags, 0u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(GroupRequestHandlerTest, GroupListIsScopedToItsWorkspace)
{
    const qint64 other_id = addWorkspace("beta", gk_);
    ASSERT_GT(other_id, 0);

    ASSERT_GT(addGroup(workspace_id_, 0, "servers"), 0);
    ASSERT_GT(addGroup(other_id, 0, "foreign"), 0);

    const proto::router::GroupList list = groupList(workspace_id_);

    ASSERT_EQ(list.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(list.group_size(), 1);
    EXPECT_EQ(list.group(0).name(), "servers");
    EXPECT_EQ(list.workspace_id(), workspace_id_);
}

//--------------------------------------------------------------------------------------------------
TEST_F(GroupRequestHandlerTest, GroupListRequiresWorkspaceAccess)
{
    RouterUser client = addUser("client", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_TRUE(client.isValid());
    setCaller(client, proto::router::SESSION_TYPE_CLIENT);

    const proto::router::GroupList list = groupList(workspace_id_);

    EXPECT_EQ(list.error_code(), proto::router::kErrorAccessDenied);
    EXPECT_EQ(list.group_size(), 0);
    EXPECT_EQ(list.workspace_id(), workspace_id_);
}

//--------------------------------------------------------------------------------------------------
TEST_F(GroupRequestHandlerTest, GroupListOfInvalidWorkspaceIsInvalidRequest)
{
    const proto::router::GroupList list = groupList(0);

    EXPECT_EQ(list.error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(list.group_size(), 0);
}
