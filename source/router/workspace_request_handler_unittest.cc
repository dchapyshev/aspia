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

#include "router/workspace_request_handler.h"

#include "proto/router_admin.h"
#include "router/router_test_base.h"
#include "router/workers/client_worker.h"

// The workspace surface of the admin channel against a real database.
class WorkspaceRequestHandlerTest : public RouterTestBase
{
protected:
    WorkspaceRequestHandler::Result handle(const proto::router::WorkspaceRequest& request)
    {
        return WorkspaceRequestHandler::handle(db_, caller_, request);
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
    const WorkspaceRequestHandler::Result result = handle(
        makeRequest(proto::router::kCommandWorkspaceAdd, QStringLiteral("alpha"), gk));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_GT(result.entry_id, 0);

    // A creation can only claim hosts, so with none requested the host lists stay valid.
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_WORKSPACES));
    EXPECT_EQ(workspaceName(result.entry_id), QStringLiteral("alpha"));
}

//--------------------------------------------------------------------------------------------------
// The group key is sealed by the sender, so a list without the sender's own entry would create a
// workspace nobody can open.
TEST_F(WorkspaceRequestHandlerTest, AddWithoutOwnAccessIsRejected)
{
    const SecureByteArray gk(Random::byteArray(32));
    proto::router::WorkspaceRequest request =
        makeRequest(proto::router::kCommandWorkspaceAdd, QStringLiteral("alpha"), gk);
    request.mutable_workspace()->clear_access();

    const WorkspaceRequestHandler::Result result = handle(request);

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
        makeRequest(proto::router::kCommandWorkspaceAdd, QStringLiteral("alpha"), gk);
    request.mutable_workspace()->add_host_id(host_id);

    const WorkspaceRequestHandler::Result result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.notify_flags,
              quint32(ClientWorker::NOTIFY_WORKSPACES | ClientWorker::NOTIFY_HOSTS));
    EXPECT_EQ(findHost(host_id).workspace_id(), result.entry_id);
}

//--------------------------------------------------------------------------------------------------
// The name of a workspace is what the operator typed, spaces included; the stored value is trimmed
// so " alpha " and "alpha" cannot coexist.
TEST_F(WorkspaceRequestHandlerTest, AddTrimsNameAndRejectsDuplicate)
{
    const SecureByteArray gk(Random::byteArray(32));
    const WorkspaceRequestHandler::Result first = handle(
        makeRequest(proto::router::kCommandWorkspaceAdd, QStringLiteral("  alpha  "), gk));

    ASSERT_EQ(first.error_code, proto::router::kErrorOk);
    EXPECT_EQ(workspaceName(first.entry_id), QStringLiteral("alpha"));

    const WorkspaceRequestHandler::Result second = handle(
        makeRequest(proto::router::kCommandWorkspaceAdd, QStringLiteral("alpha"), gk));

    EXPECT_EQ(second.error_code, proto::router::kErrorAlreadyExists);
    EXPECT_EQ(second.notify_flags, 0u);
    EXPECT_EQ(second.entry_id, 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(WorkspaceRequestHandlerTest, ModifyAppliesAndNotifies)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace(QStringLiteral("alpha"), gk);
    ASSERT_GT(workspace_id, 0);

    proto::router::WorkspaceRequest request =
        makeRequest(proto::router::kCommandWorkspaceModify, QStringLiteral("beta"), gk);
    request.mutable_workspace()->set_entry_id(workspace_id);
    request.mutable_workspace()->set_revision(workspaceRevision(workspace_id));

    const WorkspaceRequestHandler::Result result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);

    // The host assignments change even when no host is requested (every host of the workspace is
    // released), so the host lists are always announced as stale.
    EXPECT_EQ(result.notify_flags,
              quint32(ClientWorker::NOTIFY_WORKSPACES | ClientWorker::NOTIFY_HOSTS));
    EXPECT_EQ(workspaceName(workspace_id), QStringLiteral("beta"));
    EXPECT_EQ(workspaceRevision(workspace_id), 2);
}

//--------------------------------------------------------------------------------------------------
// An administrator has access to every workspace, so a save that drops the sender is malformed -
// and it must not apply the rest of the change either.
TEST_F(WorkspaceRequestHandlerTest, ModifyWithoutOwnAccessIsRejected)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace(QStringLiteral("alpha"), gk);
    ASSERT_GT(workspace_id, 0);

    proto::router::WorkspaceRequest request =
        makeRequest(proto::router::kCommandWorkspaceModify, QStringLiteral("beta"), gk);
    request.mutable_workspace()->set_entry_id(workspace_id);
    request.mutable_workspace()->set_revision(workspaceRevision(workspace_id));
    request.mutable_workspace()->clear_access();

    const WorkspaceRequestHandler::Result result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(workspaceName(workspace_id), QStringLiteral("alpha"));
}

//--------------------------------------------------------------------------------------------------
// A save built on an outdated snapshot answers conflict and changes nothing: the client refetches
// and retries instead of overwriting the concurrent change.
TEST_F(WorkspaceRequestHandlerTest, StaleRevisionIsConflict)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace(QStringLiteral("alpha"), gk);
    ASSERT_GT(workspace_id, 0);

    proto::router::WorkspaceRequest first =
        makeRequest(proto::router::kCommandWorkspaceModify, QStringLiteral("beta"), gk);
    first.mutable_workspace()->set_entry_id(workspace_id);
    first.mutable_workspace()->set_revision(workspaceRevision(workspace_id));
    ASSERT_EQ(handle(first).error_code, proto::router::kErrorOk);

    // The same request again - its revision is the one the first save consumed.
    proto::router::WorkspaceRequest stale =
        makeRequest(proto::router::kCommandWorkspaceModify, QStringLiteral("gamma"), gk);
    stale.mutable_workspace()->set_entry_id(workspace_id);
    stale.mutable_workspace()->set_revision(1);

    const WorkspaceRequestHandler::Result result = handle(stale);

    EXPECT_EQ(result.error_code, proto::router::kErrorConflict);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(workspaceName(workspace_id), QStringLiteral("beta"));
}

//--------------------------------------------------------------------------------------------------
// Deleting a workspace releases its hosts and takes its group tree with it, so all three cached
// lists of the clients are stale - the group list included.
TEST_F(WorkspaceRequestHandlerTest, DeleteReleasesHostsAndDropsGroups)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace(QStringLiteral("alpha"), gk, {host_id});
    ASSERT_GT(workspace_id, 0);

    qint64 group_id = -1;
    ASSERT_EQ(db_.addGroup(workspace_id, 0, "group", std::string_view(), &group_id),
              proto::router::kErrorOk);
    ASSERT_EQ(groupCount(workspace_id), 1);

    proto::router::WorkspaceRequest request;
    request.set_command_name(proto::router::kCommandWorkspaceDelete);
    request.mutable_workspace()->set_entry_id(workspace_id);

    const WorkspaceRequestHandler::Result result = handle(request);

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

    const WorkspaceRequestHandler::Result result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(result.notify_flags, 0u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(WorkspaceRequestHandlerTest, UnknownCommandIsInvalidRequest)
{
    proto::router::WorkspaceRequest request;
    request.set_command_name("workspace_frobnicate");
    request.mutable_workspace()->set_entry_id(1);

    const WorkspaceRequestHandler::Result result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidRequest);
    EXPECT_EQ(result.notify_flags, 0u);
}
