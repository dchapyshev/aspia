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

#include "router/handlers/host_request_handler.h"

#include "base/serialization.h"
#include "base/net/tcp_channel.h"
#include "proto/router_client.h"
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

    RequestResult handle(const proto::router::HostRequest& request)
    {
        return handleHostRequest(db_, caller_, request);
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

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_HOSTS));

    const proto::router::Host stored = findHost(host_id_);
    EXPECT_EQ(stored.display_name(), "display");
    EXPECT_EQ(stored.comment(), "encrypted-comment");
    EXPECT_GT(stored.last_modify(), 0);
}

//--------------------------------------------------------------------------------------------------
// Every stored field comes back in the host list, and a reply larger than the message limit of the
// channel is not sent but ends the session instead. Two hosts carrying a three-megabyte comment
// were enough to push the list of one group past that limit, and since the records stay in the
// database the session was torn down again after every reconnect. So the fields are bounded here.
TEST_F(HostRequestHandlerTest, OversizedFieldsAreRejected)
{
    const struct { const char* what; size_t size; } kCases[] = {
        { "display_name", proto::router::kMaxEntryNameLength + 1 },
        { "comment",      proto::router::kMaxCommentLength + 1 },
        { "user_name",    proto::router::kMaxCredentialLength + 1 },
        { "password",     proto::router::kMaxCredentialLength + 1 }
    };

    for (const auto& test_case : kCases)
    {
        proto::router::HostRequest request = makeRequest(host_id_, 0, "display");
        proto::router::Host* host = request.mutable_host();
        const std::string oversized(test_case.size, 'x');

        if (test_case.what == std::string_view("display_name"))
            host->set_display_name(oversized);
        else if (test_case.what == std::string_view("comment"))
            host->set_comment(oversized);
        else if (test_case.what == std::string_view("user_name"))
            host->set_user_name(oversized);
        else
            host->set_password(oversized);

        EXPECT_EQ(handle(request).error_code, proto::router::kErrorInvalidData) << test_case.what;
    }

    // Nothing of any of those requests reached the record.
    const proto::router::Host stored = findHost(host_id_);
    EXPECT_TRUE(stored.display_name().empty());
    EXPECT_TRUE(stored.comment().empty());
    EXPECT_TRUE(stored.user_name().empty());
    EXPECT_TRUE(stored.password().empty());
}

//--------------------------------------------------------------------------------------------------
// The bounds are maximums and not a step below one, so a request that sits exactly on them is
// stored.
TEST_F(HostRequestHandlerTest, FieldsAtTheLimitAreAccepted)
{
    proto::router::HostRequest request = makeRequest(host_id_, 0, "display");
    proto::router::Host* host = request.mutable_host();
    host->set_display_name(std::string(proto::router::kMaxEntryNameLength, 'n'));
    host->set_comment(std::string(proto::router::kMaxCommentLength, 'c'));
    host->set_user_name(std::string(proto::router::kMaxCredentialLength, 'u'));
    host->set_password(std::string(proto::router::kMaxCredentialLength, 'p'));

    EXPECT_EQ(handle(request).error_code, proto::router::kErrorOk);

    const proto::router::Host stored = findHost(host_id_);
    EXPECT_EQ(stored.display_name().size(), proto::router::kMaxEntryNameLength);
    EXPECT_EQ(stored.comment().size(), proto::router::kMaxCommentLength);
    EXPECT_EQ(stored.user_name().size(), proto::router::kMaxCredentialLength);
    EXPECT_EQ(stored.password().size(), proto::router::kMaxCredentialLength);
}

//--------------------------------------------------------------------------------------------------
// The scenario the bounds exist for, end to end. Two hosts of one workspace are fed a comment of
// three megabytes each. With both edits refused the group listing serializes to something the
// channel can carry, where before it came to 6291527 bytes against a limit of 5242880.
TEST_F(HostRequestHandlerTest, HostListStaysSendableAfterOversizedEdits)
{
    const HostId second_id = addHost("hash-2");
    ASSERT_NE(second_id, kInvalidHostId);
    ASSERT_TRUE(execRaw(QString("UPDATE hosts SET workspace_id=%1 WHERE id=%2")
                        .arg(workspace_id_).arg(second_id)));

    for (HostId id : { host_id_, second_id })
    {
        proto::router::HostRequest request = makeRequest(id, 0, "display");
        request.mutable_host()->set_comment(std::string(3 * 1024 * 1024, 'x'));
        EXPECT_EQ(handle(request).error_code, proto::router::kErrorInvalidData);
    }

    proto::router::HostListRequest list_request;
    list_request.set_mode(proto::router::HostListRequest::MODE_FILTERED);
    list_request.set_workspace_id(workspace_id_);
    list_request.set_group_id(0);
    list_request.set_offset(0);
    list_request.set_count(proto::router::kMaxHostPageSize);

    caller_.session_type = proto::router::SESSION_TYPE_MANAGER;

    proto::router::RouterToClient message;
    proto::router::HostList* list = message.mutable_host_list();
    handleHostList(db_, caller_, list_request, list);

    ASSERT_EQ(list->error_code(), proto::router::kErrorOk);
    ASSERT_EQ(list->host_size(), 2);
    EXPECT_LE(serialize(message).size(), qsizetype(TcpChannel::kMaxMessageSize));
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostRequestHandlerTest, UnknownCommandIsInvalidRequest)
{
    proto::router::HostRequest request = makeRequest(host_id_, 0, "display");
    request.set_command_name(proto::router::kCommandHostDisconnect);

    const RequestResult result = handle(request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidRequest);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_TRUE(findHost(host_id_).display_name().empty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostRequestHandlerTest, UnknownHostIsNotFound)
{
    const RequestResult result = handle(makeRequest(HostId(12345), 0, "display"));

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

    const RequestResult result = handle(makeRequest(free_host, 0, "display"));

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
    caller_.name = client.name.toStdString();

    const RequestResult result = handle(makeRequest(host_id_, 0, "display"));

    EXPECT_EQ(result.error_code, proto::router::kErrorAccessDenied);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_TRUE(findHost(host_id_).display_name().empty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostRequestHandlerTest, HostIsMovedIntoGroupOfItsWorkspace)
{
    const qint64 group_id = addGroup(workspace_id_, "servers");
    ASSERT_GT(group_id, 0);

    const RequestResult result = handle(makeRequest(host_id_, group_id, "display"));

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

    const RequestResult result =
        handle(makeRequest(host_id_, foreign_group, "display"));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(findHost(host_id_).group_id(), 0);
    EXPECT_TRUE(findHost(host_id_).display_name().empty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostRequestHandlerTest, UnknownGroupIsRejected)
{
    const RequestResult result = handle(makeRequest(host_id_, 12345, "display"));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(findHost(host_id_).group_id(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostRequestHandlerTest, NegativeGroupIsRejected)
{
    const RequestResult result = handle(makeRequest(host_id_, -1, "display"));

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(findHost(host_id_).group_id(), 0);
}

// The host lists and the search of the client channel: what every session type is allowed to see.
class HostListTest : public RouterTestBase
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

    // A host assigned to the given workspace and group.
    HostId addHostTo(std::string_view key_hash, qint64 workspace_id, qint64 group_id,
                     std::string_view display_name)
    {
        const HostId host_id = addHost(key_hash);
        if (host_id == kInvalidHostId)
            return kInvalidHostId;

        // Every host of the workspace, whatever group it sits in: the save carries the complete
        // final set, and a host missing from it would be released.
        std::set<HostId> hosts;
        proto::router::HostList list;
        db_.hosts(0, proto::router::kMaxHostPageSize, &list);
        for (int i = 0; i < list.host_size(); ++i)
        {
            if (list.host(i).workspace_id() == workspace_id)
                hosts.insert(list.host(i).host_id());
        }
        hosts.insert(host_id);

        // The complete final set of the workspace, as a save from the console would send it.
        proto::router::WorkspaceList workspaces;
        db_.workspaceListWithAllAccess(admin_.entry_id, workspace_id, &workspaces);
        if (workspaces.workspace_size() != 1)
            return kInvalidHostId;

        if (db_.modifyWorkspace(workspace_id, workspaces.workspace(0).revision(),
                                workspaces.workspace(0).name(), std::string_view(),
                                {accessEntry(admin_, gk_)}, hosts) != proto::router::kErrorOk)
        {
            return kInvalidHostId;
        }

        if (!db_.modifyHost(host_id, group_id, display_name, std::string_view(),
                            std::string_view(), std::string_view()))
        {
            return kInvalidHostId;
        }

        return host_id;
    }

    proto::router::HostListRequest hostListRequest(proto::router::HostListRequest::Mode mode,
                                                   qint64 workspace_id, qint64 group_id)
    {
        proto::router::HostListRequest request;
        request.set_mode(mode);
        request.set_workspace_id(workspace_id);
        request.set_group_id(group_id);
        // The page is mandatory; a test that is not about paging asks for the largest one.
        request.set_offset(0);
        request.set_count(proto::router::kMaxHostPageSize);
        return request;
    }

    proto::router::HostList hostList(const proto::router::HostListRequest& request)
    {
        proto::router::HostList out;
        handleHostList(db_, caller_, request, &out);
        return out;
    }

    proto::router::HostSearchResult searchHosts(const QString& query)
    {
        proto::router::HostSearchRequest request;
        request.set_query(query.toStdString());
        // The page is mandatory; a test that is not about paging asks for the largest one.
        request.set_offset(0);
        request.set_count(proto::router::kMaxHostPageSize);

        proto::router::HostSearchResult out;
        handleHostSearch(db_, caller_, request, &out);
        return out;
    }

    SecureByteArray gk_;
    qint64 workspace_id_ = 0;
};

//--------------------------------------------------------------------------------------------------
// The unfiltered list is the admin view of the whole installation: every host, assigned or not.
TEST_F(HostListTest, AdminSeesEveryHostInModeAll)
{
    ASSERT_NE(addHostTo("hash-1", workspace_id_, 0, "first"), kInvalidHostId);
    ASSERT_NE(addHost("hash-2"), kInvalidHostId);

    const proto::router::HostList list =
        hostList(hostListRequest(proto::router::HostListRequest::MODE_ALL, 0, 0));

    EXPECT_EQ(list.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(list.host_size(), 2);
    EXPECT_EQ(list.total_count(), 2);
}

//--------------------------------------------------------------------------------------------------
// The unfiltered list ignores workspace membership, so only an administrator may ask for it.
TEST_F(HostListTest, NonAdminCannotUseModeAll)
{
    const RouterUser client = addUser("client",
                                      proto::router::SESSION_TYPE_CLIENT);
    ASSERT_TRUE(client.isValid());
    setCaller(client, proto::router::SESSION_TYPE_CLIENT);

    const proto::router::HostList list =
        hostList(hostListRequest(proto::router::HostListRequest::MODE_ALL, 0, 0));

    EXPECT_EQ(list.error_code(), proto::router::kErrorAccessDenied);
    EXPECT_EQ(list.host_size(), 0);
    EXPECT_EQ(list.total_count(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostListTest, FilteredListRequiresWorkspaceAccess)
{
    const RouterUser client = addUser("client",
                                      proto::router::SESSION_TYPE_CLIENT);
    ASSERT_TRUE(client.isValid());
    ASSERT_NE(addHostTo("hash-1", workspace_id_, 0, "first"), kInvalidHostId);

    setCaller(client, proto::router::SESSION_TYPE_CLIENT);

    const proto::router::HostList list = hostList(
        hostListRequest(proto::router::HostListRequest::MODE_FILTERED, workspace_id_, 0));

    EXPECT_EQ(list.error_code(), proto::router::kErrorAccessDenied);
    EXPECT_EQ(list.host_size(), 0);

    // The reply still echoes the selection so the client can route it.
    EXPECT_EQ(list.workspace_id(), workspace_id_);
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostListTest, FilteredListIsScopedToWorkspaceAndGroup)
{
    qint64 group_id = -1;
    ASSERT_EQ(db_.addGroup(workspace_id_, 0, "servers", std::string_view(), &group_id),
              proto::router::kErrorOk);

    ASSERT_NE(addHostTo("hash-1", workspace_id_, group_id, "in-group"), kInvalidHostId);
    ASSERT_NE(addHostTo("hash-2", workspace_id_, 0, "at-root"), kInvalidHostId);
    ASSERT_NE(addHost("hash-3"), kInvalidHostId); // Unassigned.

    const proto::router::HostList in_group = hostList(
        hostListRequest(proto::router::HostListRequest::MODE_FILTERED, workspace_id_, group_id));

    ASSERT_EQ(in_group.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(in_group.host_size(), 1);
    EXPECT_EQ(in_group.host(0).display_name(), "in-group");
    EXPECT_EQ(in_group.total_count(), 1);
    EXPECT_EQ(in_group.group_id(), group_id);

    const proto::router::HostList at_root = hostList(
        hostListRequest(proto::router::HostListRequest::MODE_FILTERED, workspace_id_, 0));

    ASSERT_EQ(at_root.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(at_root.host_size(), 1);
    EXPECT_EQ(at_root.host(0).display_name(), "at-root");
}

//--------------------------------------------------------------------------------------------------
// The page is the window the client asked for, but the count is the whole scope - that is what
// drives its pagination.
TEST_F(HostListTest, PaginationReturnsWindowAndFullCount)
{
    ASSERT_NE(addHostTo("hash-1", workspace_id_, 0, "first"), kInvalidHostId);
    ASSERT_NE(addHostTo("hash-2", workspace_id_, 0, "second"), kInvalidHostId);
    ASSERT_NE(addHostTo("hash-3", workspace_id_, 0, "third"), kInvalidHostId);

    proto::router::HostListRequest request =
        hostListRequest(proto::router::HostListRequest::MODE_ALL, 0, 0);
    request.set_offset(1);
    request.set_count(2);

    const proto::router::HostList list = hostList(request);

    EXPECT_EQ(list.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(list.host_size(), 2);
    EXPECT_EQ(list.total_count(), 3);
}

//--------------------------------------------------------------------------------------------------
// A malformed or oversized page is refused instead of being clamped, and the count of the failed
// reply is dropped with the list.
TEST_F(HostListTest, InvalidPaginationIsRejectedWithoutCount)
{
    ASSERT_NE(addHostTo("hash-1", workspace_id_, 0, "first"), kInvalidHostId);

    proto::router::HostListRequest request =
        hostListRequest(proto::router::HostListRequest::MODE_ALL, 0, 0);
    request.set_offset(-1);
    request.set_count(10);

    const proto::router::HostList negative = hostList(request);
    EXPECT_EQ(negative.error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(negative.host_size(), 0);
    EXPECT_EQ(negative.total_count(), 0);

    request.set_offset(0);
    request.set_count(proto::router::kMaxHostPageSize + 1);

    const proto::router::HostList huge = hostList(request);
    EXPECT_EQ(huge.error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(huge.total_count(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostListTest, UnknownHostListModeIsInvalidRequest)
{
    const proto::router::HostList list =
        hostList(hostListRequest(proto::router::HostListRequest::MODE_UNKNOWN, 0, 0));

    EXPECT_EQ(list.error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(list.host_size(), 0);
}

//--------------------------------------------------------------------------------------------------
// A request that names no page is refused in either mode. The whole point of the page is that the
// size of a reply never follows the size of the database: a reply the channel cannot carry is not
// sent but ends the session, so a client that forgot to page has to hear about it.
TEST_F(HostListTest, ListWithoutAPageIsRejected)
{
    ASSERT_NE(addHostTo("hash-1", workspace_id_, 0, "first"), kInvalidHostId);

    for (const auto mode : { proto::router::HostListRequest::MODE_ALL,
                             proto::router::HostListRequest::MODE_FILTERED })
    {
        proto::router::HostListRequest request = hostListRequest(mode, workspace_id_, 0);
        request.set_offset(0);
        request.set_count(0);

        const proto::router::HostList list = hostList(request);

        EXPECT_EQ(list.error_code(), proto::router::kErrorInvalidRequest) << mode;
        EXPECT_EQ(list.host_size(), 0) << mode;
        EXPECT_EQ(list.total_count(), 0) << mode;
    }
}

//--------------------------------------------------------------------------------------------------
// A full page of hosts whose every field sits on its limit still fits what the channel can carry.
// This is what picks the value of kMaxHostPageSize, and it is the assumption that quietly breaks
// once a field is added to a host record or a field limit is raised.
TEST_F(HostListTest, FullPageOfLargestHostsFitsTheChannel)
{
    // Written straight into the table: the point is the size of the reply, not the path the rows
    // took to get there.
    const QString sql = QString(
        "INSERT INTO hosts (id, key, hwid, workspace_id, group_id, display_name, computer_name, "
        "cpu_arch, version, os_name, address, comment, user_name, password) "
        "WITH RECURSIVE seq(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM seq WHERE n < %1) "
        "SELECT NULL, randomblob(64), 'hwid', %2, 0, "
        "substr(hex(zeroblob(64)),1,%3), substr(hex(zeroblob(64)),1,64), "
        "substr(hex(zeroblob(32)),1,32), '3.0.0.0', substr(hex(zeroblob(64)),1,64), "
        "'255.255.255.255', zeroblob(%4), zeroblob(%5), zeroblob(%5) FROM seq")
        .arg(proto::router::kMaxHostPageSize).arg(workspace_id_)
        .arg(proto::router::kMaxEntryNameLength).arg(proto::router::kMaxCommentLength).arg(proto::router::kMaxCredentialLength);

    ASSERT_TRUE(execRaw(sql));

    proto::router::HostListRequest request =
        hostListRequest(proto::router::HostListRequest::MODE_FILTERED, workspace_id_, 0);

    proto::router::RouterToClient message;
    proto::router::HostList* list = message.mutable_host_list();
    handleHostList(db_, caller_, request, list);

    ASSERT_EQ(list->error_code(), proto::router::kErrorOk);
    ASSERT_EQ(list->host_size(), proto::router::kMaxHostPageSize);

    EXPECT_LE(serialize(message).size(), qsizetype(TcpChannel::kMaxMessageSize));
}

//--------------------------------------------------------------------------------------------------
// Search is scoped to the workspaces of the caller regardless of its session type: a host of a
// workspace the user is not a member of must not surface through it.
TEST_F(HostListTest, SearchIsScopedToAccessibleWorkspaces)
{
    const RouterUser client = addUser("client",
                                      proto::router::SESSION_TYPE_CLIENT);
    ASSERT_TRUE(client.isValid());

    const qint64 other_id = addWorkspace("beta", gk_);
    ASSERT_GT(other_id, 0);

    ASSERT_NE(addHostTo("hash-1", workspace_id_, 0, "alpha-host"), kInvalidHostId);
    ASSERT_NE(addHostTo("hash-2", other_id, 0, "beta-host"), kInvalidHostId);

    // The admin is a member of both.
    const proto::router::HostSearchResult admin_result = searchHosts("host");
    EXPECT_EQ(admin_result.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(admin_result.host_size(), 2);

    // A user without any membership sees nothing, and that is not an error.
    setCaller(client, proto::router::SESSION_TYPE_CLIENT);

    const proto::router::HostSearchResult client_result = searchHosts("host");
    EXPECT_EQ(client_result.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(client_result.host_size(), 0);
}

//--------------------------------------------------------------------------------------------------
// The operator can search by the number they dial as well as by the label.
TEST_F(HostListTest, SearchMatchesDisplayNameAndHostId)
{
    const HostId host_id = addHostTo("hash-1", workspace_id_, 0, "Accounting");
    ASSERT_NE(host_id, kInvalidHostId);

    const proto::router::HostSearchResult by_name = searchHosts("count");
    ASSERT_EQ(by_name.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(by_name.host_size(), 1);
    EXPECT_EQ(by_name.host(0).host_id(), host_id);

    const proto::router::HostSearchResult by_id =
        searchHosts(QString::number(static_cast<quint64>(host_id)));
    ASSERT_EQ(by_id.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(by_id.host_size(), 1);
    EXPECT_EQ(by_id.host(0).host_id(), host_id);
}

//--------------------------------------------------------------------------------------------------
// The search answers with the page that was asked for and with the number of matches in the whole
// scope, so the client can walk the rest instead of being handed a silently cut list.
TEST_F(HostListTest, SearchReturnsWindowAndFullCount)
{
    ASSERT_NE(addHostTo("hash-1", workspace_id_, 0, "host-a"), kInvalidHostId);
    ASSERT_NE(addHostTo("hash-2", workspace_id_, 0, "host-b"), kInvalidHostId);
    ASSERT_NE(addHostTo("hash-3", workspace_id_, 0, "host-c"), kInvalidHostId);

    proto::router::HostSearchRequest request;
    request.set_query("host");
    request.set_offset(1);
    request.set_count(2);

    proto::router::HostSearchResult result;
    handleHostSearch(db_, caller_, request, &result);

    EXPECT_EQ(result.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(result.host_size(), 2);
    EXPECT_EQ(result.host(0).display_name(), "host-b");
    EXPECT_EQ(result.host(1).display_name(), "host-c");
    EXPECT_EQ(result.total_count(), 3);
}

//--------------------------------------------------------------------------------------------------
// The search is bounded by the same rule as the list, and a refused request carries neither
// matches nor a count.
TEST_F(HostListTest, SearchWithAnInvalidPageIsRejected)
{
    ASSERT_NE(addHostTo("hash-1", workspace_id_, 0, "host-a"), kInvalidHostId);

    proto::router::HostSearchRequest request;
    request.set_query("host");

    proto::router::HostSearchResult no_page;
    handleHostSearch(db_, caller_, request, &no_page);

    EXPECT_EQ(no_page.error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(no_page.host_size(), 0);
    EXPECT_EQ(no_page.total_count(), 0);

    request.set_offset(0);
    request.set_count(proto::router::kMaxHostPageSize + 1);

    proto::router::HostSearchResult huge;
    handleHostSearch(db_, caller_, request, &huge);

    EXPECT_EQ(huge.error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(huge.total_count(), 0);
}
