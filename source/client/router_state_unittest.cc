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

#include <QHash>
#include <QObject>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/private_key_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/sealed_box.h"
#include "base/peer/router_user.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"

namespace {

const char kUserName[] = "admin";
const char kPassword[] = "Password1234!";

constexpr qint64 kUserId = 1;
constexpr qint64 kWorkspaceId = 10;

const RouterState::HostCacheKey kHostKey { kWorkspaceId, 0, 0, 100 };

} // namespace

// The client side of a router session: the keys it holds, the replies it decodes and the lists it
// caches. No socket and no database are involved, so every rule can be stated as an assertion.
class RouterStateTest : public testing::Test
{
protected:
    void SetUp() override
    {
        user_ = RouterUser::create(QString::fromUtf8(kUserName), SecureString(kPassword));
        ASSERT_TRUE(user_.isValid());
    }

    const SecureByteArray& groupKey(qint64 workspace_id)
    {
        if (!group_keys_.contains(workspace_id))
            group_keys_.insert(workspace_id, SecureByteArray(Random::byteArray(32)));
        return group_keys_[workspace_id];
    }

    // The UserKeys message the router sends right after the two-factor stage.
    proto::router::UserKeys userKeys(const QList<qint64>& workspace_ids)
    {
        proto::router::UserKeys keys;
        keys.set_user_id(kUserId);
        keys.set_name(kUserName);
        keys.set_public_key(user_.public_key.toStdString());
        keys.set_wrap_private_key(user_.wrap_private_key.toStdString());
        keys.set_wrap_salt(user_.wrap_salt.toStdString());

        for (qint64 workspace_id : workspace_ids)
        {
            proto::router::UserKeys::WorkspaceKey* key = keys.add_workspace_key();
            key->set_workspace_id(workspace_id);
            key->set_wrapped_gk(
                SealedBox::seal(groupKey(workspace_id), user_.public_key).toStdString());
        }

        return keys;
    }

    // Loads the identity and the keys of the given workspaces into |state_|.
    void loadKeys(const QList<qint64>& workspace_ids)
    {
        ASSERT_EQ(state_.applyUserKeys(userKeys(workspace_ids), SecureString(kPassword)),
                  RouterState::KeysResult::OK);
    }

    std::string encrypt(qint64 workspace_id, const QString& plaintext)
    {
        const DataCryptor cryptor(CipherType::AES256_GCM, groupKey(workspace_id));
        std::optional<QByteArray> encrypted = cryptor.encrypt(plaintext.toUtf8());
        if (!encrypted.has_value())
            return std::string();
        return encrypted->toStdString();
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

    RouterUser user_;
    QHash<qint64, SecureByteArray> group_keys_;
    RouterState state_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(RouterStateTest, UserKeysLoadIdentityAndWorkspaceKeys)
{
    ASSERT_EQ(state_.applyUserKeys(userKeys({10, 20}), SecureString(kPassword)),
              RouterState::KeysResult::OK);

    EXPECT_EQ(state_.userId(), kUserId);
    EXPECT_EQ(state_.userName(), QString::fromUtf8(kUserName));
    EXPECT_TRUE(state_.hasWorkspaceKey(10));
    EXPECT_TRUE(state_.hasWorkspaceKey(20));
    EXPECT_FALSE(state_.hasWorkspaceKey(30));
}

//--------------------------------------------------------------------------------------------------
// A record from before the key pair existed: the user must change its password before it can hold
// any workspace key.
TEST_F(RouterStateTest, UserKeysWithoutWrappedPrivateKeyAskForPasswordChange)
{
    proto::router::UserKeys keys = userKeys({10});
    keys.clear_wrap_private_key();

    EXPECT_EQ(state_.applyUserKeys(keys, SecureString(kPassword)),
              RouterState::KeysResult::PASSWORD_CHANGE_REQUIRED);
    EXPECT_FALSE(state_.hasWorkspaceKey(10));
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterStateTest, UserKeysWithWrongPasswordDoNotOpen)
{
    EXPECT_EQ(state_.applyUserKeys(userKeys({10}), SecureString("wrong-password")),
              RouterState::KeysResult::DECRYPT_FAILED);
    EXPECT_FALSE(state_.hasWorkspaceKey(10));
}

//--------------------------------------------------------------------------------------------------
// A key sealed to somebody else is skipped instead of failing the whole session: the rest of the
// workspaces stay usable.
TEST_F(RouterStateTest, UnopenableWorkspaceKeyIsSkipped)
{
    proto::router::UserKeys keys = userKeys({10});

    proto::router::UserKeys::WorkspaceKey* foreign = keys.add_workspace_key();
    foreign->set_workspace_id(20);
    foreign->set_wrapped_gk(SealedBox::seal(groupKey(20),
        RouterUser::create(QStringLiteral("other"), SecureString(kPassword)).public_key)
            .toStdString());

    ASSERT_EQ(state_.applyUserKeys(keys, SecureString(kPassword)), RouterState::KeysResult::OK);

    EXPECT_TRUE(state_.hasWorkspaceKey(10));
    EXPECT_FALSE(state_.hasWorkspaceKey(20));
}

//--------------------------------------------------------------------------------------------------
// The list carries the workspace key of every member entry of ours, so a session can open a
// workspace it learned about after the UserKeys message.
TEST_F(RouterStateTest, WorkspaceListDecryptsCommentAndAddsKey)
{
    loadKeys({});

    const RouterWorkspaceList decoded =
        state_.applyWorkspaceList(workspaceList({10}, QStringLiteral("secret")), 0);

    ASSERT_EQ(decoded.error_code, QString::fromStdString(proto::router::kErrorOk));
    ASSERT_EQ(decoded.workspaces.size(), 1);
    EXPECT_EQ(decoded.workspaces.at(0).comment, QStringLiteral("secret"));
    EXPECT_TRUE(state_.hasWorkspaceKey(10));
}

//--------------------------------------------------------------------------------------------------
// Losing access to a workspace must take its group key with it: keeping the key would let this
// session keep decrypting the records of a workspace it no longer belongs to and, worse, hand the
// key to a user it grants access to.
TEST_F(RouterStateTest, CompleteListDropsKeysOfLostWorkspaces)
{
    loadKeys({10, 20});

    state_.applyWorkspaceList(workspaceList({10}), 0);

    EXPECT_TRUE(state_.hasWorkspaceKey(10));
    EXPECT_FALSE(state_.hasWorkspaceKey(20));
}

//--------------------------------------------------------------------------------------------------
// A reply about one workspace says nothing about the others, so it must not drop their keys.
TEST_F(RouterStateTest, SingleWorkspaceReplyKeepsOtherKeys)
{
    loadKeys({10, 20});

    state_.applyWorkspaceList(workspaceList({10}), 10);

    EXPECT_TRUE(state_.hasWorkspaceKey(10));
    EXPECT_TRUE(state_.hasWorkspaceKey(20));
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

    EXPECT_TRUE(state_.hasWorkspaceKey(10));
    EXPECT_TRUE(state_.hasWorkspaceKey(20));
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
    EXPECT_TRUE(state_.hasWorkspaceKey(10));
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterStateTest, HostFieldsAreDecryptedWithTheWorkspaceKey)
{
    loadKeys({10});

    proto::router::HostList list = hostList(10, {HostId(1)}, 1);
    list.mutable_host(0)->set_comment(encrypt(10, QStringLiteral("comment")));
    list.mutable_host(0)->set_user_name(encrypt(10, QStringLiteral("user")));
    list.mutable_host(0)->set_password(encrypt(10, QStringLiteral("password")));

    const RouterHostList decoded = state_.applyHostList(list, RouterState::HostCacheKey(), false);

    ASSERT_EQ(decoded.hosts.size(), 1);
    EXPECT_EQ(decoded.hosts.at(0).comment, QStringLiteral("comment"));
    EXPECT_EQ(decoded.hosts.at(0).user_name, QStringLiteral("user"));
    EXPECT_EQ(decoded.hosts.at(0).password.toString(), QStringLiteral("password"));
}

//--------------------------------------------------------------------------------------------------
// Without the key the encrypted fields stay empty; the plain ones still arrive, so the host is
// listed instead of disappearing.
TEST_F(RouterStateTest, HostOfUnknownWorkspaceKeepsPlainFieldsOnly)
{
    loadKeys({10});

    proto::router::HostList list = hostList(20, {HostId(1)}, 1);
    list.mutable_host(0)->set_comment(encrypt(20, QStringLiteral("comment")));

    const RouterHostList decoded = state_.applyHostList(list, RouterState::HostCacheKey(), false);

    ASSERT_EQ(decoded.hosts.size(), 1);
    EXPECT_EQ(decoded.hosts.at(0).display_name, QStringLiteral("host"));
    EXPECT_TRUE(decoded.hosts.at(0).comment.isEmpty());
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
    group->set_comment(encrypt(10, QStringLiteral("group comment")));

    const RouterGroupList decoded = state_.applyGroupList(list);

    ASSERT_EQ(decoded.groups.size(), 1);
    EXPECT_EQ(decoded.groups.at(0).comment, QStringLiteral("group comment"));
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
TEST_F(RouterStateTest, DispatchInvokesTheHandlerOnce)
{
    QObject receiver;
    int calls = 0;

    proto::router::UserListRequest request;
    request.set_request_id(state_.nextRequestId());
    state_.registerPending<proto::router::UserList>(&request, &receiver,
        [&calls](const proto::router::UserList&) { ++calls; });

    EXPECT_EQ(state_.pendingCount(), 1);

    proto::router::UserList response;
    response.set_request_id(request.request_id());

    state_.dispatch(request.request_id(), response);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(state_.pendingCount(), 0);

    // A second reply with the same id has nothing to deliver to.
    state_.dispatch(request.request_id(), response);
    EXPECT_EQ(calls, 1);
}

//--------------------------------------------------------------------------------------------------
// The receiver is a widget that can be closed while its request is in flight.
TEST_F(RouterStateTest, DispatchSkipsDestroyedReceiver)
{
    int calls = 0;

    proto::router::UserListRequest request;
    request.set_request_id(state_.nextRequestId());

    {
        QObject receiver;
        state_.registerPending<proto::router::UserList>(&request, &receiver,
            [&calls](const proto::router::UserList&) { ++calls; });
    }

    proto::router::UserList response;
    state_.dispatch(request.request_id(), response);

    EXPECT_EQ(calls, 0);
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
    request.set_request_id(state_.nextRequestId());

    const RouterState::HostCacheKey key{ 10, 0, 0, 0 };
    state_.registerPending<proto::router::HostList>(&request, &receiver,
        [&delivered](const RouterHostList& list) { delivered = list; },
        [this, key](const proto::router::HostList& raw)
    {
        return state_.applyHostList(raw, key, true);
    });

    state_.dispatch(request.request_id(), hostList(10, {HostId(1)}, 1));

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

    proto::router::UserListRequest request;
    request.set_request_id(state_.nextRequestId());
    state_.registerPending<proto::router::UserList>(&request, &receiver,
        [&calls](const proto::router::UserList&) { ++calls; });

    state_.clearCaches();
    state_.clearPending();

    EXPECT_EQ(state_.pendingCount(), 0);
    EXPECT_FALSE(state_.workspacesLoaded());
    EXPECT_EQ(state_.cachedHostList(key), nullptr);

    // A late reply to a request of the dead window must not reach the caller.
    proto::router::UserList response;
    state_.dispatch(request.request_id(), response);
    EXPECT_EQ(calls, 0);

    // The keys survive: the session is being re-authenticated, not lost.
    EXPECT_TRUE(state_.hasWorkspaceKey(10));
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterStateTest, ClearedSessionKeepsNoKeys)
{
    loadKeys({10});
    state_.clearSession();

    EXPECT_EQ(state_.userId(), 0);
    EXPECT_FALSE(state_.hasWorkspaceKey(10));
    EXPECT_EQ(state_.pendingCount(), 0);
}

//--------------------------------------------------------------------------------------------------
// The key travels only for a user that is being granted access; for the others the router keeps
// the entry it already stores.
TEST_F(RouterStateTest, WorkspaceSaveSealsForNewMembersOnly)
{
    loadKeys({10});

    const RouterUser other = RouterUser::create(QStringLiteral("other"), SecureString(kPassword));

    RouterWorkspace workspace;
    workspace.entry_id = 10;
    workspace.name = QStringLiteral("alpha");
    workspace.comment = QStringLiteral("comment");
    workspace.revision = 3;
    workspace.access.append({ kUserId, QByteArray() });          // Already a member.
    workspace.access.append({ 2, other.public_key });            // Newly granted.
    workspace.host_ids.append(HostId(7));

    proto::router::Workspace out;
    ASSERT_TRUE(state_.buildWorkspace(workspace, &out));

    EXPECT_EQ(out.entry_id(), 10);
    EXPECT_EQ(out.revision(), 3);
    ASSERT_EQ(out.access_size(), 2);
    EXPECT_TRUE(out.access(0).wrapped_gk().empty());
    EXPECT_TRUE(out.access(0).public_key().empty());
    EXPECT_FALSE(out.access(1).wrapped_gk().empty());
    EXPECT_EQ(out.access(1).public_key(), other.public_key.toStdString());
    ASSERT_EQ(out.host_id_size(), 1);
    EXPECT_EQ(out.host_id(0), 7u);

    // The new member can open exactly the key of this workspace.
    const SecureByteArray private_key = PrivateKeyCryptor::decrypt(
        other.wrap_private_key, SecureString(kPassword), other.wrap_salt);
    ASSERT_FALSE(private_key.isEmpty());

    const std::optional<SecureByteArray> opened = SealedBox::open(
        QByteArray::fromStdString(out.access(1).wrapped_gk()),
        KeyPair::fromPrivateKey(private_key));
    ASSERT_TRUE(opened.has_value());
    EXPECT_EQ(*opened, groupKey(10));
}

//--------------------------------------------------------------------------------------------------
// Without the key of an existing workspace the save would encrypt its comment with a key nobody
// has, and the entries of the new members would be sealed to it as well.
TEST_F(RouterStateTest, WorkspaceSaveWithoutItsKeyIsRefused)
{
    loadKeys({10});

    RouterWorkspace workspace;
    workspace.entry_id = 20;
    workspace.name = QStringLiteral("beta");

    proto::router::Workspace out;
    EXPECT_FALSE(state_.buildWorkspace(workspace, &out));
}

//--------------------------------------------------------------------------------------------------
// A workspace being created has no id yet, so its key is generated here - and must not be stored
// under the id 0, or the next created workspace would silently reuse it.
TEST_F(RouterStateTest, NewWorkspaceGetsAFreshKeyThatIsNotKept)
{
    loadKeys({});

    RouterWorkspace workspace;
    workspace.name = QStringLiteral("alpha");
    workspace.comment = QStringLiteral("comment");

    proto::router::Workspace first;
    ASSERT_TRUE(state_.buildWorkspace(workspace, &first));
    EXPECT_FALSE(state_.hasWorkspaceKey(0));

    proto::router::Workspace second;
    ASSERT_TRUE(state_.buildWorkspace(workspace, &second));

    // Two creations of the same workspace data do not produce the same ciphertext: the keys differ.
    EXPECT_NE(first.comment(), second.comment());
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterStateTest, HostAndGroupSavesRequireTheWorkspaceKey)
{
    loadKeys({10});

    RouterHost host;
    host.host_id = HostId(1);
    host.workspace_id = 20;
    host.comment = QStringLiteral("comment");

    proto::router::Host host_out;
    EXPECT_FALSE(state_.buildHost(host, &host_out));

    host.workspace_id = 10;
    ASSERT_TRUE(state_.buildHost(host, &host_out));
    EXPECT_FALSE(host_out.comment().empty());

    RouterGroup group;
    group.name = QStringLiteral("servers");
    group.comment = QStringLiteral("comment");

    proto::router::Group group_out;
    EXPECT_FALSE(state_.buildGroup(20, group, &group_out));
    EXPECT_TRUE(state_.buildGroup(10, group, &group_out));
}

//--------------------------------------------------------------------------------------------------
// Creating an administrator and changing our own password both hand over every workspace key we
// hold, re-sealed to the key pair of the target.
TEST_F(RouterStateTest, ResealCoversEveryHeldWorkspace)
{
    loadKeys({10, 20});

    const RouterUser other = RouterUser::create(QStringLiteral("other"), SecureString(kPassword));

    proto::router::User user;
    state_.resealGroupKeys(other.public_key, &user);

    ASSERT_EQ(user.workspace_key_size(), 2);

    const SecureByteArray private_key = PrivateKeyCryptor::decrypt(
        other.wrap_private_key, SecureString(kPassword), other.wrap_salt);
    ASSERT_FALSE(private_key.isEmpty());

    for (int i = 0; i < user.workspace_key_size(); ++i)
    {
        const std::optional<SecureByteArray> opened = SealedBox::open(
            QByteArray::fromStdString(user.workspace_key(i).wrapped_gk()),
            KeyPair::fromPrivateKey(private_key));
        ASSERT_TRUE(opened.has_value());
        EXPECT_EQ(*opened, groupKey(user.workspace_key(i).workspace_id()));
    }

    // A workspace we lost access to is not offered to anybody any more.
    state_.applyWorkspaceList(workspaceList({10}), 0);

    proto::router::User after;
    state_.resealGroupKeys(other.public_key, &after);
    ASSERT_EQ(after.workspace_key_size(), 1);
    EXPECT_EQ(after.workspace_key(0).workspace_id(), 10);
}

//--------------------------------------------------------------------------------------------------
// A reply of the administrator channel finds the request that is waiting for it.
TEST_F(RouterStateTest, AdminReplyReachesItsRequest)
{
    QObject receiver;
    int calls = 0;

    proto::router::UserListRequest request;
    request.set_request_id(state_.nextRequestId());
    state_.registerPending<proto::router::UserList>(&request, &receiver,
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
    request.set_request_id(state_.nextRequestId());
    state_.registerPending<proto::router::GroupResult>(&request, &receiver,
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
    request.set_request_id(state_.nextRequestId());
    state_.registerPending<proto::router::HostStatus>(&request, &receiver,
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
