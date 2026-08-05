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

#include "router/client_channel_handler.h"

#include <unordered_map>

#include "base/serialization.h"
#include "base/net/tcp_channel.h"
#include "router/router_test_base.h"
#include "router/workers/client_worker.h"

// The client channel of every session type against a real database: the lists a session is allowed
// to see and the rotation of its own password.
class ClientChannelHandlerTest : public RouterTestBase
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

    // A session of the given user and type, in place of the built-in administrator.
    void setCaller(const RouterUser& user, proto::router::SessionType session_type)
    {
        caller_.user_id = user.entry_id;
        caller_.name = user.name;
        caller_.session_type = session_type;
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
        ClientChannelHandler::handleHostList(db_, caller_, request, &out);
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
        ClientChannelHandler::handleHostSearch(db_, caller_, request, &out);
        return out;
    }

    proto::router::WorkspaceList workspaceList(qint64 workspace_id)
    {
        proto::router::WorkspaceListRequest request;
        request.set_workspace_id(workspace_id);

        proto::router::WorkspaceList out;
        ClientChannelHandler::handleWorkspaceList(db_, caller_, request, &out);
        return out;
    }

    proto::router::GroupList groupList(qint64 workspace_id)
    {
        proto::router::GroupListRequest request;
        request.set_workspace_id(workspace_id);

        proto::router::GroupList out;
        ClientChannelHandler::handleGroupList(db_, caller_, request, &out);
        return out;
    }

    SecureByteArray gk_;
    qint64 workspace_id_ = 0;
};

//--------------------------------------------------------------------------------------------------
// The unfiltered list is the admin view of the whole installation: every host, assigned or not.
TEST_F(ClientChannelHandlerTest, AdminSeesEveryHostInModeAll)
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
TEST_F(ClientChannelHandlerTest, NonAdminCannotUseModeAll)
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
TEST_F(ClientChannelHandlerTest, FilteredListRequiresWorkspaceAccess)
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
TEST_F(ClientChannelHandlerTest, FilteredListIsScopedToWorkspaceAndGroup)
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
TEST_F(ClientChannelHandlerTest, PaginationReturnsWindowAndFullCount)
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
TEST_F(ClientChannelHandlerTest, InvalidPaginationIsRejectedWithoutCount)
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
TEST_F(ClientChannelHandlerTest, UnknownHostListModeIsInvalidRequest)
{
    const proto::router::HostList list =
        hostList(hostListRequest(proto::router::HostListRequest::MODE_UNKNOWN, 0, 0));

    EXPECT_EQ(list.error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(list.host_size(), 0);
}

//--------------------------------------------------------------------------------------------------
// Search is scoped to the workspaces of the caller regardless of its session type: a host of a
// workspace the user is not a member of must not surface through it.
TEST_F(ClientChannelHandlerTest, SearchIsScopedToAccessibleWorkspaces)
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
TEST_F(ClientChannelHandlerTest, SearchMatchesDisplayNameAndHostId)
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
// A request that names no page is refused in either mode. The whole point of the page is that the
// size of a reply never follows the size of the database: a reply the channel cannot carry is not
// sent but ends the session, so a client that forgot to page has to hear about it.
TEST_F(ClientChannelHandlerTest, ListWithoutAPageIsRejected)
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
TEST_F(ClientChannelHandlerTest, FullPageOfLargestHostsFitsTheChannel)
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
        .arg(kMaxEntryNameLength).arg(kMaxCommentLength).arg(kMaxCredentialLength);

    ASSERT_TRUE(execRaw(sql));

    proto::router::HostListRequest request =
        hostListRequest(proto::router::HostListRequest::MODE_FILTERED, workspace_id_, 0);

    proto::router::RouterToClient message;
    proto::router::HostList* list = message.mutable_host_list();
    ClientChannelHandler::handleHostList(db_, caller_, request, list);

    ASSERT_EQ(list->error_code(), proto::router::kErrorOk);
    ASSERT_EQ(list->host_size(), proto::router::kMaxHostPageSize);

    EXPECT_LE(serialize(message).size(), qsizetype(TcpChannel::kMaxMessageSize));
}

//--------------------------------------------------------------------------------------------------
// The search answers with the page that was asked for and with the number of matches in the whole
// scope, so the client can walk the rest instead of being handed a silently cut list.
TEST_F(ClientChannelHandlerTest, SearchReturnsWindowAndFullCount)
{
    ASSERT_NE(addHostTo("hash-1", workspace_id_, 0, "host-a"), kInvalidHostId);
    ASSERT_NE(addHostTo("hash-2", workspace_id_, 0, "host-b"), kInvalidHostId);
    ASSERT_NE(addHostTo("hash-3", workspace_id_, 0, "host-c"), kInvalidHostId);

    proto::router::HostSearchRequest request;
    request.set_query("host");
    request.set_offset(1);
    request.set_count(2);

    proto::router::HostSearchResult result;
    ClientChannelHandler::handleHostSearch(db_, caller_, request, &result);

    EXPECT_EQ(result.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(result.host_size(), 2);
    EXPECT_EQ(result.host(0).display_name(), "host-b");
    EXPECT_EQ(result.host(1).display_name(), "host-c");
    EXPECT_EQ(result.total_count(), 3);
}

//--------------------------------------------------------------------------------------------------
// The search is bounded by the same rule as the list, and a refused request carries neither
// matches nor a count.
TEST_F(ClientChannelHandlerTest, SearchWithAnInvalidPageIsRejected)
{
    ASSERT_NE(addHostTo("hash-1", workspace_id_, 0, "host-a"), kInvalidHostId);

    proto::router::HostSearchRequest request;
    request.set_query("host");

    proto::router::HostSearchResult no_page;
    ClientChannelHandler::handleHostSearch(db_, caller_, request, &no_page);

    EXPECT_EQ(no_page.error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(no_page.host_size(), 0);
    EXPECT_EQ(no_page.total_count(), 0);

    request.set_offset(0);
    request.set_count(proto::router::kMaxHostPageSize + 1);

    proto::router::HostSearchResult huge;
    ClientChannelHandler::handleHostSearch(db_, caller_, request, &huge);

    EXPECT_EQ(huge.error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(huge.total_count(), 0);
}

//--------------------------------------------------------------------------------------------------
// An administrator manages membership, so it gets every member's access entry.
TEST_F(ClientChannelHandlerTest, AdminSeesFullMembership)
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
TEST_F(ClientChannelHandlerTest, NonAdminSeesOnlyOwnAccessEntry)
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
TEST_F(ClientChannelHandlerTest, InvisibleWorkspaceIsNotListed)
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
TEST_F(ClientChannelHandlerTest, WorkspaceListNarrowsToRequestedId)
{
    const qint64 other_id = addWorkspace("beta", gk_);
    ASSERT_GT(other_id, 0);

    EXPECT_EQ(workspaceList(0).workspace_size(), 2);

    const proto::router::WorkspaceList single = workspaceList(other_id);
    ASSERT_EQ(single.workspace_size(), 1);
    EXPECT_EQ(single.workspace(0).entry_id(), other_id);
}

//--------------------------------------------------------------------------------------------------
TEST_F(ClientChannelHandlerTest, GroupListIsScopedToItsWorkspace)
{
    const qint64 other_id = addWorkspace("beta", gk_);
    ASSERT_GT(other_id, 0);

    qint64 group_id = -1;
    ASSERT_EQ(db_.addGroup(workspace_id_, 0, "servers", std::string_view(), &group_id),
              proto::router::kErrorOk);
    ASSERT_EQ(db_.addGroup(other_id, 0, "foreign", std::string_view(), &group_id),
              proto::router::kErrorOk);

    const proto::router::GroupList list = groupList(workspace_id_);

    ASSERT_EQ(list.error_code(), proto::router::kErrorOk);
    ASSERT_EQ(list.group_size(), 1);
    EXPECT_EQ(list.group(0).name(), "servers");
    EXPECT_EQ(list.workspace_id(), workspace_id_);
}

//--------------------------------------------------------------------------------------------------
TEST_F(ClientChannelHandlerTest, GroupListRequiresWorkspaceAccess)
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
TEST_F(ClientChannelHandlerTest, GroupListOfInvalidWorkspaceIsInvalidRequest)
{
    const proto::router::GroupList list = groupList(0);

    EXPECT_EQ(list.error_code(), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(list.group_size(), 0);
}

//--------------------------------------------------------------------------------------------------
// The rotation replaces the credentials, revokes every device token issued against the old ones
// and re-wraps the workspace keys - all in one transaction.
TEST_F(ClientChannelHandlerTest, ChangePasswordRotatesCredentialsAndRevokesTokens)
{
    std::string token;
    qint64 token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &token, &token_id));

    const RouterUser rotated = makeUser("admin", kAllSessions);

    proto::router::ChangePasswordRequest request;
    request.set_salt(toStdString(rotated.salt));
    request.set_verifier(toStdString(rotated.verifier));
    request.set_public_key(toStdString(rotated.public_key));
    request.set_wrap_private_key(toStdString(rotated.wrap_private_key));
    request.set_wrap_salt(toStdString(rotated.wrap_salt));

    proto::router::ChangePasswordRequest::WorkspaceKey* key = request.add_workspace_key();
    key->set_workspace_id(workspace_id_);
    key->set_wrapped_gk(toStdString(SealedBox::seal(gk_, rotated.public_key)));

    const ClientChannelHandler::PasswordResult result =
        ClientChannelHandler::handleChangePassword(db_, caller_, request);

    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_USERS));

    const RouterUser stored = db_.findUser(admin_.entry_id);
    EXPECT_EQ(stored.verifier, rotated.verifier);
    EXPECT_EQ(stored.public_key, rotated.public_key);

    // The name, the session mask and the flags of the record are not the caller's to change here.
    EXPECT_EQ(stored.name, admin_.name);
    EXPECT_EQ(stored.sessions, admin_.sessions);
    EXPECT_EQ(stored.flags, admin_.flags);

    std::vector<DeviceToken> tokens;
    ASSERT_TRUE(db_.listClientDeviceTokens(admin_.entry_id, &tokens));
    EXPECT_TRUE(tokens.empty());
}

//--------------------------------------------------------------------------------------------------
// The second factor is not derived from the password: a rotation must leave the enrollment alone,
// otherwise every password change would silently drop the user back to a fresh TOTP enrollment.
TEST_F(ClientChannelHandlerTest, ChangePasswordKeepsOtpEnrollment)
{
    const QByteArray secret = Random::byteArray(32);
    ASSERT_TRUE(db_.setUserOtp(admin_.entry_id, secret, 100));

    const RouterUser rotated = makeUser("admin", kAllSessions);

    proto::router::ChangePasswordRequest request;
    request.set_salt(toStdString(rotated.salt));
    request.set_verifier(toStdString(rotated.verifier));
    request.set_public_key(toStdString(rotated.public_key));
    request.set_wrap_private_key(toStdString(rotated.wrap_private_key));
    request.set_wrap_salt(toStdString(rotated.wrap_salt));

    proto::router::ChangePasswordRequest::WorkspaceKey* key = request.add_workspace_key();
    key->set_workspace_id(workspace_id_);
    key->set_wrapped_gk(toStdString(SealedBox::seal(gk_, rotated.public_key)));

    ASSERT_EQ(ClientChannelHandler::handleChangePassword(db_, caller_, request).error_code,
              proto::router::kErrorOk);

    const RouterUser stored = db_.findUser(admin_.entry_id);
    EXPECT_EQ(stored.otp_secret, secret);
    EXPECT_EQ(stored.otp_counter, 100u);
}

//--------------------------------------------------------------------------------------------------
// Without a re-sealed key for every workspace the user can access the rotation would lock it out
// of them, so nothing is applied - the credentials and the tokens survive intact.
TEST_F(ClientChannelHandlerTest, ChangePasswordWithoutKeysIsConflict)
{
    std::string token;
    qint64 token_id = 0;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &token, &token_id));

    const RouterUser rotated = makeUser("admin", kAllSessions);

    proto::router::ChangePasswordRequest request;
    request.set_salt(toStdString(rotated.salt));
    request.set_verifier(toStdString(rotated.verifier));
    request.set_public_key(toStdString(rotated.public_key));
    request.set_wrap_private_key(toStdString(rotated.wrap_private_key));
    request.set_wrap_salt(toStdString(rotated.wrap_salt));

    const ClientChannelHandler::PasswordResult result =
        ClientChannelHandler::handleChangePassword(db_, caller_, request);

    EXPECT_EQ(result.error_code, proto::router::kErrorConflict);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(db_.findUser(admin_.entry_id).verifier, admin_.verifier);

    std::vector<DeviceToken> tokens;
    ASSERT_TRUE(db_.listClientDeviceTokens(admin_.entry_id, &tokens));
    EXPECT_EQ(tokens.size(), 1u);
}

//--------------------------------------------------------------------------------------------------
TEST_F(ClientChannelHandlerTest, ChangePasswordRejectsInvalidCredentials)
{
    proto::router::ChangePasswordRequest request; // No credentials at all.

    const ClientChannelHandler::PasswordResult result =
        ClientChannelHandler::handleChangePassword(db_, caller_, request);

    EXPECT_EQ(result.error_code, proto::router::kErrorInvalidData);
    EXPECT_EQ(db_.findUser(admin_.entry_id).verifier, admin_.verifier);
}

//--------------------------------------------------------------------------------------------------
// The account was deleted while its session was live: the answer is the same one the modify path
// gives a moment later.
TEST_F(ClientChannelHandlerTest, ChangePasswordOfDeletedUserIsNotFound)
{
    const RouterUser client = addUser("client",
                                      proto::router::SESSION_TYPE_CLIENT);
    ASSERT_TRUE(client.isValid());
    setCaller(client, proto::router::SESSION_TYPE_CLIENT);
    ASSERT_EQ(db_.removeUser(client.entry_id), proto::router::kErrorOk);

    const RouterUser rotated = makeUser("client",
                                        proto::router::SESSION_TYPE_CLIENT);

    proto::router::ChangePasswordRequest request;
    request.set_salt(toStdString(rotated.salt));
    request.set_verifier(toStdString(rotated.verifier));
    request.set_public_key(toStdString(rotated.public_key));
    request.set_wrap_private_key(toStdString(rotated.wrap_private_key));
    request.set_wrap_salt(toStdString(rotated.wrap_salt));

    const ClientChannelHandler::PasswordResult result =
        ClientChannelHandler::handleChangePassword(db_, caller_, request);

    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(result.notify_flags, 0u);
}
