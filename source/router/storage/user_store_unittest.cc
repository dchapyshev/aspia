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

#include "router/storage/user_store.h"

#include <QUuid>

#include "router/router_test_base.h"

class UserStoreTest : public RouterTestBase
{
protected:
    using RouterTestBase::addWorkspace;

    qint64 addWorkspace(const QString& name, const std::vector<qint64>& access)
    {
        qint64 entry_id = -1;
        const std::string_view error_code =
            workspaces_.add(name.toStdString(), std::string_view(), access, &entry_id);
        if (error_code != proto::router::kErrorOk)
            return -1;
        return entry_id;
    }

    qint64 workspaceRevision(qint64 workspace_id)
    {
        proto::router::WorkspaceList list;
        workspaces_.listForAdmin(workspace_id, &list);
        if (list.error_code() != proto::router::kErrorOk || list.workspace_size() != 1)
            return -1;
        return list.workspace(0).revision();
    }
};

//--------------------------------------------------------------------------------------------------
TEST_F(UserStoreTest, AddUserStoresRecord)
{
    RouterUser user = makeUser("bob", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_EQ(users_.add(user), proto::router::kErrorOk);

    const RouterUser stored = findUser("bob");
    EXPECT_GT(stored.entry_id, 1);
    EXPECT_EQ(stored.sessions, proto::router::SESSION_TYPE_OPERATOR);
    EXPECT_EQ(stored.flags, quint32(User::ENABLED));
    EXPECT_EQ(stored.verifier, user.verifier);
}

//--------------------------------------------------------------------------------------------------
// The guid names the user for as long as the record lives. A rename, which always comes with a
// password, leaves it as it is, and a user added later gets another one.
TEST_F(UserStoreTest, GuidOfTheUserSurvivesARename)
{
    ASSERT_EQ(users_.add(makeUser("bob", proto::router::SESSION_TYPE_OPERATOR)), proto::router::kErrorOk);

    const RouterUser bob = findUser("bob");
    ASSERT_FALSE(QUuid::fromString(bob.guid).isNull());
    EXPECT_NE(bob.guid, admin_.guid);

    RouterUser renamed = makeUser("robert", proto::router::SESSION_TYPE_OPERATOR);
    renamed.entry_id = bob.entry_id;
    ASSERT_EQ(users_.modify(renamed), proto::router::kErrorOk);

    const RouterUser robert = findUser("robert");
    EXPECT_EQ(robert.entry_id, bob.entry_id);
    EXPECT_EQ(robert.guid, bob.guid);
}

//--------------------------------------------------------------------------------------------------
// A client names the access level as a single session type; the record holds every session type
// the level implies.
TEST_F(UserStoreTest, AddUserStoresTheFullMaskOfTheLevel)
{
    ASSERT_EQ(users_.add(makeUser("root", proto::router::SESSION_TYPE_ADMIN)), proto::router::kErrorOk);
    ASSERT_EQ(users_.add(makeUser("lead", proto::router::SESSION_TYPE_MANAGER)), proto::router::kErrorOk);

    EXPECT_EQ(findUser("root").sessions, kAllSessions);
    EXPECT_EQ(findUser("lead").sessions,
              proto::router::SESSION_TYPE_MANAGER | proto::router::SESSION_TYPE_OPERATOR);
}

//--------------------------------------------------------------------------------------------------
// The list is read one page at a time, and the same page holds the same records whatever the
// query planner does with it.
TEST_F(UserStoreTest, UserListReturnsRequestedPage)
{
    for (int i = 0; i < 5; ++i)
    {
        ASSERT_EQ(users_.add(makeUser(QString("user%1").arg(i),
                                      proto::router::SESSION_TYPE_OPERATOR)),
                  proto::router::kErrorOk);
    }

    qint64 count = 0;
    ASSERT_EQ(users_.count(&count), proto::router::kErrorOk);
    EXPECT_EQ(count, 6);

    std::vector<RouterUser> users;
    ASSERT_EQ(users_.list(0, 2, &users), proto::router::kErrorOk);
    ASSERT_EQ(users.size(), 2u);
    EXPECT_EQ(users[0].name, "admin");
    EXPECT_EQ(users[1].name, "user0");

    ASSERT_EQ(users_.list(4, 10, &users), proto::router::kErrorOk);
    ASSERT_EQ(users.size(), 2u);
    EXPECT_EQ(users[0].name, "user3");
    EXPECT_EQ(users[1].name, "user4");

    ASSERT_EQ(users_.list(100, 10, &users), proto::router::kErrorOk);
    EXPECT_TRUE(users.empty());
}

//--------------------------------------------------------------------------------------------------
// A request that names no page, or one bigger than the cap, is refused instead of answering the
// whole table.
TEST_F(UserStoreTest, UserListRefusesUnboundedPage)
{
    std::vector<RouterUser> users;
    EXPECT_EQ(users_.list(0, 0, &users), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(users_.list(-1, 10, &users), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(users_.list(0, proto::router::kMaxUserPageSize + 1, &users), proto::router::kErrorInvalidRequest);
}

//--------------------------------------------------------------------------------------------------
// SRP folds the user name to lower case before it derives the verifier, so records differing only
// in case would be one identity with two passwords. The database refuses the second one and finds
// the first whatever case the client types.
TEST_F(UserStoreTest, UserNamesAreCaseFolded)
{
    ASSERT_EQ(users_.add(makeUser("bob", proto::router::SESSION_TYPE_OPERATOR)), proto::router::kErrorOk);
    EXPECT_EQ(users_.add(makeUser("BoB", proto::router::SESSION_TYPE_OPERATOR)), proto::router::kErrorAlreadyExists);

    RouterUser user;
    ASSERT_EQ(users_.find("BOB", &user), proto::router::kErrorOk);
    EXPECT_EQ(user.name, "bob");

    // A rename cannot take the folded name of somebody else either.
    RouterUser renamed = makeUser("BOB", kAllSessions);
    renamed.entry_id = admin_.entry_id;
    EXPECT_EQ(users_.modify(renamed), proto::router::kErrorAlreadyExists);
}

//--------------------------------------------------------------------------------------------------
// A name that belongs to nobody is not a failure of the query.
TEST_F(UserStoreTest, FindUserSeparatesMissAndFailure)
{
    RouterUser user;
    EXPECT_EQ(users_.find("nobody", &user), proto::router::kErrorNotFound);
    EXPECT_EQ(users_.find(qint64(4242), &user), proto::router::kErrorNotFound);
    EXPECT_EQ(users_.find("admin", &user), proto::router::kErrorOk);
    EXPECT_EQ(user.entry_id, admin_.entry_id);
}

//--------------------------------------------------------------------------------------------------
// A modify names the new level as one bit and the row gets its full mask. The change is
// reported, and the same level again is not a change.
TEST_F(UserStoreTest, ModifyUserStoresTheFullMaskOfTheNewLevel)
{
    RouterUser user = makeUser("bob", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_EQ(users_.add(user), proto::router::kErrorOk);
    const qint64 user_id = findUser("bob").entry_id;

    RouterUser promoted = makeUser("bob", proto::router::SESSION_TYPE_MANAGER);
    promoted.entry_id = user_id;

    bool sessions_changed = false;
    ASSERT_EQ(users_.modify(promoted, nullptr, &sessions_changed), proto::router::kErrorOk);
    EXPECT_TRUE(sessions_changed);
    EXPECT_EQ(findUser(user_id).sessions,
              proto::router::SESSION_TYPE_MANAGER | proto::router::SESSION_TYPE_OPERATOR);

    ASSERT_EQ(users_.modify(promoted, nullptr, &sessions_changed), proto::router::kErrorOk);
    EXPECT_FALSE(sessions_changed);
}

//--------------------------------------------------------------------------------------------------
// A request with empty credentials changes only the level and the flags: the stored name and
// credentials survive, so a stale snapshot cannot roll back a concurrent change. Without a level
// the stored one survives too.
TEST_F(UserStoreTest, FlagsOnlyModifyChangesOnlyFlags)
{
    RouterUser user = makeUser("bob", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_EQ(users_.add(user), proto::router::kErrorOk);
    const RouterUser stored = findUser("bob");

    RouterUser request;
    request.entry_id = stored.entry_id;
    request.name = "renamed";
    request.flags = 0; // Disabled.

    bool password_changed = true;
    bool sessions_changed = true;
    ASSERT_EQ(users_.modify(request, &password_changed, &sessions_changed), proto::router::kErrorOk);
    EXPECT_FALSE(password_changed);
    EXPECT_FALSE(sessions_changed);

    const RouterUser after = findUser(stored.entry_id);
    EXPECT_EQ(after.flags, 0u);
    EXPECT_EQ(after.sessions, stored.sessions);
    EXPECT_EQ(after.name, "bob");
    EXPECT_EQ(after.verifier, stored.verifier);

    request.sessions = proto::router::SESSION_TYPE_ADMIN;
    ASSERT_EQ(users_.modify(request, &password_changed, &sessions_changed), proto::router::kErrorOk);
    EXPECT_TRUE(sessions_changed);
    EXPECT_EQ(findUser(stored.entry_id).sessions, kAllSessions);
}

//--------------------------------------------------------------------------------------------------
// A modify request with an empty address keeps the stored one, with or without the credentials.
TEST_F(UserStoreTest, EmailIsStoredAndKeptWhenNotSent)
{
    ASSERT_EQ(users_.add(makeUser("bob", proto::router::SESSION_TYPE_OPERATOR)), proto::router::kErrorOk);
    const RouterUser stored = findUser("bob");
    EXPECT_TRUE(stored.email.isEmpty());

    RouterUser request;
    request.entry_id = stored.entry_id;
    request.flags = User::ENABLED;
    request.email = "bob@example.com";
    ASSERT_EQ(users_.modify(request), proto::router::kErrorOk);
    EXPECT_EQ(findUser(stored.entry_id).email, "bob@example.com");

    request.email.clear();
    request.flags = 0;
    ASSERT_EQ(users_.modify(request), proto::router::kErrorOk);
    EXPECT_EQ(findUser(stored.entry_id).email, "bob@example.com");

    RouterUser rotated = makeUser("bob", proto::router::SESSION_TYPE_OPERATOR);
    rotated.entry_id = stored.entry_id;
    ASSERT_EQ(users_.modify(rotated), proto::router::kErrorOk);
    EXPECT_EQ(findUser(stored.entry_id).email, "bob@example.com");

    RouterUser with_email = makeUser("alice", proto::router::SESSION_TYPE_OPERATOR);
    with_email.email = "alice@example.com";
    ASSERT_EQ(users_.add(with_email), proto::router::kErrorOk);
    EXPECT_EQ(findUser("alice").email, "alice@example.com");
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserStoreTest, BuiltInUserCannotBeDisabledDemotedOrRemoved)
{
    RouterUser request;
    request.entry_id = admin_.entry_id;
    request.flags = 0;
    EXPECT_EQ(users_.modify(request), proto::router::kErrorAccessDenied);

    request.flags = User::ENABLED;
    request.sessions = proto::router::SESSION_TYPE_MANAGER;
    EXPECT_EQ(users_.modify(request), proto::router::kErrorAccessDenied);
    EXPECT_EQ(findUser(admin_.entry_id).sessions, kAllSessions);

    EXPECT_EQ(users_.remove(admin_.entry_id), proto::router::kErrorAccessDenied);
}

//--------------------------------------------------------------------------------------------------
// The credentials path keeps the stored level for a request without one, the same way the
// flags-only path does: the dialog sends zero for an untouched level with either.
TEST_F(UserStoreTest, RotationWithoutALevelKeepsTheStoredOne)
{
    ASSERT_EQ(users_.add(makeUser("bob", proto::router::SESSION_TYPE_MANAGER)), proto::router::kErrorOk);
    const RouterUser stored = findUser("bob");

    RouterUser rotated = RouterUser::create("bob", SecureString("Rotated1234!"));
    rotated.entry_id = stored.entry_id;
    rotated.flags = User::ENABLED;

    bool password_changed = false;
    bool sessions_changed = true;
    ASSERT_EQ(users_.modify(rotated, &password_changed, &sessions_changed), proto::router::kErrorOk);
    EXPECT_TRUE(password_changed);
    EXPECT_FALSE(sessions_changed);

    const RouterUser after = findUser(stored.entry_id);
    EXPECT_EQ(after.sessions, stored.sessions);
    EXPECT_EQ(after.verifier, rotated.verifier);
}

//--------------------------------------------------------------------------------------------------
// A password rotation revokes every device token of the user: the tokens were issued to the
// credentials that are gone.
TEST_F(UserStoreTest, PasswordRotationRevokesDeviceTokens)
{
    RouterUser rotated = makeUser("admin", kAllSessions);
    rotated.entry_id = admin_.entry_id;

    DeviceTokenStore store(db_);

    std::string token;
    ASSERT_TRUE(store.issue(admin_.entry_id, "127.0.0.1", &token));

    bool password_changed = false;
    ASSERT_EQ(users_.modify(rotated, &password_changed), proto::router::kErrorOk);
    EXPECT_TRUE(password_changed);
    EXPECT_EQ(findUser(admin_.entry_id).verifier, rotated.verifier);

    qint64 user_id = 0;
    EXPECT_EQ(store.find(token, &user_id), proto::router::kErrorNotFound);
}

//--------------------------------------------------------------------------------------------------
// An OTP reset drops the device tokens of that user in the same transaction: a token that outlived
// the secret it was issued against would still open a session without a code. The tokens of other
// users are untouched.
TEST_F(UserStoreTest, OtpResetRevokesDeviceTokensOfTheUser)
{
    RouterUser other = makeUser("operator", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_EQ(users_.add(other), proto::router::kErrorOk);
    ASSERT_EQ(users_.find("operator", &other), proto::router::kErrorOk);

    const QByteArray secret(20, 'x');
    ASSERT_TRUE(users_.setOtp(admin_.entry_id, secret, 42));

    DeviceTokenStore store(db_);

    std::string token;
    ASSERT_TRUE(store.issue(admin_.entry_id, "127.0.0.1", &token));

    std::string other_token;
    ASSERT_TRUE(store.issue(other.entry_id, "127.0.0.1", &other_token));

    EXPECT_EQ(users_.resetOtp(admin_.entry_id), proto::router::kErrorOk);

    const RouterUser stored = findUser(admin_.entry_id);
    EXPECT_TRUE(stored.otp_secret.isEmpty());
    EXPECT_EQ(stored.otp_counter, 0u);

    qint64 token_owner = 0;
    EXPECT_EQ(store.find(token, &token_owner), proto::router::kErrorNotFound);

    EXPECT_EQ(store.find(other_token, &token_owner), proto::router::kErrorOk);
    EXPECT_EQ(token_owner, other.entry_id);
}

//--------------------------------------------------------------------------------------------------
// The reset is answered by the state it leaves behind, so a user that has nothing to reset is not
// an error. A user that is not there is.
TEST_F(UserStoreTest, OtpResetOfUserWithoutEnrollment)
{
    EXPECT_EQ(users_.resetOtp(admin_.entry_id), proto::router::kErrorOk);
    EXPECT_EQ(users_.resetOtp(admin_.entry_id + 1000), proto::router::kErrorNotFound);
}

//--------------------------------------------------------------------------------------------------
// A step is consumed once: only a counter newer than the stored one advances it, so parallel
// sessions cannot accept the same code. A row that is not there answers the same way a spent
// step does - there is nothing to advance.
TEST_F(UserStoreTest, OtpCounterIsConsumedOnce)
{
    ASSERT_TRUE(users_.setOtp(admin_.entry_id, QByteArray(20, 'x'), 42));

    EXPECT_EQ(users_.consumeOtpCounter(admin_.entry_id, 43), proto::router::kErrorOk);
    EXPECT_EQ(findUser(admin_.entry_id).otp_counter, 43u);

    EXPECT_EQ(users_.consumeOtpCounter(admin_.entry_id, 43), proto::router::kErrorNotFound);
    EXPECT_EQ(users_.consumeOtpCounter(admin_.entry_id, 42), proto::router::kErrorNotFound);
    EXPECT_EQ(findUser(admin_.entry_id).otp_counter, 43u);

    EXPECT_EQ(users_.consumeOtpCounter(admin_.entry_id + 1000, 100), proto::router::kErrorNotFound);
}

//--------------------------------------------------------------------------------------------------
TEST_F(UserStoreTest, RemoveUserCascadesAccessEntries)
{
    RouterUser client = makeUser("client", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_EQ(users_.add(client), proto::router::kErrorOk);
    client = findUser("client");

    const qint64 workspace_id = addWorkspace(
        "alpha", {admin_.entry_id, client.entry_id});
    ASSERT_GT(workspace_id, 0);

    ASSERT_EQ(users_.remove(client.entry_id), proto::router::kErrorOk);

    proto::router::WorkspaceList list;
    WorkspaceStore(db_).listForAdmin(workspace_id, &list);
    ASSERT_EQ(list.workspace_size(), 1);
    EXPECT_EQ(list.workspace(0).user_id_size(), 1);
}

//--------------------------------------------------------------------------------------------------
// The membership of a workspace changes on the user paths too - removing a user moves the
// revision, so a workspace save built from the old member list conflicts instead of silently
// granting the access back.
TEST_F(UserStoreTest, RemovedUserBumpsWorkspaceRevision)
{
    RouterUser client = makeUser("client", proto::router::SESSION_TYPE_OPERATOR);
    ASSERT_EQ(users_.add(client), proto::router::kErrorOk);
    client = findUser("client");

    const qint64 workspace_id = addWorkspace(
        "alpha", {admin_.entry_id, client.entry_id});
    ASSERT_GT(workspace_id, 0);
    ASSERT_EQ(workspaceRevision(workspace_id), 1);

    ASSERT_EQ(users_.remove(client.entry_id), proto::router::kErrorOk);
    EXPECT_EQ(workspaceRevision(workspace_id), 2);

    EXPECT_EQ(WorkspaceStore(db_).modify(workspace_id, 1, "alpha", std::string_view(),
                                         {admin_.entry_id}),
              proto::router::kErrorConflict);
}

//--------------------------------------------------------------------------------------------------
// A lost create/rename race over the user name answers kErrorAlreadyExists, not the internal
// error of the UNIQUE constraint.
TEST_F(UserStoreTest, DuplicateUserNameIsAlreadyExists)
{
    ASSERT_EQ(users_.add(makeUser("bob", proto::router::SESSION_TYPE_OPERATOR)), proto::router::kErrorOk);
    EXPECT_EQ(users_.add(makeUser("bob", proto::router::SESSION_TYPE_OPERATOR)), proto::router::kErrorAlreadyExists);

    ASSERT_EQ(users_.add(makeUser("alice", proto::router::SESSION_TYPE_OPERATOR)), proto::router::kErrorOk);

    RouterUser renamed = makeUser("bob", proto::router::SESSION_TYPE_OPERATOR);
    renamed.entry_id = findUser("alice").entry_id;
    EXPECT_EQ(users_.modify(renamed), proto::router::kErrorAlreadyExists);
    EXPECT_TRUE(findUser("alice").isValid());
}
