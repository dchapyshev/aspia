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

#include "client/router_session.h"

#include <gtest/gtest.h>

#include <QObject>
#include <QTemporaryDir>

#include <optional>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/secure_byte_array.h"
#include "client/database.h"
#include "client/router_controller.h"
#include "client/router_test_fixture.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"

namespace {

constexpr qint64 kRouterId = 1;
constexpr qint64 kWorkspaceId = 10;

} // namespace

// Test-only access to the identity, the pending replies and the incoming messages.
class RouterSessionTestPeer
{
public:
    static RouterRpc& rpc(RouterSession& router) { return router.rpc_; }

    // The replies arrive parsed and typed, the way the owner hands them in.
    template <class Message>
    static void onMessageReceived(RouterSession& router, const Message& message)
    {
        router.onMessageReceived(message);
    }
};

// A session without a worker: what it sends goes nowhere, the replies are handed to it parsed,
// as the owner does. The request ids are handed out sequentially from one, so the fixture
// counts the requests the tests make and builds every reply by that count. The cache and the
// rpc have tests of their own; here the conversation is under test.
class RouterSessionTest : public RouterTestFixture
{
protected:
    RouterSessionTest()
        : router_(config(), kUserId, QVersionNumber(3, 0, 0))
    {
        // Nothing
    }

    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());
        DatabaseTestPeer::setFilePath(temp_dir_.path() + "/client.db3");
        ASSERT_TRUE(Database::instance().isValid());

        // The records of the book are sealed with the master key; the tests run against an
        // unlocked one.
        DataCryptor::instance().setKey(SecureByteArray(QByteArray(32, 'k')));
    }

    static SharedPointer<RouterConfig> config(const QByteArray& device_token = QByteArray())
    {
        RouterConfig* config = new RouterConfig();
        config->setRouterId(kRouterId);
        config->setAddress("router.example.com");
        config->setUsername("user");
        config->setPassword(SecureString(QString("secret")));
        config->setDeviceToken(device_token);
        return SharedPointer<RouterConfig>(config);
    }

    // Puts the record the session works with into the isolated book.
    void seedRecord(const QByteArray& device_token = QByteArray())
    {
        RouterConfig record = *config(device_token);
        ASSERT_TRUE(Database::instance().addRouter(record));
        ASSERT_EQ(record.routerId(), kRouterId);
    }

    // Hands a reply to the session the way the owner does after parsing the wire.
    template <class Message>
    void deliver(const Message& message)
    {
        RouterSessionTestPeer::onMessageReceived(router_, message);
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
        router_.listWorkspaces(RouterSession::CachePolicy::RELOAD, workspace_id,
                               { &receiver_, store });
        list.set_request_id(++next_request_id_);

        proto::router::RouterToClient reply;
        reply.mutable_workspace_list()->Swap(&list);
        deliver(reply);

        return delivered;
    }

    RouterHostList fetchHosts(proto::router::HostList list)
    {
        RouterHostList delivered;
        auto store = [&delivered](const RouterHostList& value) { delivered = value; };
        router_.listHosts(RouterSession::CachePolicy::RELOAD, hostListRequest(),
                          { &receiver_, store });
        list.set_request_id(++next_request_id_);

        proto::router::RouterToClient reply;
        reply.mutable_host_list()->Swap(&list);
        deliver(reply);

        return delivered;
    }

    RouterGroupList fetchGroups(qint64 workspace_id, proto::router::GroupList list)
    {
        RouterGroupList delivered;
        auto store = [&delivered](const RouterGroupList& value) { delivered = value; };
        router_.listGroups(RouterSession::CachePolicy::RELOAD, workspace_id, { &receiver_, store });
        list.set_request_id(++next_request_id_);

        proto::router::RouterToClient reply;
        reply.mutable_group_list()->Swap(&list);
        deliver(reply);

        return delivered;
    }

    // A cached list is one the session answers on its own, synchronously. A miss goes to the
    // wire and consumes a request id.
    bool workspacesServedFromCache()
    {
        bool answered = false;
        auto probe = [&answered](const RouterWorkspaceList&) { answered = true; };
        router_.listWorkspaces(RouterSession::CachePolicy::USE_CACHE, 0, { &receiver_, probe });
        if (!answered)
            ++next_request_id_;
        return answered;
    }

    bool groupsServedFromCache(qint64 workspace_id = kWorkspaceId)
    {
        bool answered = false;
        auto probe = [&answered](const RouterGroupList&) { answered = true; };
        router_.listGroups(RouterSession::CachePolicy::USE_CACHE, workspace_id,
                           { &receiver_, probe });
        if (!answered)
            ++next_request_id_;
        return answered;
    }

    bool hostsServedFromCache()
    {
        bool answered = false;
        auto probe = [&answered](const RouterHostList&) { answered = true; };
        router_.listHosts(RouterSession::CachePolicy::USE_CACHE, hostListRequest(),
                          { &receiver_, probe });
        if (!answered)
            ++next_request_id_;
        return answered;
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

    QTemporaryDir temp_dir_;
    QObject receiver_;
    RouterSession router_;
    qint64 next_request_id_ = 0;
};

//--------------------------------------------------------------------------------------------------
// The list arrives as the router stored it, membership included.
TEST_F(RouterSessionTest, WorkspaceListIsParsed)
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
TEST_F(RouterSessionTest, FailedWorkspaceListChangesNothing)
{
    proto::router::WorkspaceList failed;
    failed.set_error_code(proto::router::kErrorInternalError);
    fetchWorkspaces(0, failed);

    EXPECT_FALSE(workspacesServedFromCache());
}

//--------------------------------------------------------------------------------------------------
// The whole conversation seen from outside: the caller asks, and the reply carrying the id of
// the request reaches the caller through the admin channel.
TEST_F(RouterSessionTest, ListUsersConversation)
{
    int calls = 0;
    router_.listUsers(0, proto::router::kMaxUserPageSize,
                      { &receiver_, [&calls](const proto::router::UserList&) { ++calls; } });
    EXPECT_EQ(calls, 0);

    proto::router::RouterToAdmin reply;
    reply.mutable_user_list()->set_request_id(++next_request_id_);
    reply.mutable_user_list()->set_error_code(proto::router::kErrorOk);
    deliver(reply);

    EXPECT_EQ(calls, 1);
}

//--------------------------------------------------------------------------------------------------
// The same conversation on the manager channel, which carries the records of a workspace.
TEST_F(RouterSessionTest, GroupConversationUsesTheManagerChannel)
{
    RouterGroup group;
    group.name = "servers";

    int calls = 0;
    router_.addGroup(kWorkspaceId, group,
                     { &receiver_, [&calls](const proto::router::GroupResult&) { ++calls; } });
    EXPECT_EQ(calls, 0);

    proto::router::RouterToManager reply;
    reply.mutable_group_result()->set_request_id(++next_request_id_);
    reply.mutable_group_result()->set_error_code(proto::router::kErrorOk);
    deliver(reply);

    EXPECT_EQ(calls, 1);
}

//--------------------------------------------------------------------------------------------------
// And on the client channel, where the status of a host is asked before a connection.
TEST_F(RouterSessionTest, HostStatusConversationUsesTheClientChannel)
{
    int calls = 0;
    router_.checkHostStatus(
        HostId(1), { &receiver_, [&calls](const proto::router::HostStatus&) { ++calls; } });
    EXPECT_EQ(calls, 0);

    proto::router::RouterToClient reply;
    reply.mutable_host_status()->set_request_id(++next_request_id_);
    reply.mutable_host_status()->set_error_code(proto::router::kErrorOk);
    deliver(reply);

    EXPECT_EQ(calls, 1);
}

//--------------------------------------------------------------------------------------------------
// The offer of the router comes back to whoever asked for the connection.
TEST_F(RouterSessionTest, ConnectionRequestBringsTheOffer)
{
    std::string received_error;
    router_.requestConnection(HostId(7),
        { &receiver_, [&received_error](const proto::router::ConnectionOffer& offer)
    {
        received_error = offer.error_code();
    } });

    proto::router::RouterToClient reply;
    proto::router::ConnectionOffer* offer = reply.mutable_connection_offer();
    offer->set_request_id(++next_request_id_);
    offer->set_error_code(proto::router::kErrorOk);
    deliver(reply);

    EXPECT_EQ(received_error, proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
// The caller receives the rows of the reply, and the next one that accepts a cached answer is
// without a request.
TEST_F(RouterSessionTest, HostListIsParsedAndCached)
{
    proto::router::HostList list = hostList(kWorkspaceId, { HostId(1) }, 25);
    list.mutable_host(0)->set_comment("comment");

    const RouterHostList delivered = fetchHosts(list);

    ASSERT_EQ(delivered.hosts.size(), 1);
    EXPECT_EQ(delivered.hosts.at(0).comment, "comment");

    // The count of the whole scope drives the pagination of the client, so it must survive the
    // cache: a cached answer with a zero count would collapse the page list.
    RouterHostList cached;
    router_.listHosts(RouterSession::CachePolicy::USE_CACHE, hostListRequest(),
                      { &receiver_, [&cached](const RouterHostList& value) { cached = value; } });

    ASSERT_EQ(cached.hosts.size(), 1);
    EXPECT_EQ(cached.hosts.at(0).comment, "comment");
    EXPECT_EQ(cached.total_count, 25);
    EXPECT_EQ(cached.workspace_id, kWorkspaceId);
    EXPECT_EQ(cached.error_code, QString::fromStdString(proto::router::kErrorOk));
}

//--------------------------------------------------------------------------------------------------
// The groups arrive with the workspace they belong to and are cached per workspace.
TEST_F(RouterSessionTest, GroupListIsParsedAndCached)
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
// The accepted rotation retires the stored device token: the router revoked every token of the
// account in the same transaction, so the next login goes straight to the code prompt.
TEST_F(RouterSessionTest, AcceptedPasswordRotationDropsTheStoredToken)
{
    seedRecord("stale-device-token");

    SharedPointer<RouterConfig> shared = config("stale-device-token");
    RouterSession router(shared, kUserId, QVersionNumber(3, 0, 0));

    int calls = 0;
    router.changePassword(SecureString(QString("new-password")),
                          { &receiver_, [&calls](const proto::router::ChangePasswordResult&)
    {
        ++calls;
    } });

    proto::router::RouterToClient reply;
    proto::router::ChangePasswordResult* result = reply.mutable_change_password_result();
    result->set_request_id(1);
    result->set_error_code(proto::router::kErrorOk);
    RouterSessionTestPeer::onMessageReceived(router, reply);

    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(shared->deviceToken().isEmpty());
    EXPECT_TRUE(shared->password() == SecureString(QString("new-password")));

    const std::optional<RouterConfig> stored = Database::instance().findRouter(kRouterId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->deviceToken().isEmpty());
    EXPECT_TRUE(stored->password() == SecureString(QString("new-password")));
}

//--------------------------------------------------------------------------------------------------
// The router refused the rotation, so nothing of it reaches the record. The old password is the
// one that still opens the account, and the stored token is still valid on the router.
TEST_F(RouterSessionTest, RefusedPasswordRotationStoresNothing)
{
    seedRecord("stale-device-token");

    SharedPointer<RouterConfig> shared = config("stale-device-token");
    RouterSession router(shared, kUserId, QVersionNumber(3, 0, 0));

    int calls = 0;
    router.changePassword(SecureString(QString("new-password")),
                          { &receiver_, [&calls](const proto::router::ChangePasswordResult&)
    {
        ++calls;
    } });

    proto::router::RouterToClient reply;
    proto::router::ChangePasswordResult* result = reply.mutable_change_password_result();
    result->set_request_id(1);
    result->set_error_code(proto::router::kErrorInternalError);
    RouterSessionTestPeer::onMessageReceived(router, reply);

    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(shared->deviceToken().isEmpty());
    EXPECT_TRUE(shared->password() == SecureString(QString("secret")));

    const std::optional<RouterConfig> stored = Database::instance().findRouter(kRouterId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->deviceToken().isEmpty());
    EXPECT_TRUE(stored->password() == SecureString(QString("secret")));
}

//--------------------------------------------------------------------------------------------------
// A rename of the own account rides the same store as the rotation. The next login authenticates
// with the name of the record, so a record that kept the old name would never connect again.
TEST_F(RouterSessionTest, StoreCredentialsWritesTheNewName)
{
    seedRecord();

    SharedPointer<RouterConfig> shared = config();
    RouterSession router(shared, kUserId, QVersionNumber(3, 0, 0));

    EXPECT_TRUE(router.storeCredentials("renamed", SecureString(QString("new-password"))));

    EXPECT_EQ(shared->username(), QString("renamed"));

    const std::optional<RouterConfig> stored = Database::instance().findRouter(kRouterId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->username(), QString("renamed"));
    EXPECT_TRUE(stored->password() == SecureString(QString("new-password")));
}

//--------------------------------------------------------------------------------------------------
// The record can be gone by the time the router accepts the rotation. The store says so instead
// of pretending the record now holds the new password, and the journal of the record tells the
// operator what to do.
TEST_F(RouterSessionTest, CredentialsOfAGoneRecordAreReportedUnstored)
{
    // The controller does not run in this stand; the store reaches it only for the journal line.
    RouterController controller;

    SharedPointer<RouterConfig> shared = config();
    RouterSession router(shared, kUserId, QVersionNumber(3, 0, 0));

    int warnings = 0;
    QObject::connect(&controller, &RouterController::sig_event,
                     &receiver_, [&warnings](qint64 router_id, const RouterEvent& event)
    {
        if (router_id == kRouterId && event.severity == RouterEvent::Severity::WARNING)
            ++warnings;
    });

    EXPECT_FALSE(router.storeCredentials("user", SecureString(QString("new-password"))));
    EXPECT_EQ(warnings, 1);
}

//--------------------------------------------------------------------------------------------------
// The session does not watch the connection; its owner destroys it when the connection dies.
// The death answers everyone the session still owes, once.
TEST_F(RouterSessionTest, DyingSessionAnswersItsCallers)
{
    SharedPointer<RouterConfig> other = config();
    other->setRouterId(kRouterId + 1);

    int calls = 0;
    std::string last_error;

    {
        RouterSession router(other, kUserId, QVersionNumber(3, 0, 0));
        router.listUsers(0, proto::router::kMaxUserPageSize,
                         { &receiver_, [&](const proto::router::UserList& list)
        {
            ++calls;
            last_error = list.error_code();
        } });

        EXPECT_EQ(calls, 0);
    }

    EXPECT_EQ(calls, 1);
    EXPECT_EQ(last_error, proto::router::kErrorLostConnection);
}

//--------------------------------------------------------------------------------------------------
// A reply nobody waits for any more (the dialog was closed) still carries its cache rules. What
// the rules are is the business of RouterCache; here every reply channel must feed them.
TEST_F(RouterSessionTest, ReplyWithoutARequestStillAppliesItsRules)
{
    fillCaches();

    deliver(workspaceResult(proto::router::kCommandWorkspaceModify, proto::router::kErrorOk));
    EXPECT_FALSE(workspacesServedFromCache());

    fillCaches();

    deliver(groupResult(proto::router::kCommandGroupDelete, proto::router::kErrorOk));
    EXPECT_FALSE(groupsServedFromCache());
    EXPECT_FALSE(hostsServedFromCache());
}

//--------------------------------------------------------------------------------------------------
// A change notification names the lists that went stale. It arrives like any other client
// message; the named list leaves the cache and the others stay.
TEST_F(RouterSessionTest, NotificationDropsItsList)
{
    fillCaches();

    proto::router::RouterToClient reply;
    reply.mutable_notification()->set_hosts_dirty(true);
    deliver(reply);

    EXPECT_FALSE(hostsServedFromCache());
    EXPECT_TRUE(groupsServedFromCache());
}

//--------------------------------------------------------------------------------------------------
// An unsendable record is refused before the wire, in the same terms the router would answer with,
// so the caller is not left waiting. The bounds count UTF-8 bytes, so a name of 64 non-ASCII
// characters is over the bound while the input field that accepted it is not; the name of a group
// or a workspace is also mandatory and judged after trimming, the way the router judges it.
TEST_F(RouterSessionTest, RefusedRecordIsAnsweredWithoutTouchingTheWire)
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

    EXPECT_EQ(RouterSessionTestPeer::rpc(router_).pendingCount(), 0);
}

//--------------------------------------------------------------------------------------------------
// A record sitting exactly on the bounds goes out. The name is measured after trimming, the way
// the router measures it, so a name padded with blanks up to twice the bound still passes.
TEST_F(RouterSessionTest, RecordOnTheBoundsIsSent)
{
    RouterWorkspace workspace;
    workspace.entry_id = kWorkspaceId;
    workspace.name = "  " + QString(proto::router::kMaxEntryNameLength, QChar('n')) + "  ";

    bool answered = false;
    router_.modifyWorkspace(workspace,
                            { &receiver_, [&answered](const proto::router::WorkspaceResult&)
    {
        answered = true;
    } });
    ++next_request_id_;

    EXPECT_FALSE(answered);
    EXPECT_EQ(RouterSessionTestPeer::rpc(router_).pendingCount(), 1);
}

//--------------------------------------------------------------------------------------------------
// The count of a scope is never negative. One that arrives so is not data the pagination can work
// with - it takes a non-negative count as its contract and ends the process on anything else - so
// the parsing of the reply is where it stops.
TEST_F(RouterSessionTest, NegativeTotalCountDoesNotReachTheCallers)
{
    const RouterHostList delivered = fetchHosts(hostList(kWorkspaceId, { HostId(1) }, -5));
    EXPECT_EQ(delivered.total_count, 0);

    RouterHostList found;
    router_.searchHosts("host", 0, 25,
                        { &receiver_, [&found](const RouterHostList& value) { found = value; } });

    proto::router::RouterToClient reply;
    auto* result = reply.mutable_host_search_result();
    result->set_request_id(++next_request_id_);
    result->set_error_code(proto::router::kErrorOk);
    result->set_total_count(-5);
    deliver(reply);

    EXPECT_EQ(found.error_code, QString::fromStdString(proto::router::kErrorOk));
    EXPECT_EQ(found.total_count, 0);
}
