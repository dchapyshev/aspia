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

#include "client/router_cache.h"

#include <gtest/gtest.h>

#include "proto/router_client.h"
#include "proto/router_constants.h"

namespace {

constexpr qint64 kWorkspaceId = 10;

const RouterCache::HostKey kHostKey { kWorkspaceId, 0, 0, 100 };

} // namespace

// The lists are plain decoded structs here: what is under test is the storing and the staleness
// rules, not the decoding.
class RouterCacheTest : public testing::Test
{
protected:
    static RouterWorkspaceList workspaceList(const char* error_code = proto::router::kErrorOk)
    {
        RouterWorkspaceList list;
        list.error_code = QString::fromUtf8(error_code);
        list.workspaces.emplaceBack().entry_id = kWorkspaceId;
        return list;
    }

    static RouterHostList hostList(const char* error_code = proto::router::kErrorOk)
    {
        RouterHostList list;
        list.error_code = QString::fromUtf8(error_code);
        list.workspace_id = kWorkspaceId;
        list.total_count = 25;
        list.hosts.emplaceBack().host_id = HostId(1);
        return list;
    }

    static RouterGroupList groupList(const char* error_code = proto::router::kErrorOk)
    {
        RouterGroupList list;
        list.error_code = QString::fromUtf8(error_code);
        list.workspace_id = kWorkspaceId;
        return list;
    }

    // Puts one entry in every cache, so what a rule drops can be seen by what is left.
    void fillAll()
    {
        cache_.storeWorkspaces(workspaceList());
        cache_.storeHosts(kHostKey, hostList());
        cache_.storeGroups(groupList());

        ASSERT_TRUE(cache_.workspacesLoaded());
        ASSERT_NE(cache_.hostList(kHostKey), nullptr);
        ASSERT_NE(cache_.groupList(kWorkspaceId), nullptr);
    }

    RouterCache cache_;
};

//--------------------------------------------------------------------------------------------------
// The page is part of the identity of a cached host list: serving the rows of one page for another
// would show the wrong hosts.
TEST_F(RouterCacheTest, HostPagesAreCachedApart)
{
    const RouterCache::HostKey first{ kWorkspaceId, 0, 0, 9 };
    const RouterCache::HostKey second{ kWorkspaceId, 0, 10, 19 };

    cache_.storeHosts(first, hostList());

    ASSERT_NE(cache_.hostList(first), nullptr);
    EXPECT_EQ(cache_.hostList(first)->hosts.size(), 1);
    EXPECT_EQ(cache_.hostList(first)->total_count, 25);
    EXPECT_EQ(cache_.hostList(second), nullptr);
}

//--------------------------------------------------------------------------------------------------
// A network reply always carries an error code, so a handler is allowed to check it and the
// synthesized reply must not look like an error.
TEST_F(RouterCacheTest, CachedWorkspaceListLooksLikeASuccessfulReply)
{
    cache_.storeWorkspaces(workspaceList());

    const RouterWorkspaceList cached = cache_.workspaceList();
    EXPECT_EQ(cached.error_code, QString::fromUtf8(proto::router::kErrorOk));
    EXPECT_EQ(cached.workspaces.size(), 1);

    // An edit that moved the workspaces marks the list stale without dropping it.
    cache_.invalidateWorkspaces();
    EXPECT_FALSE(cache_.workspacesLoaded());
}

//--------------------------------------------------------------------------------------------------
// An error reply carries no list - storing it would serve its emptiness as a success.
TEST_F(RouterCacheTest, ErrorRepliesAreNotStored)
{
    cache_.storeWorkspaces(workspaceList(proto::router::kErrorInternalError));
    cache_.storeHosts(kHostKey, hostList(proto::router::kErrorAccessDenied));
    cache_.storeGroups(groupList(proto::router::kErrorAccessDenied));

    EXPECT_FALSE(cache_.workspacesLoaded());
    EXPECT_EQ(cache_.hostList(kHostKey), nullptr);
    EXPECT_EQ(cache_.groupList(kWorkspaceId), nullptr);
}

//--------------------------------------------------------------------------------------------------
// A refused operation changed nothing, so there is nothing to reload.
TEST_F(RouterCacheTest, RefusedResultKeepsTheCaches)
{
    fillAll();

    cache_.onResult(RouterCache::Result::WORKSPACE, proto::router::kCommandWorkspaceDelete, false);

    EXPECT_TRUE(cache_.workspacesLoaded());
    EXPECT_NE(cache_.hostList(kHostKey), nullptr);
    EXPECT_NE(cache_.groupList(kWorkspaceId), nullptr);
}

//--------------------------------------------------------------------------------------------------
// A host change is a host change: the workspaces and the groups are untouched.
TEST_F(RouterCacheTest, HostResultDropsOnlyTheHostLists)
{
    fillAll();

    cache_.onResult(RouterCache::Result::HOST, proto::router::kCommandHostModify, true);

    EXPECT_EQ(cache_.hostList(kHostKey), nullptr);
    EXPECT_TRUE(cache_.workspacesLoaded());
    EXPECT_NE(cache_.groupList(kWorkspaceId), nullptr);
}

//--------------------------------------------------------------------------------------------------
// Every workspace operation assigns hosts to the workspace or releases them from it, so the host
// lists are stale the moment the reply arrives - seconds before the batched notification. Deleting
// a workspace also takes its whole group tree with it.
TEST_F(RouterCacheTest, WorkspaceResultDropsByTheReachOfItsCommand)
{
    fillAll();

    cache_.onResult(RouterCache::Result::WORKSPACE, proto::router::kCommandWorkspaceModify, true);

    EXPECT_FALSE(cache_.workspacesLoaded());
    EXPECT_EQ(cache_.hostList(kHostKey), nullptr);
    EXPECT_NE(cache_.groupList(kWorkspaceId), nullptr);   // A modified workspace keeps its tree.

    fillAll();

    cache_.onResult(RouterCache::Result::WORKSPACE, proto::router::kCommandWorkspaceDelete, true);

    EXPECT_FALSE(cache_.workspacesLoaded());
    EXPECT_EQ(cache_.hostList(kHostKey), nullptr);
    EXPECT_EQ(cache_.groupList(kWorkspaceId), nullptr);
}

//--------------------------------------------------------------------------------------------------
// Adding or renaming a group moves no host; a deleted group releases its hosts, which is why the
// host lists go with it. The workspace record does not carry groups, so its list stays current.
TEST_F(RouterCacheTest, GroupResultDropsByTheReachOfItsCommand)
{
    fillAll();

    cache_.onResult(RouterCache::Result::GROUP, proto::router::kCommandGroupAdd, true);

    EXPECT_EQ(cache_.groupList(kWorkspaceId), nullptr);
    EXPECT_NE(cache_.hostList(kHostKey), nullptr);
    EXPECT_TRUE(cache_.workspacesLoaded());

    fillAll();

    cache_.onResult(RouterCache::Result::GROUP, proto::router::kCommandGroupDelete, true);

    EXPECT_EQ(cache_.groupList(kWorkspaceId), nullptr);
    EXPECT_EQ(cache_.hostList(kHostKey), nullptr);
    EXPECT_TRUE(cache_.workspacesLoaded());
}

//--------------------------------------------------------------------------------------------------
// Adding an administrator grants it an access entry in every workspace and deleting a user drops
// its entries by cascade - both move the revisions the cached list carries. Resetting the second
// factor or revoking a token touches nothing but the user itself.
TEST_F(RouterCacheTest, UserResultDropsTheWorkspacesForMembershipCommandsOnly)
{
    for (const char* command : { proto::router::kCommandUserAdd,
                                 proto::router::kCommandUserModify,
                                 proto::router::kCommandUserDelete })
    {
        fillAll();

        cache_.onResult(RouterCache::Result::USER, command, true);

        EXPECT_FALSE(cache_.workspacesLoaded()) << "command: " << command;
        EXPECT_NE(cache_.hostList(kHostKey), nullptr) << "command: " << command;
        EXPECT_NE(cache_.groupList(kWorkspaceId), nullptr) << "command: " << command;
    }

    for (const char* command : { proto::router::kCommandUserResetOtp,
                                 proto::router::kCommandUserRevokeTokens })
    {
        fillAll();

        cache_.onResult(RouterCache::Result::USER, command, true);

        EXPECT_TRUE(cache_.workspacesLoaded()) << "command: " << command;
        EXPECT_NE(cache_.hostList(kHostKey), nullptr) << "command: " << command;
        EXPECT_NE(cache_.groupList(kWorkspaceId), nullptr) << "command: " << command;
    }
}

//--------------------------------------------------------------------------------------------------
// A notification is the router saying a change made elsewhere left what we hold out of date. Each
// flag names its list one to one - the router computed the reach of the operation on its side.
TEST_F(RouterCacheTest, NotificationDropsOnlyTheListsItNames)
{
    fillAll();

    proto::router::Notification hosts;
    hosts.set_hosts_dirty(true);
    cache_.onNotification(hosts);

    EXPECT_EQ(cache_.hostList(kHostKey), nullptr);
    EXPECT_NE(cache_.groupList(kWorkspaceId), nullptr);
    EXPECT_TRUE(cache_.workspacesLoaded());

    fillAll();

    proto::router::Notification groups;
    groups.set_groups_dirty(true);
    cache_.onNotification(groups);

    EXPECT_EQ(cache_.groupList(kWorkspaceId), nullptr);
    EXPECT_NE(cache_.hostList(kHostKey), nullptr);

    fillAll();

    proto::router::Notification workspaces;
    workspaces.set_workspaces_dirty(true);
    cache_.onNotification(workspaces);

    EXPECT_FALSE(cache_.workspacesLoaded());
    EXPECT_NE(cache_.hostList(kHostKey), nullptr);
}

//--------------------------------------------------------------------------------------------------
// What the notification says nothing about is still good: those lists are not cached here at all,
// and dropping the ones that are would cost a reload for nothing.
TEST_F(RouterCacheTest, NotificationOfSomethingElseKeepsTheLists)
{
    fillAll();

    proto::router::Notification notification;
    notification.set_users_dirty(true);
    notification.set_relays_dirty(true);
    notification.set_clients_dirty(true);
    notification.set_temp_hosts_dirty(true);
    cache_.onNotification(notification);

    EXPECT_TRUE(cache_.workspacesLoaded());
    EXPECT_NE(cache_.groupList(kWorkspaceId), nullptr);
    EXPECT_NE(cache_.hostList(kHostKey), nullptr);
}

//--------------------------------------------------------------------------------------------------
// The session is suspended or gone: nothing it fetched is known to be current any more.
TEST_F(RouterCacheTest, ClearDropsEverything)
{
    fillAll();

    cache_.clear();

    EXPECT_FALSE(cache_.workspacesLoaded());
    EXPECT_EQ(cache_.hostList(kHostKey), nullptr);
    EXPECT_EQ(cache_.groupList(kWorkspaceId), nullptr);
    EXPECT_TRUE(cache_.workspaceList().workspaces.isEmpty());
}
