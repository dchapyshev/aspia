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

const RouterState::HostCacheKey kHostKey { kWorkspaceId, 0, 0, 100 };

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

        ASSERT_TRUE(state_.workspacesLoaded());
        ASSERT_NE(state_.cachedGroupList(kWorkspaceId), nullptr);
        ASSERT_NE(state_.cachedHostList(kHostKey), nullptr);
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
    EXPECT_FALSE(state_.workspacesLoaded());
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterStateTest, CachedWorkspaceListLooksLikeASuccessfulReply)
{
    loadKeys({10});
    state_.applyWorkspaceList(workspaceList({10}), 0);

    ASSERT_TRUE(state_.workspacesLoaded());

    const RouterWorkspaceList cached = state_.cachedWorkspaceList();
    EXPECT_EQ(cached.error_code, QString::fromStdString(proto::router::kErrorOk));
    EXPECT_EQ(cached.workspaces.size(), 1);

    // An edit that moved the workspaces marks the cache stale without dropping the keys.
    state_.invalidateWorkspaces();
    EXPECT_FALSE(state_.workspacesLoaded());
    EXPECT_TRUE(state_.keys().hasWorkspaceKey(10));
}

//--------------------------------------------------------------------------------------------------
// The page is part of the identity of a cached host list: serving the rows of one page for another
// would show the wrong hosts.
TEST_F(RouterStateTest, HostPagesAreCachedApart)
{
    loadKeys({10});

    const RouterState::HostCacheKey first{ 10, 0, 0, 9 };
    const RouterState::HostCacheKey second{ 10, 0, 10, 19 };

    state_.applyHostList(hostList(10, {HostId(1)}, 25), first, true);

    ASSERT_NE(state_.cachedHostList(first), nullptr);
    EXPECT_EQ(state_.cachedHostList(first)->hosts.size(), 1);
    EXPECT_EQ(state_.cachedHostList(second), nullptr);
}

//--------------------------------------------------------------------------------------------------
// The count of the whole scope drives the pagination of the client, so it must survive the cache:
// a cached answer with a zero count would collapse the page list.
TEST_F(RouterStateTest, CachedHostListKeepsTotalCountAndEchoes)
{
    loadKeys({10});

    const RouterState::HostCacheKey key{ 10, 0, 0, 9 };
    state_.applyHostList(hostList(10, {HostId(1), HostId(2)}, 25), key, true);

    const RouterHostList* cached = state_.cachedHostList(key);
    ASSERT_NE(cached, nullptr);
    EXPECT_EQ(cached->total_count, 25);
    EXPECT_EQ(cached->workspace_id, 10);
    EXPECT_EQ(cached->error_code, QString::fromStdString(proto::router::kErrorOk));
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterStateTest, FailedHostListIsNotCached)
{
    loadKeys({10});

    proto::router::HostList failed;
    failed.set_error_code(proto::router::kErrorAccessDenied);

    const RouterState::HostCacheKey key{ 10, 0, 0, 0 };
    state_.applyHostList(failed, key, true);

    EXPECT_EQ(state_.cachedHostList(key), nullptr);
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

    const RouterGroupList* cached = state_.cachedGroupList(10);
    ASSERT_NE(cached, nullptr);
    EXPECT_EQ(cached->groups.size(), 1);
    EXPECT_EQ(state_.cachedGroupList(20), nullptr);
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterStateTest, FailedGroupListIsNotCached)
{
    loadKeys({10});

    proto::router::GroupList failed;
    failed.set_error_code(proto::router::kErrorAccessDenied);
    failed.set_workspace_id(10);

    state_.applyGroupList(failed);

    EXPECT_EQ(state_.cachedGroupList(10), nullptr);
}

//--------------------------------------------------------------------------------------------------
// A notification is the router saying a change made elsewhere left what we hold out of date. The
// invalidation next to a reply only covers what this client wrote, so without this a caller that
// accepts a cached answer keeps being served the very rows the notification was about.
TEST_F(RouterStateTest, NotificationDropsOnlyTheListsItNames)
{
    fillCaches();

    proto::router::Notification hosts;
    hosts.set_hosts_dirty(true);
    state_.applyNotification(hosts);

    EXPECT_EQ(state_.cachedHostList(kHostKey), nullptr);
    EXPECT_NE(state_.cachedGroupList(kWorkspaceId), nullptr);
    EXPECT_TRUE(state_.workspacesLoaded());

    fillCaches();

    proto::router::Notification groups;
    groups.set_groups_dirty(true);
    state_.applyNotification(groups);

    EXPECT_EQ(state_.cachedGroupList(kWorkspaceId), nullptr);
    EXPECT_NE(state_.cachedHostList(kHostKey), nullptr);

    fillCaches();

    proto::router::Notification workspaces;
    workspaces.set_workspaces_dirty(true);
    state_.applyNotification(workspaces);

    EXPECT_FALSE(state_.workspacesLoaded());
    EXPECT_NE(state_.cachedHostList(kHostKey), nullptr);
}

//--------------------------------------------------------------------------------------------------
// What the notification says nothing about is still good: those lists are not cached here at all,
// and dropping the ones that are would cost a reload for nothing.
TEST_F(RouterStateTest, NotificationOfSomethingElseKeepsTheLists)
{
    fillCaches();

    proto::router::Notification notification;
    notification.set_users_dirty(true);
    notification.set_relays_dirty(true);
    notification.set_clients_dirty(true);
    notification.set_temp_hosts_dirty(true);
    state_.applyNotification(notification);

    EXPECT_TRUE(state_.workspacesLoaded());
    EXPECT_NE(state_.cachedGroupList(kWorkspaceId), nullptr);
    EXPECT_NE(state_.cachedHostList(kHostKey), nullptr);
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

    const RouterState::HostCacheKey key{ 10, 0, 0, 0 };
    state_.rpc().registerPending<proto::router::HostList>(&request, &receiver,
        [&delivered](const RouterHostList& list) { delivered = list; },
        [this, key](const proto::router::HostList& raw)
    {
        return state_.applyHostList(raw, key, true);
    });

    state_.rpc().dispatch(request.request_id(), hostList(10, {HostId(1)}, 1));

    EXPECT_EQ(delivered.hosts.size(), 1);
    EXPECT_NE(state_.cachedHostList(key), nullptr);
}

//--------------------------------------------------------------------------------------------------
// The router re-opens the two-factor stage of a live session after a password change and drops
// everything it receives until the stage completes. The session is suspended then: the replies we
// wait for will never arrive and the cached lists are no longer known to be current.
TEST_F(RouterStateTest, SuspendedSessionDropsPendingRepliesAndCaches)
{
    loadKeys({10});

    const RouterState::HostCacheKey key{ 10, 0, 0, 0 };
    state_.applyHostList(hostList(10, {HostId(1)}, 1), key, true);
    state_.applyWorkspaceList(workspaceList({10}), 0);
    ASSERT_TRUE(state_.workspacesLoaded());

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
    EXPECT_FALSE(state_.workspacesLoaded());
    EXPECT_EQ(state_.cachedHostList(key), nullptr);

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
// come with it must run.
TEST_F(RouterStateTest, ReplyWithoutARequestStillAppliesItsRules)
{
    fillCaches();

    EXPECT_TRUE(state_.routeReply(workspaceResult(proto::router::kCommandWorkspaceModify,
                                                  proto::router::kErrorOk)));
    EXPECT_FALSE(state_.workspacesLoaded());
}

//--------------------------------------------------------------------------------------------------
// Every workspace operation assigns hosts to the workspace or releases them from it, so the cached
// host lists are stale the moment the reply arrives - seconds before the batched notification that
// would refresh them.
TEST_F(RouterStateTest, WorkspaceChangeDropsTheHostsItMoved)
{
    fillCaches();

    ASSERT_TRUE(state_.routeReply(workspaceResult(proto::router::kCommandWorkspaceModify,
                                                  proto::router::kErrorOk)));

    EXPECT_FALSE(state_.workspacesLoaded());
    EXPECT_EQ(state_.cachedHostList(kHostKey), nullptr);

    // A modified workspace keeps its group tree.
    EXPECT_NE(state_.cachedGroupList(kWorkspaceId), nullptr);
}

//--------------------------------------------------------------------------------------------------
// Deleting a workspace takes its whole group tree with it. Keeping the cached groups would show
// the branches of a workspace that is gone.
TEST_F(RouterStateTest, WorkspaceDeleteAlsoDropsTheGroupsItTookWithIt)
{
    fillCaches();

    ASSERT_TRUE(state_.routeReply(workspaceResult(proto::router::kCommandWorkspaceDelete,
                                                  proto::router::kErrorOk)));

    EXPECT_FALSE(state_.workspacesLoaded());
    EXPECT_EQ(state_.cachedHostList(kHostKey), nullptr);
    EXPECT_EQ(state_.cachedGroupList(kWorkspaceId), nullptr);
}

//--------------------------------------------------------------------------------------------------
// A refused operation changed nothing, so there is nothing to reload.
TEST_F(RouterStateTest, FailedWorkspaceChangeKeepsTheCaches)
{
    fillCaches();

    ASSERT_TRUE(state_.routeReply(workspaceResult(proto::router::kCommandWorkspaceDelete,
                                                  proto::router::kErrorConflict)));

    EXPECT_TRUE(state_.workspacesLoaded());
    EXPECT_NE(state_.cachedHostList(kHostKey), nullptr);
    EXPECT_NE(state_.cachedGroupList(kWorkspaceId), nullptr);
}

//--------------------------------------------------------------------------------------------------
// A deleted group releases its hosts, which is why the host lists go with it.
TEST_F(RouterStateTest, GroupDeleteAlsoDropsTheHostsItReleased)
{
    fillCaches();

    ASSERT_TRUE(state_.routeReply(groupResult(proto::router::kCommandGroupDelete,
                                              proto::router::kErrorOk)));

    EXPECT_EQ(state_.cachedGroupList(kWorkspaceId), nullptr);
    EXPECT_EQ(state_.cachedHostList(kHostKey), nullptr);

    // Groups are not part of a workspace record, so the workspace list is still current.
    EXPECT_TRUE(state_.workspacesLoaded());
}

//--------------------------------------------------------------------------------------------------
// Adding or renaming a group moves no host.
TEST_F(RouterStateTest, GroupAddKeepsTheHostCache)
{
    fillCaches();

    ASSERT_TRUE(state_.routeReply(groupResult(proto::router::kCommandGroupAdd,
                                              proto::router::kErrorOk)));

    EXPECT_EQ(state_.cachedGroupList(kWorkspaceId), nullptr);
    EXPECT_NE(state_.cachedHostList(kHostKey), nullptr);
}

//--------------------------------------------------------------------------------------------------
// A host change is a host change: the workspaces and the groups are untouched.
TEST_F(RouterStateTest, HostChangeDropsOnlyTheHostCache)
{
    fillCaches();

    proto::router::RouterToManager message;
    proto::router::HostResult* result = message.mutable_host_result();
    result->set_request_id(1);
    result->set_command_name(proto::router::kCommandHostModify);
    result->set_error_code(proto::router::kErrorOk);

    ASSERT_TRUE(state_.routeReply(message));

    EXPECT_EQ(state_.cachedHostList(kHostKey), nullptr);
    EXPECT_TRUE(state_.workspacesLoaded());
    EXPECT_NE(state_.cachedGroupList(kWorkspaceId), nullptr);
}

//--------------------------------------------------------------------------------------------------
// Adding an administrator grants it an access entry in every workspace and deleting a user drops
// its entries by cascade - both move the revisions the cached list carries.
TEST_F(RouterStateTest, MembershipUserCommandsDropTheWorkspaceCache)
{
    for (const char* command : { proto::router::kCommandUserAdd,
                                 proto::router::kCommandUserModify,
                                 proto::router::kCommandUserDelete })
    {
        fillCaches();

        ASSERT_TRUE(state_.routeReply(userResult(command, proto::router::kErrorOk)));

        EXPECT_FALSE(state_.workspacesLoaded()) << "command: " << command;

        // Users are not cached, and no user command moves a host or a group.
        EXPECT_NE(state_.cachedHostList(kHostKey), nullptr) << "command: " << command;
        EXPECT_NE(state_.cachedGroupList(kWorkspaceId), nullptr) << "command: " << command;
    }
}

//--------------------------------------------------------------------------------------------------
// Resetting the second factor or revoking a token touches nothing but the user itself.
TEST_F(RouterStateTest, TokenUserCommandsKeepTheCaches)
{
    for (const char* command : { proto::router::kCommandUserResetOtp,
                                 proto::router::kCommandUserRevokeTokens })
    {
        fillCaches();

        ASSERT_TRUE(state_.routeReply(userResult(command, proto::router::kErrorOk)));

        EXPECT_TRUE(state_.workspacesLoaded()) << "command: " << command;
        EXPECT_NE(state_.cachedHostList(kHostKey), nullptr) << "command: " << command;
        EXPECT_NE(state_.cachedGroupList(kWorkspaceId), nullptr) << "command: " << command;
    }
}
