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

#include "client/router.h"

#include <gtest/gtest.h>

#include <QObject>

#include "client/router_test_fixture.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"

namespace {

constexpr qint64 kRouterId = 1;
constexpr qint64 kWorkspaceId = 10;

} // namespace

// Test-only access to the identity, the pending replies and the incoming messages.
class RouterTestPeer
{
public:
    static RouterRpc& rpc(Router& router) { return router.rpc_; }

    static void receive(Router& router, quint8 channel_id, const QByteArray& bytes)
    {
        router.onTcpMessageReceived(router.routerId(), channel_id, bytes);
    }

    // ONLINE is reached through the login conversation, which a test does not run.
    static void setStatus(Router& router, Router::Status status) { router.setStatus(status); }
};

// A session without a worker: what it sends is collected from sig_sendMessage, the replies are
// handed to it as the worker would. The cache and the rpc have tests of their own; here the
// conversation is under test.
class RouterTest : public RouterTestFixture
{
protected:
    RouterTest()
        : router_(config())
    {
        QObject::connect(&router_, &Router::sig_sendMessage,
                         [this](qint64, quint8 channel_id, const QByteArray& buffer)
        {
            sent_.append({ channel_id, buffer });
        });
    }

    static RouterConfig config()
    {
        RouterConfig config;
        config.setRouterId(kRouterId);
        return config;
    }

    // Hands a message to the session the way the worker does.
    void deliver(quint8 channel_id, const google::protobuf::MessageLite& message)
    {
        RouterTestPeer::receive(router_, channel_id,
                                QByteArray::fromStdString(message.SerializeAsString()));
    }

    // The request that went out last, parsed back from the bytes.
    template<typename MessageT>
    MessageT lastRequest()
    {
        MessageT message;
        EXPECT_FALSE(sent_.isEmpty());
        if (!sent_.isEmpty())
        {
            const QByteArray& buffer = sent_.constLast().second;
            EXPECT_TRUE(message.ParseFromArray(buffer.data(), buffer.size()));
        }
        return message;
    }

    // A workspace list as the router builds it for an admin session: every workspace carries its
    // membership.
    static proto::router::WorkspaceList workspaceList(const QList<qint64>& workspace_ids,
                                                      const QString& comment = QString())
    {
        proto::router::WorkspaceList list;
        list.set_error_code(proto::router::kErrorOk);

        for (qint64 workspace_id : workspace_ids)
        {
            proto::router::Workspace* workspace = list.add_workspace();
            workspace->set_entry_id(workspace_id);
            workspace->set_name("workspace");
            workspace->set_revision(1);
            workspace->set_comment(comment.toStdString());
            workspace->add_user_id(kUserId);
        }

        return list;
    }

    // The filtered query of one workspace: the one kind of host list that is cached.
    static proto::router::HostListRequest hostListRequest()
    {
        proto::router::HostListRequest request;
        request.set_mode(proto::router::HostListRequest::MODE_FILTERED);
        request.set_workspace_id(kWorkspaceId);
        request.set_count(100);
        return request;
    }

    // Asks for a list and answers it the way the router would: the reply carries the id of the
    // request that just went out.
    RouterWorkspaceList fetchWorkspaces(qint64 workspace_id, proto::router::WorkspaceList list)
    {
        RouterWorkspaceList delivered;
        auto store = [&delivered](const RouterWorkspaceList& value) { delivered = value; };
        router_.listWorkspaces(Router::CachePolicy::RELOAD, workspace_id, { &receiver_, store });

        const auto request = lastRequest<proto::router::ClientToRouter>();
        EXPECT_TRUE(request.has_workspace_list_request());
        list.set_request_id(request.workspace_list_request().request_id());

        proto::router::RouterToClient reply;
        reply.mutable_workspace_list()->Swap(&list);
        deliver(proto::router::CHANNEL_ID_CLIENT, reply);

        return delivered;
    }

    RouterHostList fetchHosts(proto::router::HostList list)
    {
        RouterHostList delivered;
        auto store = [&delivered](const RouterHostList& value) { delivered = value; };
        router_.listHosts(Router::CachePolicy::RELOAD, hostListRequest(), { &receiver_, store });

        const auto request = lastRequest<proto::router::ClientToRouter>();
        EXPECT_TRUE(request.has_host_list_request());
        list.set_request_id(request.host_list_request().request_id());

        proto::router::RouterToClient reply;
        reply.mutable_host_list()->Swap(&list);
        deliver(proto::router::CHANNEL_ID_CLIENT, reply);

        return delivered;
    }

    RouterGroupList fetchGroups(qint64 workspace_id, proto::router::GroupList list)
    {
        RouterGroupList delivered;
        auto store = [&delivered](const RouterGroupList& value) { delivered = value; };
        router_.listGroups(Router::CachePolicy::RELOAD, workspace_id, { &receiver_, store });

        const auto request = lastRequest<proto::router::ClientToRouter>();
        EXPECT_TRUE(request.has_group_list_request());
        list.set_request_id(request.group_list_request().request_id());

        proto::router::RouterToClient reply;
        reply.mutable_group_list()->Swap(&list);
        deliver(proto::router::CHANNEL_ID_CLIENT, reply);

        return delivered;
    }

    // A cached list is one the session answers on its own: nothing new reaches the wire.
    bool workspacesServedFromCache()
    {
        const int sent_before = sent_.size();
        router_.listWorkspaces(Router::CachePolicy::USE_CACHE, 0,
                               { &receiver_, [](const RouterWorkspaceList&) {} });
        return sent_.size() == sent_before;
    }

    bool groupsServedFromCache(qint64 workspace_id = kWorkspaceId)
    {
        const int sent_before = sent_.size();
        router_.listGroups(Router::CachePolicy::USE_CACHE, workspace_id,
                           { &receiver_, [](const RouterGroupList&) {} });
        return sent_.size() == sent_before;
    }

    bool hostsServedFromCache()
    {
        const int sent_before = sent_.size();
        router_.listHosts(Router::CachePolicy::USE_CACHE, hostListRequest(),
                          { &receiver_, [](const RouterHostList&) {} });
        return sent_.size() == sent_before;
    }

    // Puts one entry in every cache, so what a reply drops can be seen by what is left.
    void fillCaches()
    {
        fetchWorkspaces(0, workspaceList({ kWorkspaceId }));

        proto::router::GroupList groups;
        groups.set_error_code(proto::router::kErrorOk);
        groups.set_workspace_id(kWorkspaceId);
        fetchGroups(kWorkspaceId, groups);

        fetchHosts(hostList(kWorkspaceId, { HostId(1) }, 1));

        ASSERT_TRUE(workspacesServedFromCache());
        ASSERT_TRUE(groupsServedFromCache());
        ASSERT_TRUE(hostsServedFromCache());
    }

    static proto::router::RouterToAdmin workspaceResult(const char* command, const char* error_code)
    {
        proto::router::RouterToAdmin message;
        proto::router::WorkspaceResult* result = message.mutable_workspace_result();
        result->set_command_name(command);
        result->set_error_code(error_code);
        return message;
    }

    static proto::router::RouterToManager groupResult(const char* command, const char* error_code)
    {
        proto::router::RouterToManager message;
        proto::router::GroupResult* result = message.mutable_group_result();
        result->set_command_name(command);
        result->set_error_code(error_code);
        return message;
    }

    // What the session sent, as it goes to the worker: the channel and the serialized message.
    QList<QPair<quint8, QByteArray>> sent_;
    QObject receiver_;
    Router router_;
};

//--------------------------------------------------------------------------------------------------
// The list arrives as the router stored it, membership included.
TEST_F(RouterTest, WorkspaceListIsParsed)
{
    const RouterWorkspaceList workspaces = fetchWorkspaces(0, workspaceList({10}, "note"));

    ASSERT_EQ(workspaces.error_code, QString::fromStdString(proto::router::kErrorOk));
    ASSERT_EQ(workspaces.workspaces.size(), 1);
    EXPECT_EQ(workspaces.workspaces.at(0).comment, "note");
    ASSERT_EQ(workspaces.workspaces.at(0).user_ids.size(), 1);
    EXPECT_EQ(workspaces.workspaces.at(0).user_ids.at(0), kUserId);
}

//--------------------------------------------------------------------------------------------------
// An error reply carries no list at all, so it must not be cached as the answer about what we
// can access.
TEST_F(RouterTest, FailedWorkspaceListChangesNothing)
{
    proto::router::WorkspaceList failed;
    failed.set_error_code(proto::router::kErrorInternalError);
    fetchWorkspaces(0, failed);

    EXPECT_FALSE(workspacesServedFromCache());
}

//--------------------------------------------------------------------------------------------------
// The whole conversation seen from outside: the request leaves for its channel with an id of its
// own, and the reply carrying that id reaches the caller.
TEST_F(RouterTest, ListUsersConversation)
{
    int calls = 0;
    router_.listUsers(0, proto::router::kMaxUserPageSize,
                      { &receiver_, [&calls](const proto::router::UserList&) { ++calls; } });

    ASSERT_EQ(sent_.size(), 1);
    EXPECT_EQ(sent_.at(0).first, proto::router::CHANNEL_ID_ADMIN);

    const auto request = lastRequest<proto::router::AdminToRouter>();
    ASSERT_TRUE(request.has_user_list_request());
    ASSERT_GT(request.user_list_request().request_id(), 0);

    proto::router::RouterToAdmin reply;
    reply.mutable_user_list()->set_request_id(request.user_list_request().request_id());
    reply.mutable_user_list()->set_error_code(proto::router::kErrorOk);
    deliver(proto::router::CHANNEL_ID_ADMIN, reply);

    EXPECT_EQ(calls, 1);
}

//--------------------------------------------------------------------------------------------------
// The same conversation on the manager channel, which carries the records of a workspace.
TEST_F(RouterTest, GroupConversationUsesTheManagerChannel)
{
    RouterGroup group;
    group.name = "servers";

    int calls = 0;
    router_.addGroup(kWorkspaceId, group,
                     { &receiver_, [&calls](const proto::router::GroupResult&) { ++calls; } });

    ASSERT_EQ(sent_.size(), 1);
    EXPECT_EQ(sent_.at(0).first, proto::router::CHANNEL_ID_MANAGER);

    const auto request = lastRequest<proto::router::ManagerToRouter>();
    ASSERT_TRUE(request.has_group_request());
    EXPECT_EQ(request.group_request().command_name(), proto::router::kCommandGroupAdd);

    proto::router::RouterToManager reply;
    reply.mutable_group_result()->set_request_id(request.group_request().request_id());
    reply.mutable_group_result()->set_error_code(proto::router::kErrorOk);
    deliver(proto::router::CHANNEL_ID_MANAGER, reply);

    EXPECT_EQ(calls, 1);
}

//--------------------------------------------------------------------------------------------------
// And on the client channel, where the status of a host is asked before a connection.
TEST_F(RouterTest, HostStatusConversationUsesTheClientChannel)
{
    int calls = 0;
    router_.checkHostStatus(
        HostId(1), { &receiver_, [&calls](const proto::router::HostStatus&) { ++calls; } });

    ASSERT_EQ(sent_.size(), 1);
    EXPECT_EQ(sent_.at(0).first, proto::router::CHANNEL_ID_CLIENT);

    const auto request = lastRequest<proto::router::ClientToRouter>();
    ASSERT_TRUE(request.has_check_host_status());

    proto::router::RouterToClient reply;
    reply.mutable_host_status()->set_request_id(request.check_host_status().request_id());
    reply.mutable_host_status()->set_error_code(proto::router::kErrorOk);
    deliver(proto::router::CHANNEL_ID_CLIENT, reply);

    EXPECT_EQ(calls, 1);
}

//--------------------------------------------------------------------------------------------------
// The connection request names the host, and the offer of the router comes back to the caller.
TEST_F(RouterTest, ConnectionRequestBringsTheOffer)
{
    std::string received_error;
    router_.requestConnection(HostId(7),
        { &receiver_, [&received_error](const proto::router::ConnectionOffer& offer)
    {
        received_error = offer.error_code();
    } });

    const auto request = lastRequest<proto::router::ClientToRouter>();
    ASSERT_TRUE(request.has_connection_request());
    EXPECT_EQ(request.connection_request().host_id(), 7u);

    proto::router::RouterToClient reply;
    proto::router::ConnectionOffer* offer = reply.mutable_connection_offer();
    offer->set_request_id(request.connection_request().request_id());
    offer->set_error_code(proto::router::kErrorOk);
    deliver(proto::router::CHANNEL_ID_CLIENT, reply);

    EXPECT_EQ(received_error, proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
// The caller receives the rows of the reply, and the next one that accepts a cached answer is
// without a request.
TEST_F(RouterTest, HostListIsParsedAndCached)
{
    proto::router::HostList list = hostList(kWorkspaceId, { HostId(1) }, 25);
    list.mutable_host(0)->set_comment("comment");

    const RouterHostList delivered = fetchHosts(list);

    ASSERT_EQ(delivered.hosts.size(), 1);
    EXPECT_EQ(delivered.hosts.at(0).comment, "comment");

    // The count of the whole scope drives the pagination of the client, so it must survive the
    // cache: a cached answer with a zero count would collapse the page list.
    RouterHostList cached;
    const int sent_before = sent_.size();
    router_.listHosts(Router::CachePolicy::USE_CACHE, hostListRequest(),
                      { &receiver_, [&cached](const RouterHostList& value) { cached = value; } });

    EXPECT_EQ(sent_.size(), sent_before);
    ASSERT_EQ(cached.hosts.size(), 1);
    EXPECT_EQ(cached.hosts.at(0).comment, "comment");
    EXPECT_EQ(cached.total_count, 25);
    EXPECT_EQ(cached.workspace_id, kWorkspaceId);
    EXPECT_EQ(cached.error_code, QString::fromStdString(proto::router::kErrorOk));
}

//--------------------------------------------------------------------------------------------------
// The groups arrive with the workspace they belong to and are cached per workspace.
TEST_F(RouterTest, GroupListIsParsedAndCached)
{
    proto::router::GroupList list;
    list.set_error_code(proto::router::kErrorOk);
    list.set_workspace_id(kWorkspaceId);

    proto::router::Group* group = list.add_group();
    group->set_entry_id(5);
    group->set_name("servers");
    group->set_comment("group comment");

    const RouterGroupList groups = fetchGroups(kWorkspaceId, list);

    ASSERT_EQ(groups.groups.size(), 1);
    EXPECT_EQ(groups.groups.at(0).comment, "group comment");
    EXPECT_EQ(groups.groups.at(0).workspace_id, kWorkspaceId);

    EXPECT_TRUE(groupsServedFromCache());
    EXPECT_FALSE(groupsServedFromCache(20));
}

//--------------------------------------------------------------------------------------------------
// A session that leaves ONLINE (a reconnect, or the two-factor stage re-opened after a password
// change) is suspended: nothing it waits for will arrive and the cached lists are no longer known
// to be current. The keys survive - it is being re-authenticated, not lost.
TEST_F(RouterTest, SuspendedSessionDropsPendingRepliesAndCaches)
{
    fillCaches();

    int calls = 0;
    std::string last_error;
    router_.listUsers(0, proto::router::kMaxUserPageSize,
                      { &receiver_, [&](const proto::router::UserList& list)
    {
        ++calls;
        last_error = list.error_code();
    } });

    const auto request = lastRequest<proto::router::AdminToRouter>();
    ASSERT_TRUE(request.has_user_list_request());

    router_.connectToRouter();

    EXPECT_EQ(RouterTestPeer::rpc(router_).pendingCount(), 0);
    EXPECT_FALSE(workspacesServedFromCache());
    EXPECT_FALSE(hostsServedFromCache());

    // The caller is told the answer will never come, once.
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(last_error, proto::router::kErrorLostConnection);

    // A late reply to a request of the dead window must not reach the caller.
    proto::router::RouterToAdmin reply;
    reply.mutable_user_list()->set_request_id(request.user_list_request().request_id());
    deliver(proto::router::CHANNEL_ID_ADMIN, reply);
    EXPECT_EQ(calls, 1);

}

//--------------------------------------------------------------------------------------------------
// A caller answered by the teardown can retry from inside its handler, and a retry that accepts a
// cached answer must not be served the lists of the session that just died: the caches have to be
// gone before the callers are woken.
TEST_F(RouterTest, CallerAnsweredByTeardownDoesNotSeeTheDeadCaches)
{
    fillCaches();

    int wire_requests = 0;
    router_.listUsers(0, proto::router::kMaxUserPageSize,
                      { &receiver_, [&](const proto::router::UserList&)
    {
        const int sent_before = sent_.size();
        router_.listWorkspaces(Router::CachePolicy::USE_CACHE, 0,
                               { &receiver_, [](const RouterWorkspaceList&) {} });
        if (sent_.size() > sent_before)
            ++wire_requests;
    } });

    router_.connectToRouter();

    EXPECT_EQ(wire_requests, 1);
}

//--------------------------------------------------------------------------------------------------
// Until the router accepts our keys it drops everything we send, so a request issued on the way up
// can never be answered. Reaching ONLINE has to wake its caller: a dialog that disabled itself for
// the round trip has nothing else to wait for.
TEST_F(RouterTest, RequestIssuedBeforeTheSessionIsUpIsAnswered)
{
    int calls = 0;
    std::string last_error;
    router_.connectToRouter();
    router_.listUsers(0, proto::router::kMaxUserPageSize,
                      { &receiver_, [&](const proto::router::UserList& list)
    {
        ++calls;
        last_error = list.error_code();
    } });

    RouterTestPeer::setStatus(router_, Router::Status::ONLINE);

    EXPECT_EQ(calls, 1);
    EXPECT_EQ(last_error, proto::router::kErrorLostConnection);
    EXPECT_EQ(RouterTestPeer::rpc(router_).pendingCount(), 0);
}

//--------------------------------------------------------------------------------------------------
// A session that ends keeps nothing waiting: the callers are answered and the session state goes
// with it.
TEST_F(RouterTest, ClearedSessionKeepsNothingPending)
{
    router_.disconnectFromRouter();

    EXPECT_EQ(RouterTestPeer::rpc(router_).pendingCount(), 0);
    EXPECT_EQ(router_.status(), Router::Status::OFFLINE);
}

//--------------------------------------------------------------------------------------------------
// A reply nobody waits for any more (the dialog was closed) still carries its cache rules. What
// the rules are is the business of RouterCache; here every reply channel must feed them.
TEST_F(RouterTest, ReplyWithoutARequestStillAppliesItsRules)
{
    fillCaches();

    deliver(proto::router::CHANNEL_ID_ADMIN,
            workspaceResult(proto::router::kCommandWorkspaceModify, proto::router::kErrorOk));
    EXPECT_FALSE(workspacesServedFromCache());

    fillCaches();

    deliver(proto::router::CHANNEL_ID_MANAGER,
            groupResult(proto::router::kCommandGroupDelete, proto::router::kErrorOk));
    EXPECT_FALSE(groupsServedFromCache());
    EXPECT_FALSE(hostsServedFromCache());
}

//--------------------------------------------------------------------------------------------------
// A change notification is not a reply to anything: it drops the lists it names and raises the
// signal the interface refetches on.
TEST_F(RouterTest, NotificationDropsItsListAndIsAnnounced)
{
    fillCaches();

    int announced = 0;
    QObject::connect(&router_, &Router::sig_hostsChanged, [&announced](qint64) { ++announced; });

    proto::router::RouterToClient message;
    message.mutable_notification()->set_hosts_dirty(true);
    deliver(proto::router::CHANNEL_ID_CLIENT, message);

    EXPECT_EQ(announced, 1);
    EXPECT_FALSE(hostsServedFromCache());
    EXPECT_TRUE(groupsServedFromCache());
}

//--------------------------------------------------------------------------------------------------
// A lookup names the record it wants and asks for no page; the paged call is the other one.
TEST_F(RouterTest, UserLookupNamesTheRecord)
{
    router_.findUser(qint64(42), { &receiver_, [](const proto::router::UserList&) {} });

    auto request = lastRequest<proto::router::AdminToRouter>();
    ASSERT_TRUE(request.has_user_list_request());
    EXPECT_EQ(request.user_list_request().entry_id(), 42);
    EXPECT_EQ(request.user_list_request().count(), 0);

    router_.findUser(QString("bob"), { &receiver_, [](const proto::router::UserList&) {} });

    request = lastRequest<proto::router::AdminToRouter>();
    ASSERT_TRUE(request.has_user_list_request());
    EXPECT_EQ(request.user_list_request().name(), "bob");
    EXPECT_EQ(request.user_list_request().entry_id(), 0);
}

//--------------------------------------------------------------------------------------------------
// The device tokens are a domain of their own: they are listed and revoked by their own messages.
TEST_F(RouterTest, TokensAreListedAndRevokedByTheirOwnMessages)
{
    router_.listUserTokens(7, { &receiver_, [](const proto::router::UserTokenList&) {} });

    auto request = lastRequest<proto::router::AdminToRouter>();
    ASSERT_TRUE(request.has_user_token_list_request());
    EXPECT_EQ(request.user_token_list_request().user_id(), 7);

    router_.revokeUserTokens(7, { 100, 101 },
                             { &receiver_, [](const proto::router::UserTokenResult&) {} });

    request = lastRequest<proto::router::AdminToRouter>();
    ASSERT_TRUE(request.has_user_token_request());
    EXPECT_EQ(request.user_token_request().command_name(),
              proto::router::kCommandUserTokenRevoke);
    EXPECT_EQ(request.user_token_request().user_id(), 7);
    ASSERT_EQ(request.user_token_request().token_id_size(), 2);
    EXPECT_EQ(request.user_token_request().token_id(0), 100);
}

//--------------------------------------------------------------------------------------------------
// An edit carries the workspace the host is to end up in and the revision it was built on. An
// ordinary edit repeats the workspace the host is already in, so editing a host never releases
// it by omission.
TEST_F(RouterTest, HostEditCarriesTheWorkspaceAndTheRevision)
{
    RouterHost host;
    host.host_id = HostId(1);
    host.workspace_id = kWorkspaceId;
    host.group_id = 5;
    host.display_name = "display";
    host.revision = 3;

    router_.editHost(host, { &receiver_, [](const proto::router::HostResult&) {} });

    const auto request = lastRequest<proto::router::ManagerToRouter>();
    ASSERT_TRUE(request.has_host_request());
    EXPECT_EQ(request.host_request().command_name(), proto::router::kCommandHostModify);
    EXPECT_EQ(request.host_request().host().workspace_id(), kWorkspaceId);
    EXPECT_EQ(request.host_request().host().group_id(), 5);
    EXPECT_EQ(request.host_request().host().revision(), 3);
}

//--------------------------------------------------------------------------------------------------
// An unsendable record is refused before the wire, in the same terms the router would answer with,
// so the caller is not left waiting. The bounds count UTF-8 bytes, so a name of 64 non-ASCII
// characters is over the bound while the input field that accepted it is not; the name of a group
// or a workspace is also mandatory and judged after trimming, the way the router judges it.
TEST_F(RouterTest, RefusedRecordIsAnsweredWithoutTouchingTheWire)
{
    const QString cyrillic_name(proto::router::kMaxEntryNameLength / 2 + 1, QChar(0x0410));
    const QString long_comment(proto::router::kMaxCommentLength + 1, QChar('c'));

    RouterHost host;
    host.host_id = HostId(1);
    host.workspace_id = kWorkspaceId;
    host.display_name = QString(proto::router::kMaxEntryNameLength + 1, QChar('n'));

    proto::router::HostResult host_result;
    auto store_host = [&host_result](const proto::router::HostResult& reply) { host_result = reply; };
    router_.editHost(host, { &receiver_, store_host });
    EXPECT_EQ(host_result.error_code(), proto::router::kErrorInvalidData);

    host.display_name = cyrillic_name;
    host_result.Clear();
    router_.editHost(host, { &receiver_, store_host });
    EXPECT_EQ(host_result.error_code(), proto::router::kErrorInvalidData);

    RouterWorkspace workspace;
    workspace.entry_id = kWorkspaceId;
    workspace.name = "   ";

    proto::router::WorkspaceResult workspace_result;
    auto store_workspace = [&workspace_result](const proto::router::WorkspaceResult& reply)
    {
        workspace_result = reply;
    };
    router_.modifyWorkspace(workspace, { &receiver_, store_workspace });
    EXPECT_EQ(workspace_result.error_code(), proto::router::kErrorInvalidData);

    workspace.name = "alpha";
    workspace.comment = long_comment;
    workspace_result.Clear();
    router_.modifyWorkspace(workspace, { &receiver_, store_workspace });
    EXPECT_EQ(workspace_result.error_code(), proto::router::kErrorInvalidData);

    RouterGroup group; // The name is empty.

    proto::router::GroupResult group_result;
    auto store_group = [&group_result](const proto::router::GroupResult& reply)
    {
        group_result = reply;
    };
    router_.addGroup(kWorkspaceId, group, { &receiver_, store_group });
    EXPECT_EQ(group_result.error_code(), proto::router::kErrorInvalidData);

    EXPECT_TRUE(sent_.isEmpty());
    EXPECT_EQ(RouterTestPeer::rpc(router_).pendingCount(), 0);
}

//--------------------------------------------------------------------------------------------------
// A record sitting exactly on the bounds goes out, and its name goes out trimmed - that is the
// value the router stores and measures, otherwise a name of blanks would pass here and be refused
// there.
TEST_F(RouterTest, RecordOnTheBoundsIsSentWithItsNameTrimmed)
{
    RouterWorkspace workspace;
    workspace.entry_id = kWorkspaceId;
    workspace.name = "  " + QString(proto::router::kMaxEntryNameLength, QChar('n')) + "  ";

    router_.modifyWorkspace(workspace,
                            { &receiver_, [](const proto::router::WorkspaceResult&) {} });

    ASSERT_EQ(sent_.size(), 1);
    const auto request = lastRequest<proto::router::AdminToRouter>();
    ASSERT_TRUE(request.has_workspace_request());
    EXPECT_EQ(request.workspace_request().workspace().name().size(),
              proto::router::kMaxEntryNameLength);
}

//--------------------------------------------------------------------------------------------------
// The count of a scope is never negative. One that arrives so is not data the pagination can work
// with - it takes a non-negative count as its contract and ends the process on anything else - so
// the parsing of the reply is where it stops.
TEST_F(RouterTest, NegativeTotalCountDoesNotReachTheCallers)
{
    const RouterHostList delivered = fetchHosts(hostList(kWorkspaceId, { HostId(1) }, -5));
    EXPECT_EQ(delivered.total_count, 0);

    RouterHostList found;
    router_.searchHosts("host", 0, 25,
                        { &receiver_, [&found](const RouterHostList& value) { found = value; } });

    const auto request = lastRequest<proto::router::ClientToRouter>();
    ASSERT_TRUE(request.has_host_search_request());

    proto::router::RouterToClient reply;
    auto* result = reply.mutable_host_search_result();
    result->set_request_id(request.host_search_request().request_id());
    result->set_error_code(proto::router::kErrorOk);
    result->set_total_count(-5);
    deliver(proto::router::CHANNEL_ID_CLIENT, reply);

    EXPECT_EQ(found.error_code, QString::fromStdString(proto::router::kErrorOk));
    EXPECT_EQ(found.total_count, 0);
}
