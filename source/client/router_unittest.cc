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

// Test-only access to the keys, the pending replies and the incoming messages.
class RouterTestPeer
{
public:
    static RouterKeys& keys(Router& router) { return router.keys_; }
    static RouterRpc& rpc(Router& router) { return router.rpc_; }

    static void receive(Router& router, quint8 channel_id, const QByteArray& bytes)
    {
        router.onTcpMessageReceived(router.routerId(), channel_id, bytes);
    }
};

// A session without a worker: what it sends is collected from sig_sendMessage, the replies are
// handed to it as the worker would. The keys, the codec, the cache and the rpc have tests of
// their own; here the conversation is under test.
class RouterTest : public RouterKeysFixture
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

    // Loads the identity and the keys of the given workspaces into the session.
    void loadKeys(const QList<qint64>& workspace_ids)
    {
        RouterKeysFixture::loadKeys(&RouterTestPeer::keys(router_), workspace_ids);
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

    // A workspace list as the router builds it for an admin session: our own access entry carries
    // the sealed group key.
    proto::router::WorkspaceList workspaceList(const QList<qint64>& workspace_ids,
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
            if (!comment.isEmpty())
                workspace->set_comment(encrypt(workspace_id, comment));

            proto::router::WorkspaceAccess* access = workspace->add_access();
            access->set_user_id(kUserId);
            access->set_wrapped_gk(
                SealedBox::seal(groupKey(workspace_id), user_.public_key).toStdString());
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
        loadKeys({ kWorkspaceId });
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
// The list carries the key of every workspace of ours, so a session can open one it learned
// about after the UserKeys message.
TEST_F(RouterTest, WorkspaceListDecryptsCommentAndAddsKey)
{
    loadKeys({});

    const RouterWorkspaceList decoded = fetchWorkspaces(0, workspaceList({10}, "secret"));

    ASSERT_EQ(decoded.error_code, QString::fromStdString(proto::router::kErrorOk));
    ASSERT_EQ(decoded.workspaces.size(), 1);
    EXPECT_EQ(decoded.workspaces.at(0).comment, "secret");
    EXPECT_TRUE(RouterTestPeer::keys(router_).hasWorkspaceKey(10));
}

//--------------------------------------------------------------------------------------------------
// Lost access must take the group key with it: a kept key still decrypts the records and, worse,
// would be handed to a user this session grants access to.
TEST_F(RouterTest, CompleteListDropsKeysOfLostWorkspaces)
{
    loadKeys({10, 20});

    fetchWorkspaces(0, workspaceList({10}));

    EXPECT_TRUE(RouterTestPeer::keys(router_).hasWorkspaceKey(10));
    EXPECT_FALSE(RouterTestPeer::keys(router_).hasWorkspaceKey(20));
}

//--------------------------------------------------------------------------------------------------
// A reply about one workspace says nothing about the others, so it must not drop their keys.
TEST_F(RouterTest, SingleWorkspaceReplyKeepsOtherKeys)
{
    loadKeys({10, 20});

    fetchWorkspaces(10, workspaceList({10}));

    EXPECT_TRUE(RouterTestPeer::keys(router_).hasWorkspaceKey(10));
    EXPECT_TRUE(RouterTestPeer::keys(router_).hasWorkspaceKey(20));
}

//--------------------------------------------------------------------------------------------------
// An error reply carries no list at all - read as "everything is gone" it would drop every key
// on a transient failure.
TEST_F(RouterTest, FailedWorkspaceListChangesNothing)
{
    loadKeys({10, 20});

    proto::router::WorkspaceList failed;
    failed.set_error_code(proto::router::kErrorInternalError);
    fetchWorkspaces(0, failed);

    EXPECT_TRUE(RouterTestPeer::keys(router_).hasWorkspaceKey(10));
    EXPECT_TRUE(RouterTestPeer::keys(router_).hasWorkspaceKey(20));
    EXPECT_FALSE(workspacesServedFromCache());
}

//--------------------------------------------------------------------------------------------------
// The whole conversation seen from outside: the request leaves for its channel with an id of its
// own, and the reply carrying that id reaches the caller.
TEST_F(RouterTest, ListUsersConversation)
{
    int calls = 0;
    router_.listUsers({ &receiver_, [&calls](const proto::router::UserList&) { ++calls; } });

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
    loadKeys({ kWorkspaceId });

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
// The caller receives the decoded rows, and the next one that accepts a cached answer is served
// without a request.
TEST_F(RouterTest, HostListIsDecodedAndCached)
{
    loadKeys({ kWorkspaceId });

    proto::router::HostList list = hostList(kWorkspaceId, { HostId(1) }, 25);
    list.mutable_host(0)->set_comment(encrypt(kWorkspaceId, "comment"));

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
// The groups of a workspace are decrypted with its key and cached per workspace.
TEST_F(RouterTest, GroupListIsDecryptedAndCached)
{
    loadKeys({ kWorkspaceId });

    proto::router::GroupList list;
    list.set_error_code(proto::router::kErrorOk);
    list.set_workspace_id(kWorkspaceId);

    proto::router::Group* group = list.add_group();
    group->set_entry_id(5);
    group->set_name("servers");
    group->set_comment(encrypt(kWorkspaceId, "group comment"));

    const RouterGroupList decoded = fetchGroups(kWorkspaceId, list);

    ASSERT_EQ(decoded.groups.size(), 1);
    EXPECT_EQ(decoded.groups.at(0).comment, "group comment");
    EXPECT_EQ(decoded.groups.at(0).workspace_id, kWorkspaceId);

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
    router_.listUsers({ &receiver_, [&](const proto::router::UserList& list)
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

    EXPECT_TRUE(RouterTestPeer::keys(router_).hasWorkspaceKey(kWorkspaceId));
}

//--------------------------------------------------------------------------------------------------
// A session that ends keeps nothing: the identity and the keys go with it.
TEST_F(RouterTest, ClearedSessionKeepsNoKeys)
{
    loadKeys({ kWorkspaceId });

    router_.disconnectFromRouter();

    EXPECT_EQ(RouterTestPeer::keys(router_).userId(), 0);
    EXPECT_FALSE(RouterTestPeer::keys(router_).hasWorkspaceKey(kWorkspaceId));
    EXPECT_EQ(RouterTestPeer::rpc(router_).pendingCount(), 0);
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
// An unsendable request must not leave a caller waiting.
TEST_F(RouterTest, RefusedRecordIsAnsweredWithoutTouchingTheWire)
{
    loadKeys({ kWorkspaceId });

    RouterHost host;
    host.host_id = HostId(1);
    host.workspace_id = kWorkspaceId;
    host.display_name = QString(proto::router::kMaxEntryNameLength + 1, QChar('n'));

    proto::router::HostResult result;
    auto store = [&result](const proto::router::HostResult& reply) { result = reply; };
    router_.editHost(host, { &receiver_, store });

    EXPECT_TRUE(sent_.isEmpty());
    EXPECT_EQ(result.error_code(), proto::router::kErrorInvalidData);
    EXPECT_EQ(RouterTestPeer::rpc(router_).pendingCount(), 0);
}
