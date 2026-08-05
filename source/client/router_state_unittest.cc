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

#include "client/router_state.h"

#include <gtest/gtest.h>

#include <QObject>

#include "client/router_test_fixture.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"

namespace {

constexpr qint64 kWorkspaceId = 10;

const RouterCache::HostKey kHostKey { kWorkspaceId, 0, 0, 100 };

} // namespace

// The session state ties the keys, the pending requests and the cached lists together; these tests
// cover the composition. The keys and the codec have tests of their own.
class RouterStateTest : public RouterKeysFixture
{
protected:
    // Loads the identity and the keys of the given workspaces into the session.
    void loadKeys(const QList<qint64>& workspace_ids)
    {
        RouterKeysFixture::loadKeys(&state_.keys(), workspace_ids);
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

    proto::router::HostList hostList(qint64 workspace_id, const QList<HostId>& host_ids,
                                     qint64 total_count)
    {
        proto::router::HostList list;
        list.set_error_code(proto::router::kErrorOk);
        list.set_workspace_id(workspace_id);
        list.set_total_count(total_count);

        for (HostId host_id : host_ids)
        {
            proto::router::Host* host = list.add_host();
            host->set_host_id(host_id);
            host->set_workspace_id(workspace_id);
            host->set_display_name("host");
        }

        return list;
    }

    // Puts one entry in every cache, so what a reply drops can be seen by what is left.
    void fillCaches()
    {
        loadKeys({ kWorkspaceId });
        state_.applyWorkspaceList(workspaceList({ kWorkspaceId }), 0);

        proto::router::GroupList groups;
        groups.set_error_code(proto::router::kErrorOk);
        groups.set_workspace_id(kWorkspaceId);
        state_.applyGroupList(groups);

        state_.applyHostList(hostList(kWorkspaceId, { HostId(1) }, 1), kHostKey, true);

        ASSERT_TRUE(state_.cache().workspacesLoaded());
        ASSERT_NE(state_.cache().groupList(kWorkspaceId), nullptr);
        ASSERT_NE(state_.cache().hostList(kHostKey), nullptr);
    }

    static proto::router::RouterToAdmin workspaceResult(const char* command, const char* error_code)
    {
        proto::router::RouterToAdmin message;
        proto::router::WorkspaceResult* result = message.mutable_workspace_result();
        result->set_request_id(1);
        result->set_command_name(command);
        result->set_error_code(error_code);
        return message;
    }

    static proto::router::RouterToAdmin userResult(const char* command, const char* error_code)
    {
        proto::router::RouterToAdmin message;
        proto::router::UserResult* result = message.mutable_user_result();
        result->set_request_id(1);
        result->set_command_name(command);
        result->set_error_code(error_code);
        return message;
    }

    static proto::router::RouterToManager groupResult(const char* command, const char* error_code)
    {
        proto::router::RouterToManager message;
        proto::router::GroupResult* result = message.mutable_group_result();
        result->set_request_id(1);
        result->set_command_name(command);
        result->set_error_code(error_code);
        return message;
    }

    RouterState state_;
};

//--------------------------------------------------------------------------------------------------
// The list carries the workspace key of every member entry of ours, so a session can open a
// workspace it learned about after the UserKeys message.
TEST_F(RouterStateTest, WorkspaceListDecryptsCommentAndAddsKey)
{
    loadKeys({});

    const RouterWorkspaceList decoded =
        state_.applyWorkspaceList(workspaceList({10}, "secret"), 0);

    ASSERT_EQ(decoded.error_code, QString::fromStdString(proto::router::kErrorOk));
    ASSERT_EQ(decoded.workspaces.size(), 1);
    EXPECT_EQ(decoded.workspaces.at(0).comment, "secret");
    EXPECT_TRUE(state_.keys().hasWorkspaceKey(10));
}

//--------------------------------------------------------------------------------------------------
// Losing access to a workspace must take its group key with it: keeping the key would let this
// session keep decrypting the records of a workspace it no longer belongs to and, worse, hand the
// key to a user it grants access to.
TEST_F(RouterStateTest, CompleteListDropsKeysOfLostWorkspaces)
{
    loadKeys({10, 20});

    state_.applyWorkspaceList(workspaceList({10}), 0);

    EXPECT_TRUE(state_.keys().hasWorkspaceKey(10));
    EXPECT_FALSE(state_.keys().hasWorkspaceKey(20));
}

//--------------------------------------------------------------------------------------------------
// A reply about one workspace says nothing about the others, so it must not drop their keys.
TEST_F(RouterStateTest, SingleWorkspaceReplyKeepsOtherKeys)
{
    loadKeys({10, 20});

    state_.applyWorkspaceList(workspaceList({10}), 10);

    EXPECT_TRUE(state_.keys().hasWorkspaceKey(10));
    EXPECT_TRUE(state_.keys().hasWorkspaceKey(20));
}

//--------------------------------------------------------------------------------------------------
// An error reply carries no list at all - reading it as "everything is gone" would drop every key
// and every cached list on a transient failure.
TEST_F(RouterStateTest, FailedWorkspaceListChangesNothing)
{
    loadKeys({10, 20});

    proto::router::WorkspaceList failed;
    failed.set_error_code(proto::router::kErrorInternalError);

    state_.applyWorkspaceList(failed, 0);

    EXPECT_TRUE(state_.keys().hasWorkspaceKey(10));
    EXPECT_TRUE(state_.keys().hasWorkspaceKey(20));
    EXPECT_FALSE(state_.cache().workspacesLoaded());
}

//--------------------------------------------------------------------------------------------------
// The count of the whole scope drives the pagination of the client, so it must survive the cache:
// a cached answer with a zero count would collapse the page list.
TEST_F(RouterStateTest, CachedHostListKeepsTotalCountAndEchoes)
{
    loadKeys({10});

    const RouterCache::HostKey key{ 10, 0, 0, 9 };
    state_.applyHostList(hostList(10, {HostId(1), HostId(2)}, 25), key, true);

    const RouterHostList* cached = state_.cache().hostList(key);
    ASSERT_NE(cached, nullptr);
    EXPECT_EQ(cached->total_count, 25);
    EXPECT_EQ(cached->workspace_id, 10);
    EXPECT_EQ(cached->error_code, QString::fromStdString(proto::router::kErrorOk));
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterStateTest, GroupListIsDecryptedAndCached)
{
    loadKeys({10});

    proto::router::GroupList list;
    list.set_error_code(proto::router::kErrorOk);
    list.set_workspace_id(10);

    proto::router::Group* group = list.add_group();
    group->set_entry_id(5);
    group->set_name("servers");
    group->set_comment(encrypt(10, "group comment"));

    const RouterGroupList decoded = state_.applyGroupList(list);

    ASSERT_EQ(decoded.groups.size(), 1);
    EXPECT_EQ(decoded.groups.at(0).comment, "group comment");
    EXPECT_EQ(decoded.groups.at(0).workspace_id, 10);

    const RouterGroupList* cached = state_.cache().groupList(10);
    ASSERT_NE(cached, nullptr);
    EXPECT_EQ(cached->groups.size(), 1);
    EXPECT_EQ(state_.cache().groupList(20), nullptr);
}

//--------------------------------------------------------------------------------------------------
// The decoding step runs before the handler, which is what lets the callers work with the plain
// structs instead of the wire messages.
TEST_F(RouterStateTest, DecoderRunsBeforeTheHandler)
{
    loadKeys({10});

    QObject receiver;
    RouterHostList delivered;

    proto::router::HostListRequest request;
    request.set_request_id(state_.rpc().nextRequestId());

    const RouterCache::HostKey key{ 10, 0, 0, 0 };
    state_.rpc().registerPending<proto::router::HostList>(&request, &receiver,
        [&delivered](const RouterHostList& list) { delivered = list; },
        [this, key](const proto::router::HostList& raw)
    {
        return state_.applyHostList(raw, key, true);
    });

    state_.rpc().dispatch(request.request_id(), hostList(10, {HostId(1)}, 1));

    EXPECT_EQ(delivered.hosts.size(), 1);
    EXPECT_NE(state_.cache().hostList(key), nullptr);
}

//--------------------------------------------------------------------------------------------------
// The router re-opens the two-factor stage of a live session after a password change and drops
// everything it receives until the stage completes. The session is suspended then: the replies we
// wait for will never arrive and the cached lists are no longer known to be current.
TEST_F(RouterStateTest, SuspendedSessionDropsPendingRepliesAndCaches)
{
    loadKeys({10});

    const RouterCache::HostKey key{ 10, 0, 0, 0 };
    state_.applyHostList(hostList(10, {HostId(1)}, 1), key, true);
    state_.applyWorkspaceList(workspaceList({10}), 0);
    ASSERT_TRUE(state_.cache().workspacesLoaded());

    QObject receiver;
    int calls = 0;
    std::string last_error;

    proto::router::UserListRequest request;
    request.set_request_id(state_.rpc().nextRequestId());
    state_.rpc().registerPending<proto::router::UserList>(&request, &receiver,
        [&](const proto::router::UserList& list) { ++calls; last_error = list.error_code(); });

    state_.clearCaches();
    state_.rpc().clearPending();

    EXPECT_EQ(state_.rpc().pendingCount(), 0);
    EXPECT_FALSE(state_.cache().workspacesLoaded());
    EXPECT_EQ(state_.cache().hostList(key), nullptr);

    // The caller is told the answer will never come, once.
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(last_error, proto::router::kErrorLostConnection);

    // A late reply to a request of the dead window must not reach the caller.
    proto::router::UserList response;
    state_.rpc().dispatch(request.request_id(), response);
    EXPECT_EQ(calls, 1);

    // The keys survive: the session is being re-authenticated, not lost.
    EXPECT_TRUE(state_.keys().hasWorkspaceKey(10));
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterStateTest, ClearedSessionKeepsNoKeys)
{
    loadKeys({10});
    state_.clearSession();

    EXPECT_EQ(state_.keys().userId(), 0);
    EXPECT_FALSE(state_.keys().hasWorkspaceKey(10));
    EXPECT_EQ(state_.rpc().pendingCount(), 0);
}

//--------------------------------------------------------------------------------------------------
// A reply of the administrator channel finds the request that is waiting for it.
TEST_F(RouterStateTest, AdminReplyReachesItsRequest)
{
    QObject receiver;
    int calls = 0;

    proto::router::UserListRequest request;
    request.set_request_id(state_.rpc().nextRequestId());
    state_.rpc().registerPending<proto::router::UserList>(&request, &receiver,
        [&calls](const proto::router::UserList&) { ++calls; });

    proto::router::RouterToAdmin message;
    message.mutable_user_list()->set_request_id(request.request_id());

    EXPECT_TRUE(state_.routeReply(message));
    EXPECT_EQ(calls, 1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterStateTest, ManagerReplyReachesItsRequest)
{
    QObject receiver;
    int calls = 0;

    proto::router::GroupRequest request;
    request.set_request_id(state_.rpc().nextRequestId());
    state_.rpc().registerPending<proto::router::GroupResult>(&request, &receiver,
        [&calls](const proto::router::GroupResult&) { ++calls; });

    proto::router::RouterToManager message;
    message.mutable_group_result()->set_request_id(request.request_id());

    EXPECT_TRUE(state_.routeReply(message));
    EXPECT_EQ(calls, 1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterStateTest, ClientReplyReachesItsRequest)
{
    QObject receiver;
    int calls = 0;

    proto::router::CheckHostStatus request;
    request.set_request_id(state_.rpc().nextRequestId());
    state_.rpc().registerPending<proto::router::HostStatus>(&request, &receiver,
        [&calls](const proto::router::HostStatus&) { ++calls; });

    proto::router::RouterToClient message;
    message.mutable_host_status()->set_request_id(request.request_id());

    EXPECT_TRUE(state_.routeReply(message));
    EXPECT_EQ(calls, 1);
}

//--------------------------------------------------------------------------------------------------
// The two-factor stage, the keys and the change notifications are not answers to anything: they
// belong to the session, which owns the status and the stored configuration.
TEST_F(RouterStateTest, SessionMessagesAreNotReplies)
{
    proto::router::RouterToClient challenge;
    challenge.mutable_two_factor_challenge()->set_mode(proto::router::TWO_FACTOR_MODE_ACTIVE);
    EXPECT_FALSE(state_.routeReply(challenge));

    proto::router::RouterToClient keys;
    keys.mutable_user_keys()->set_user_id(kUserId);
    EXPECT_FALSE(state_.routeReply(keys));

    proto::router::RouterToClient notification;
    notification.mutable_notification()->set_hosts_dirty(true);
    EXPECT_FALSE(state_.routeReply(notification));

    // A message with nothing in it at all.
    EXPECT_FALSE(state_.routeReply(proto::router::RouterToAdmin()));
    EXPECT_FALSE(state_.routeReply(proto::router::RouterToManager()));
}

//--------------------------------------------------------------------------------------------------
// A reply nobody waits for any more (the dialog was closed) is still a reply: the cache rules that
// come with it must run. What the rules are is the business of RouterCache; here it only matters
// that every reply channel feeds them.
TEST_F(RouterStateTest, ReplyWithoutARequestStillAppliesItsRules)
{
    fillCaches();

    EXPECT_TRUE(state_.routeReply(workspaceResult(proto::router::kCommandWorkspaceModify,
                                                  proto::router::kErrorOk)));
    EXPECT_FALSE(state_.cache().workspacesLoaded());

    fillCaches();

    EXPECT_TRUE(state_.routeReply(groupResult(proto::router::kCommandGroupDelete,
                                              proto::router::kErrorOk)));
    EXPECT_EQ(state_.cache().groupList(kWorkspaceId), nullptr);
    EXPECT_EQ(state_.cache().hostList(kHostKey), nullptr);
}
