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

    // ONLINE is reached through the keys of the account, which a test has no password for.
    static void setStatus(Router& router, Router::Status status) { router.setStatus(status); }
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

    // Loads the identity of the session.
    void loadKeys()
    {
        RouterKeysFixture::loadKeys(&RouterTestPeer::keys(router_));
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
            workspace->add_access()->set_user_id(kUserId);
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
        loadKeys();
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
TEST_F(RouterTest, WorkspaceListIsDecoded)
{
    loadKeys();

    const RouterWorkspaceList decoded = fetchWorkspaces(0, workspaceList({10}, "note"));

    ASSERT_EQ(decoded.error_code, QString::fromStdString(proto::router::kErrorOk));
    ASSERT_EQ(decoded.workspaces.size(), 1);
    EXPECT_EQ(decoded.workspaces.at(0).comment, "note");
    ASSERT_EQ(decoded.workspaces.at(0).access.size(), 1);
    EXPECT_EQ(decoded.workspaces.at(0).access.at(0).user_id, kUserId);
}

//--------------------------------------------------------------------------------------------------
// An error reply carries no list at all, so it must not be cached as the answer about what we
// can access.
TEST_F(RouterTest, FailedWorkspaceListChangesNothing)
{
    loadKeys();

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
    loadKeys();

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
// The connection request carries the session type, and the offer comes back with the key of the
// host intact: it is what the network worker anchors the anonymous handshake with.
TEST_F(RouterTest, ConnectionRequestCarriesTheSessionType)
{
    const quint32 kSessionType = 1;
    const std::string kHostPublicKey(32, 'k');

    std::string received_key;
    router_.requestConnection(HostId(7), kSessionType,
        { &receiver_, [&received_key](const proto::router::ConnectionOffer& offer)
    {
        received_key = offer.host_public_key();
    } });

    const auto request = lastRequest<proto::router::ClientToRouter>();
    ASSERT_TRUE(request.has_connection_request());
    EXPECT_EQ(request.connection_request().host_id(), 7u);
    EXPECT_EQ(request.connection_request().session_type(), kSessionType);

    proto::router::RouterToClient reply;
    proto::router::ConnectionOffer* offer = reply.mutable_connection_offer();
    offer->set_request_id(request.connection_request().request_id());
    offer->set_error_code(proto::router::kErrorOk);
    offer->set_host_public_key(kHostPublicKey);
    deliver(proto::router::CHANNEL_ID_CLIENT, reply);

    EXPECT_EQ(received_key, kHostPublicKey);
}

//--------------------------------------------------------------------------------------------------
// The caller receives the decoded rows, and the next one that accepts a cached answer is served
// without a request.
TEST_F(RouterTest, HostListIsDecodedAndCached)
{
    loadKeys();

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
// The groups of a workspace are decrypted with its key and cached per workspace.
TEST_F(RouterTest, GroupListIsDecryptedAndCached)
{
    loadKeys();

    proto::router::GroupList list;
    list.set_error_code(proto::router::kErrorOk);
    list.set_workspace_id(kWorkspaceId);

    proto::router::Group* group = list.add_group();
    group->set_entry_id(5);
    group->set_name("servers");
    group->set_comment("group comment");

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

    // The identity survives a lost connection: only disconnectFromRouter() drops it.
    EXPECT_EQ(RouterTestPeer::keys(router_).userId(), kUserId);
}

//--------------------------------------------------------------------------------------------------
// A caller answered by the teardown can retry from inside its handler, and a retry that accepts a
// cached answer must not be served the lists of the session that just died: the caches have to be
// gone before the callers are woken.
TEST_F(RouterTest, CallerAnsweredByTeardownDoesNotSeeTheDeadCaches)
{
    fillCaches();

    int wire_requests = 0;
    router_.listUsers({ &receiver_, [&](const proto::router::UserList&)
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
    router_.listUsers({ &receiver_, [&](const proto::router::UserList& list)
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
// A session that ends keeps nothing: the identity goes with it.
TEST_F(RouterTest, ClearedSessionKeepsNoKeys)
{
    loadKeys();

    router_.disconnectFromRouter();

    EXPECT_EQ(RouterTestPeer::keys(router_).userId(), 0);
    EXPECT_FALSE(RouterTestPeer::keys(router_).hasPrivateKey());
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
    loadKeys();

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
