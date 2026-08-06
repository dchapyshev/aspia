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

#include "router/handlers/workspace_request_handler.h"

#include "proto/router_admin.h"
#include "router/router_test_base.h"
#include "router/workers/client_worker.h"

// The workspace surface of the admin channel against a real database.
class WorkspaceRequestHandlerTest : public RouterTestBase
{
protected:
    RequestResult handle(const proto::router::WorkspaceRequest& request)
    {
        return handleWorkspaceRequest(db_, caller_, request);
    }

    // A request with the access entry of the sender attached: an administrator has access to every
    // workspace, so this is what a well-formed save looks like.
    proto::router::WorkspaceRequest makeRequest(std::string_view command, const QString& name,
                                                const SecureByteArray& gk)
    {
        proto::router::WorkspaceRequest request;
        request.set_command_name(std::string(command));

        proto::router::Workspace* workspace = request.mutable_workspace();
        workspace->set_name(name.toStdString());

        proto::router::WorkspaceAccess* access = workspace->add_access();
        access->set_user_id(admin_.entry_id);
        access->set_wrapped_gk(toStdString(SealedBox::seal(gk, admin_.public_key)));
        access->set_public_key(toStdString(admin_.public_key));

        return request;
    }

    QString workspaceName(qint64 entry_id)
    {
        return QString::fromStdString(db_.findWorkspace(entry_id).name);
    }

    qint64 workspaceRevision(qint64 entry_id)
    {
        proto::router::WorkspaceList list;
        db_.workspaceListWithAllAccess(admin_.entry_id, entry_id, &list);
        if (list.error_code() != proto::router::kErrorOk || list.workspace_size() != 1)
            return -1;
        return list.workspace(0).revision();
    }

    int groupCount(qint64 workspace_id)
    {
        proto::router::GroupList list;
        db_.groupList(workspace_id, &list);
        if (list.error_code() != proto::router::kErrorOk)
            return -1;
        return list.group_size();
    }
};

//--------------------------------------------------------------------------------------------------
TEST_F(WorkspaceRequestHandlerTest, AddCreatesWorkspaceAndNotifies)
{
    const SecureByteArray gk(Random::byteArray(32));
    const RequestResult result = handle(
        makeRequest(proto::router::kCommandWorkspaceAdd, "alpha", gk));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_GT(result.entry_id, 0);

    // A creation can only claim hosts, so with none requested the host lists stay valid.
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_WORKSPACES));
    EXPECT_EQ(workspaceName(result.entry_id), "alpha");
}

//--------------------------------------------------------------------------------------------------
// The group key is sealed by the sender, so a list without the sender's own entry would create a
// workspace nobody can open.
TEST_F(WorkspaceRequestHandlerTest, AddWithoutOwnAccessIsRejected)
{
    const SecureByteArray gk(Random::byteArray(32));
    proto::router::WorkspaceRequest request =
        makeRequest(proto::router::kCommandWorkspaceAdd, "alpha", gk);
    request.mutable_workspace()->clear_access();

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(result.entry_id, 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(WorkspaceRequestHandlerTest, AddWithHostsClaimsThemAndNotifiesHosts)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const SecureByteArray gk(Random::byteArray(32));
    proto::router::WorkspaceRequest request =
        makeRequest(proto::router::kCommandWorkspaceAdd, "alpha", gk);
    request.mutable_workspace()->add_host_id(host_id);

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.notify_flags,
              quint32(ClientWorker::NOTIFY_WORKSPACES | ClientWorker::NOTIFY_HOSTS));
    EXPECT_EQ(findHost(host_id).workspace_id(), result.entry_id);
}

//--------------------------------------------------------------------------------------------------
// A workspace must be named, and blanks alone are not a name.
TEST_F(WorkspaceRequestHandlerTest, AddRejectsEmptyName)
{
    const SecureByteArray gk(Random::byteArray(32));

    const RequestResult result = handle(
        makeRequest(proto::router::kCommandWorkspaceAdd, "   ", gk));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(result.entry_id, 0);
}

//--------------------------------------------------------------------------------------------------
// The name of a workspace is what the operator typed, spaces included; the stored value is trimmed
// so " alpha " and "alpha" cannot coexist.
TEST_F(WorkspaceRequestHandlerTest, AddTrimsNameAndRejectsDuplicate)
{
    const SecureByteArray gk(Random::byteArray(32));
    const RequestResult first = handle(
        makeRequest(proto::router::kCommandWorkspaceAdd, "  alpha  ", gk));

    ASSERT_EQ(first.error_code, proto::router::kErrorOk);
    EXPECT_EQ(workspaceName(first.entry_id), "alpha");

    const RequestResult second = handle(
        makeRequest(proto::router::kCommandWorkspaceAdd, "alpha", gk));

    EXPECT_EQ(second.error_code, proto::router::kErrorAlreadyExists);
    EXPECT_EQ(second.notify_flags, 0u);
    EXPECT_EQ(second.entry_id, 0);
}

//--------------------------------------------------------------------------------------------------
// The comment is read back in the workspace list, and a reply larger than the message limit of the
// channel is not sent but ends the session instead, so it is bounded on the way in. The name is
// already bounded by Workspace::isValidName.
TEST_F(WorkspaceRequestHandlerTest, OversizedCommentIsRejected)
{
    const SecureByteArray gk(Random::byteArray(32));
    proto::router::WorkspaceRequest request =
        makeRequest(proto::router::kCommandWorkspaceAdd, "alpha", gk);
    request.mutable_workspace()->set_comment(std::string(proto::router::kMaxCommentLength + 1, 'c'));

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(result.entry_id, 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(WorkspaceRequestHandlerTest, CommentAtTheLimitIsAccepted)
{
    const SecureByteArray gk(Random::byteArray(32));
    proto::router::WorkspaceRequest request =
        makeRequest(proto::router::kCommandWorkspaceAdd, "alpha", gk);
    request.mutable_workspace()->set_comment(std::string(proto::router::kMaxCommentLength, 'c'));

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(workspaceName(result.entry_id), "alpha");
}

//--------------------------------------------------------------------------------------------------
TEST_F(WorkspaceRequestHandlerTest, ModifyAppliesAndNotifies)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace("alpha", gk);
    ASSERT_GT(workspace_id, 0);

    proto::router::WorkspaceRequest request =
        makeRequest(proto::router::kCommandWorkspaceModify, "beta", gk);
    request.mutable_workspace()->set_entry_id(workspace_id);
    request.mutable_workspace()->set_revision(workspaceRevision(workspace_id));

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);

    // The host assignments change even when no host is requested (every host of the workspace is
    // released), so the host lists are always announced as stale.
    EXPECT_EQ(result.notify_flags,
              quint32(ClientWorker::NOTIFY_WORKSPACES | ClientWorker::NOTIFY_HOSTS));
    EXPECT_EQ(workspaceName(workspace_id), "beta");
    EXPECT_EQ(workspaceRevision(workspace_id), 2);
}

//--------------------------------------------------------------------------------------------------
// An administrator has access to every workspace, so a save that drops the sender is malformed -
// and it must not apply the rest of the change either.
TEST_F(WorkspaceRequestHandlerTest, ModifyWithoutOwnAccessIsRejected)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace("alpha", gk);
    ASSERT_GT(workspace_id, 0);

    proto::router::WorkspaceRequest request =
        makeRequest(proto::router::kCommandWorkspaceModify, "beta", gk);
    request.mutable_workspace()->set_entry_id(workspace_id);
    request.mutable_workspace()->set_revision(workspaceRevision(workspace_id));
    request.mutable_workspace()->clear_access();

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(workspaceName(workspace_id), "alpha");
}

//--------------------------------------------------------------------------------------------------
// A save built on an outdated snapshot answers conflict and changes nothing: the client refetches
// and retries instead of overwriting the concurrent change.
TEST_F(WorkspaceRequestHandlerTest, StaleRevisionIsConflict)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace("alpha", gk);
    ASSERT_GT(workspace_id, 0);

    proto::router::WorkspaceRequest first =
        makeRequest(proto::router::kCommandWorkspaceModify, "beta", gk);
    first.mutable_workspace()->set_entry_id(workspace_id);
    first.mutable_workspace()->set_revision(workspaceRevision(workspace_id));
    ASSERT_EQ(handle(first).error_code, proto::router::kErrorOk);

    // The same request again - its revision is the one the first save consumed.
    proto::router::WorkspaceRequest stale =
        makeRequest(proto::router::kCommandWorkspaceModify, "gamma", gk);
    stale.mutable_workspace()->set_entry_id(workspace_id);
    stale.mutable_workspace()->set_revision(1);

    const RequestResult result = handle(stale);

    EXPECT_EQ(result.error_code, proto::router::kErrorConflict);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(workspaceName(workspace_id), "beta");
}

//--------------------------------------------------------------------------------------------------
// Deleting a workspace releases its hosts and takes its group tree with it, so all three cached
// lists of the clients are stale - the group list included.
TEST_F(WorkspaceRequestHandlerTest, DeleteReleasesHostsAndDropsGroups)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace("alpha", gk, {host_id});
    ASSERT_GT(workspace_id, 0);

    qint64 group_id = -1;
    ASSERT_EQ(db_.addGroup(workspace_id, 0, "group", std::string_view(), &group_id),
              proto::router::kErrorOk);
    ASSERT_EQ(groupCount(workspace_id), 1);

    proto::router::WorkspaceRequest request;
    request.set_command_name(proto::router::kCommandWorkspaceDelete);
    request.mutable_workspace()->set_entry_id(workspace_id);

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.notify_flags,
              quint32(ClientWorker::NOTIFY_WORKSPACES | ClientWorker::NOTIFY_HOSTS |
                      ClientWorker::NOTIFY_GROUPS));
    EXPECT_EQ(findHost(host_id).workspace_id(), 0);
    EXPECT_EQ(groupCount(workspace_id), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(WorkspaceRequestHandlerTest, DeleteOfUnknownWorkspaceIsNotFound)
{
    proto::router::WorkspaceRequest request;
    request.set_command_name(proto::router::kCommandWorkspaceDelete);
    request.mutable_workspace()->set_entry_id(12345);

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(result.notify_flags, 0u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(WorkspaceRequestHandlerTest, UnknownCommandIsInvalidRequest)
{
    proto::router::WorkspaceRequest request;
    request.set_command_name("workspace_frobnicate");
    request.mutable_workspace()->set_entry_id(1);

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidRequest);
    EXPECT_EQ(result.notify_flags, 0u);
}

// The list of the workspaces a session may see over the client channel.
class WorkspaceListTest : public RouterTestBase
{
protected:
    void SetUp() override
    {
        RouterTestBase::SetUp();

        caller_.session_type = proto::router::SESSION_TYPE_ADMIN;

        gk_ = SecureByteArray(Random::byteArray(32));
        workspace_id_ = addWorkspace("alpha", gk_);
        ASSERT_GT(workspace_id_, 0);
    }

    proto::router::WorkspaceList workspaceList(qint64 workspace_id)
    {
        proto::router::WorkspaceListRequest request;
        request.set_workspace_id(workspace_id);

        proto::router::WorkspaceList out;
        handleWorkspaceList(db_, caller_, request, &out);
        return out;
    }

    SecureByteArray gk_;
    qint64 workspace_id_ = 0;
};

//--------------------------------------------------------------------------------------------------
// An administrator manages membership, so it gets every member's access entry.
TEST_F(WorkspaceListTest, AdminSeesFullMembership)
{
    RouterUser client = addUser("client", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_TRUE(client.isValid());

    ASSERT_EQ(db_.modifyWorkspace(workspace_id_, 1, "alpha", std::string_view(),
                                  {accessEntry(admin_, gk_), accessEntry(client, gk_)}, {}),
              proto::router::kErrorOk);

    const proto::router::WorkspaceList list = workspaceList(0);

    ASSERT_EQ(list.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(list.workspace_size(), 1);
    EXPECT_EQ(list.workspace(0).access_size(), 2);
}

//--------------------------------------------------------------------------------------------------
// A session that does not manage membership gets only its own entry: the wrapped group keys of the
// other members are not its business.
TEST_F(WorkspaceListTest, NonAdminSeesOnlyOwnAccessEntry)
{
    RouterUser client = addUser("client", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_TRUE(client.isValid());

    ASSERT_EQ(db_.modifyWorkspace(workspace_id_, 1, "alpha", std::string_view(),
                                  {accessEntry(admin_, gk_), accessEntry(client, gk_)}, {}),
              proto::router::kErrorOk);

    setCaller(client, proto::router::SESSION_TYPE_CLIENT);

    const proto::router::WorkspaceList list = workspaceList(0);

    ASSERT_EQ(list.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(list.workspace_size(), 1);
    ASSERT_EQ(list.workspace(0).access_size(), 1);
    EXPECT_EQ(list.workspace(0).access(0).user_id(), client.entry_id);
    EXPECT_FALSE(list.workspace(0).access(0).wrapped_gk().empty());
}

//--------------------------------------------------------------------------------------------------
// A workspace the session is not a member of is not in the list at all - neither in the full list
// nor when asked for by id.
TEST_F(WorkspaceListTest, InvisibleWorkspaceIsNotListed)
{
    RouterUser client = addUser("client", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_TRUE(client.isValid());
    setCaller(client, proto::router::SESSION_TYPE_CLIENT);

    const proto::router::WorkspaceList all = workspaceList(0);
    EXPECT_EQ(all.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(all.workspace_size(), 0);

    const proto::router::WorkspaceList single = workspaceList(workspace_id_);
    EXPECT_EQ(single.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(single.workspace_size(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(WorkspaceListTest, WorkspaceListNarrowsToRequestedId)
{
    const qint64 other_id = addWorkspace("beta", gk_);
    ASSERT_GT(other_id, 0);

    EXPECT_EQ(workspaceList(0).workspace_size(), 2);

    const proto::router::WorkspaceList single = workspaceList(other_id);
    ASSERT_EQ(single.workspace_size(), 1);
    EXPECT_EQ(single.workspace(0).entry_id(), other_id);
}
