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

#include "router/host_request_handler.h"

#include "proto/router_manager.h"
#include "router/router_test_base.h"
#include "router/workers/client_worker.h"

// The host edit of the manager channel against a real database.
class HostRequestHandlerTest : public RouterTestBase
{
protected:
    void SetUp() override
    {
        RouterTestBase::SetUp();

        host_id_ = addHost("hash-1");
        ASSERT_NE(host_id_, kInvalidHostId);

        gk_ = SecureByteArray(Random::byteArray(32));
        workspace_id_ = addWorkspace("alpha", gk_, {host_id_});
        ASSERT_GT(workspace_id_, 0);
    }

    HostRequestHandler::Result handle(const proto::router::HostRequest& request)
    {
        return HostRequestHandler::handle(db_, caller_, request);
    }

    proto::router::HostRequest makeRequest(HostId host_id, qint64 group_id,
                                           std::string_view display_name)
    {
        proto::router::HostRequest request;
        request.set_command_name(proto::router::kCommandHostModify);

        proto::router::Host* host = request.mutable_host();
        host->set_host_id(host_id);
        host->set_group_id(group_id);
        host->set_display_name(std::string(display_name));
        return request;
    }

    qint64 addGroup(qint64 workspace_id, const QString& name)
    {
        qint64 entry_id = -1;
        if (db_.addGroup(workspace_id, 0, name.toStdString(), std::string_view(), &entry_id) !=
            proto::router::kErrorOk)
        {
            return -1;
        }
        return entry_id;
    }

    SecureByteArray gk_;
    HostId host_id_ = kInvalidHostId;
    qint64 workspace_id_ = 0;
};

//--------------------------------------------------------------------------------------------------
TEST_F(HostRequestHandlerTest, MemberEditsHostAndNotifies)
{
    proto::router::HostRequest request = makeRequest(host_id_, 0, "display");
    request.mutable_host()->set_comment("encrypted-comment");

    const HostRequestHandler::Result result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_HOSTS));

    const proto::router::Host stored = findHost(host_id_);
    EXPECT_EQ(stored.display_name(), "display");
    EXPECT_EQ(stored.comment(), "encrypted-comment");
    EXPECT_GT(stored.last_modify(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostRequestHandlerTest, UnknownCommandIsInvalidRequest)
{
    proto::router::HostRequest request = makeRequest(host_id_, 0, "display");
    request.set_command_name(proto::router::kCommandHostDisconnect);

    const HostRequestHandler::Result result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidRequest);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_TRUE(findHost(host_id_).display_name().empty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostRequestHandlerTest, UnknownHostIsNotFound)
{
    const HostRequestHandler::Result result = handle(makeRequest(HostId(12345), 0, "display"));

    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(result.notify_flags, 0u);
}

//--------------------------------------------------------------------------------------------------
// A host outside any workspace has no group key, so its encrypted fields cannot be produced by
// anyone - editing it is refused until it is assigned to a workspace.
TEST_F(HostRequestHandlerTest, UnassignedHostCannotBeEdited)
{
    const HostId free_host = addHost("hash-2");
    ASSERT_NE(free_host, kInvalidHostId);

    const HostRequestHandler::Result result = handle(makeRequest(free_host, 0, "display"));

    EXPECT_EQ(result.error_code, proto::router::kErrorAccessDenied);
    EXPECT_TRUE(findHost(free_host).display_name().empty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostRequestHandlerTest, NonMemberCannotEditHost)
{
    const RouterUser client = addUser("client",
                                      proto::router::SESSION_TYPE_CLIENT);
    ASSERT_TRUE(client.isValid());

    caller_.user_id = client.entry_id;
    caller_.name = client.name;

    const HostRequestHandler::Result result = handle(makeRequest(host_id_, 0, "display"));

    EXPECT_EQ(result.error_code, proto::router::kErrorAccessDenied);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_TRUE(findHost(host_id_).display_name().empty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostRequestHandlerTest, HostIsMovedIntoGroupOfItsWorkspace)
{
    const qint64 group_id = addGroup(workspace_id_, "servers");
    ASSERT_GT(group_id, 0);

    const HostRequestHandler::Result result = handle(makeRequest(host_id_, group_id, "display"));

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(findHost(host_id_).group_id(), group_id);
}

//--------------------------------------------------------------------------------------------------
// The hosts table has no foreign key on group_id, so a group of another workspace would orphan the
// host in a tree it does not belong to.
TEST_F(HostRequestHandlerTest, GroupOfAnotherWorkspaceIsRejected)
{
    const qint64 other_id = addWorkspace("beta", gk_);
    ASSERT_GT(other_id, 0);

    const qint64 foreign_group = addGroup(other_id, "foreign");
    ASSERT_GT(foreign_group, 0);

    const HostRequestHandler::Result result =
        handle(makeRequest(host_id_, foreign_group, "display"));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(findHost(host_id_).group_id(), 0);
    EXPECT_TRUE(findHost(host_id_).display_name().empty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostRequestHandlerTest, UnknownGroupIsRejected)
{
    const HostRequestHandler::Result result = handle(makeRequest(host_id_, 12345, "display"));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(findHost(host_id_).group_id(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostRequestHandlerTest, NegativeGroupIsRejected)
{
    const HostRequestHandler::Result result = handle(makeRequest(host_id_, -1, "display"));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(findHost(host_id_).group_id(), 0);
}
