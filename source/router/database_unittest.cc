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

#include "router/database.h"
#include "router/workspace.h"

#include <gtest/gtest.h>

#include <QTemporaryDir>

#include <set>
#include <string>

#include "base/crypto/random.h"
#include "base/crypto/secure_string.h"
#include "base/peer/router_user.h"
#include "base/peer/user.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "proto/router.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"

namespace {

constexpr quint32 kAllSessions = proto::router::SESSION_TYPE_ADMIN |
    proto::router::SESSION_TYPE_MANAGER | proto::router::SESSION_TYPE_CLIENT;

//--------------------------------------------------------------------------------------------------
RouterUser makeUser(const QString& name, quint32 sessions)
{
    RouterUser user = RouterUser::create(name, SecureString("Password1234!"));
    user.sessions = sessions;
    user.flags = User::ENABLED;
    return user;
}

//--------------------------------------------------------------------------------------------------
std::string toStdString(const QByteArray& bytes)
{
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

} // namespace

// Every test opens an isolated database in a temporary directory and starts from the state
// --create-config leaves behind: the built-in administrator with id 1.
class RouterDatabaseTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());
        file_path_ = temp_dir_.path() + "/router.db3";
        ASSERT_TRUE(db_.open(file_path_));

        admin_ = makeUser("admin", kAllSessions);
        ASSERT_TRUE(admin_.isValid());
        ASSERT_EQ(db_.addUser(admin_), proto::router::kErrorOk);

        ASSERT_EQ(db_.findUser("admin", &admin_), proto::router::kErrorOk);
        ASSERT_EQ(admin_.entry_id, 1);
    }

    RouterUser findUser(const QString& name)
    {
        RouterUser user;
        db_.findUser(name, &user);
        return user;
    }

    RouterUser findUser(qint64 entry_id)
    {
        RouterUser user;
        db_.findUser(entry_id, &user);
        return user;
    }

    qint64 addWorkspace(const QString& name, const std::vector<qint64>& access)
    {
        qint64 entry_id = -1;
        const std::string_view error_code =
            db_.addWorkspace(name.toStdString(), std::string_view(), access, &entry_id);
        if (error_code != proto::router::kErrorOk)
            return -1;
        return entry_id;
    }

    HostId addHost(std::string_view key_hash)
    {
        if (!db_.addHost(key_hash, "hwid"))
            return kInvalidHostId;

        HostId host_id = kInvalidHostId;
        if (db_.hostId(key_hash, &host_id) != proto::router::kErrorOk)
            return kInvalidHostId;
        return host_id;
    }

    // Moves a host to the given workspace and group, keeping the fields an operator edits.
    std::string_view moveHost(HostId host_id, qint64 workspace_id, qint64 group_id = 0)
    {
        const proto::router::Host host = findHost(host_id);
        return db_.modifyHost(host_id, workspace_id, group_id, host.display_name(),
                              host.comment());
    }

    qint64 workspaceRevision(qint64 workspace_id)
    {
        proto::router::WorkspaceList list;
        db_.workspaceListForAdmin(workspace_id, &list);
        if (list.error_code() != proto::router::kErrorOk || list.workspace_size() != 1)
            return -1;
        return list.workspace(0).revision();
    }

    proto::router::Host findHost(HostId host_id)
    {
        proto::router::HostList list;
        db_.hosts(0, proto::router::kMaxHostPageSize, &list);
        for (int i = 0; i < list.host_size(); ++i)
        {
            if (list.host(i).host_id() == host_id)
                return list.host(i);
        }
        return proto::router::Host();
    }

    // Doctors rows the public API deliberately cannot produce (legacy states, induced holes).
    bool execRaw(const QString& sql)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;
        return raw.exec(sql.toStdString().c_str());
    }

    QTemporaryDir temp_dir_;
    QString file_path_;
    Database db_;
    RouterUser admin_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(RouterDatabaseTest, AddUserStoresRecord)
{
    RouterUser user = makeUser("bob", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_EQ(db_.addUser(user), proto::router::kErrorOk);

    const RouterUser stored = findUser("bob");
    EXPECT_GT(stored.entry_id, 1);
    EXPECT_EQ(stored.sessions, proto::router::SESSION_TYPE_CLIENT);
    EXPECT_EQ(stored.flags, quint32(User::ENABLED));
    EXPECT_EQ(stored.verifier, user.verifier);
    EXPECT_EQ(stored.public_key, user.public_key);
}

//--------------------------------------------------------------------------------------------------
// The list is read one page at a time, and the same page holds the same records whatever the
// query planner does with it.
TEST_F(RouterDatabaseTest, UserListReturnsRequestedPage)
{
    for (int i = 0; i < 5; ++i)
    {
        ASSERT_EQ(db_.addUser(makeUser(QString("user%1").arg(i),
                                       proto::router::SESSION_TYPE_CLIENT)),
                  proto::router::kErrorOk);
    }

    qint64 count = 0;
    ASSERT_EQ(db_.userCount(&count), proto::router::kErrorOk);
    EXPECT_EQ(count, 6);

    std::vector<RouterUser> users;
    ASSERT_EQ(db_.userList(0, 2, &users), proto::router::kErrorOk);
    ASSERT_EQ(users.size(), 2u);
    EXPECT_EQ(users[0].name, "admin");
    EXPECT_EQ(users[1].name, "user0");

    ASSERT_EQ(db_.userList(4, 10, &users), proto::router::kErrorOk);
    ASSERT_EQ(users.size(), 2u);
    EXPECT_EQ(users[0].name, "user3");
    EXPECT_EQ(users[1].name, "user4");

    ASSERT_EQ(db_.userList(100, 10, &users), proto::router::kErrorOk);
    EXPECT_TRUE(users.empty());
}

//--------------------------------------------------------------------------------------------------
// A request that names no page, or one bigger than the cap, is refused instead of answering the
// whole table.
TEST_F(RouterDatabaseTest, UserListRefusesUnboundedPage)
{
    std::vector<RouterUser> users;
    EXPECT_EQ(db_.userList(0, 0, &users), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(db_.userList(-1, 10, &users), proto::router::kErrorInvalidRequest);
    EXPECT_EQ(db_.userList(0, proto::router::kMaxUserPageSize + 1, &users),
              proto::router::kErrorInvalidRequest);
}

//--------------------------------------------------------------------------------------------------
// A name that belongs to nobody is not a failure of the query.
TEST_F(RouterDatabaseTest, FindUserSeparatesMissAndFailure)
{
    RouterUser user;
    EXPECT_EQ(db_.findUser("nobody", &user), proto::router::kErrorNotFound);
    EXPECT_EQ(db_.findUser(qint64(4242), &user), proto::router::kErrorNotFound);
    EXPECT_EQ(db_.findUser("admin", &user), proto::router::kErrorOk);
    EXPECT_EQ(user.entry_id, admin_.entry_id);
}

//--------------------------------------------------------------------------------------------------
// The tokens of one user are bounded: a device that logs in anew every time cannot grow the list
// past the cap the protocol counts on. The least recently used ones go first.
TEST_F(RouterDatabaseTest, DeviceTokensAreCappedPerUser)
{
    std::string first_token;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &first_token));

    for (int i = 0; i < proto::router::kMaxDeviceTokensPerUser; ++i)
    {
        std::string token;
        ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &token));
    }

    std::vector<DeviceToken> tokens;
    ASSERT_TRUE(db_.listClientDeviceTokens(admin_.entry_id, &tokens));
    EXPECT_EQ(tokens.size(), size_t(proto::router::kMaxDeviceTokensPerUser));

    qint64 user_id = 0;
    EXPECT_FALSE(db_.findClientDeviceToken(first_token, &user_id));
}

//--------------------------------------------------------------------------------------------------
// A negative group_id asks for the hosts of the workspace whatever group they sit in.
TEST_F(RouterDatabaseTest, HostListTakesEveryGroupOfTheWorkspace)
{
    const qint64 workspace_id = addWorkspace("alpha", {admin_.entry_id});
    ASSERT_GT(workspace_id, 0);

    qint64 group_id = -1;
    ASSERT_EQ(db_.addGroup(workspace_id, 0, "group", std::string_view(), &group_id),
              proto::router::kErrorOk);

    const HostId root_host = addHost("hash-1");
    const HostId group_host = addHost("hash-2");
    ASSERT_NE(root_host, kInvalidHostId);
    ASSERT_NE(group_host, kInvalidHostId);

    ASSERT_EQ(moveHost(root_host, workspace_id), proto::router::kErrorOk);
    ASSERT_EQ(moveHost(group_host, workspace_id, group_id), proto::router::kErrorOk);

    qint64 count = 0;
    ASSERT_EQ(db_.hostCount(workspace_id, -1, &count), proto::router::kErrorOk);
    EXPECT_EQ(count, 2);

    proto::router::HostList list;
    db_.hosts(workspace_id, -1, 0, proto::router::kMaxHostPageSize, &list);
    ASSERT_EQ(list.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(list.host_size(), 2);

    ASSERT_EQ(db_.hostCount(workspace_id, 0, &count), proto::router::kErrorOk);
    EXPECT_EQ(count, 1);
}

//--------------------------------------------------------------------------------------------------
// I1: the session mask is written once at creation; modifyUser must ignore the mask of the
// request even when the credentials are fully replaced.
TEST_F(RouterDatabaseTest, ModifyUserIgnoresSessionMask)
{
    RouterUser user = makeUser("bob", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_EQ(db_.addUser(user), proto::router::kErrorOk);
    const qint64 user_id = findUser("bob").entry_id;

    RouterUser modified = makeUser("bob", kAllSessions);
    modified.entry_id = user_id;
    ASSERT_EQ(db_.modifyUser(modified), proto::router::kErrorOk);

    EXPECT_EQ(findUser(user_id).sessions, proto::router::SESSION_TYPE_CLIENT);
}

//--------------------------------------------------------------------------------------------------
// A request with empty credentials changes only the flags: the stored name and credentials
// survive, so a stale snapshot cannot roll back a concurrent change.
TEST_F(RouterDatabaseTest, FlagsOnlyModifyChangesOnlyFlags)
{
    RouterUser user = makeUser("bob", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_EQ(db_.addUser(user), proto::router::kErrorOk);
    const RouterUser stored = findUser("bob");

    RouterUser request;
    request.entry_id = stored.entry_id;
    request.name = "renamed";
    request.flags = 0; // Disabled.
    request.public_key = stored.public_key;

    bool password_changed = true;
    ASSERT_EQ(db_.modifyUser(request, &password_changed),
              proto::router::kErrorOk);
    EXPECT_FALSE(password_changed);

    const RouterUser after = findUser(stored.entry_id);
    EXPECT_EQ(after.flags, 0u);
    EXPECT_EQ(after.name, "bob");
    EXPECT_EQ(after.verifier, stored.verifier);
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterDatabaseTest, BuiltInUserCannotBeDisabledOrRemoved)
{
    RouterUser request;
    request.entry_id = admin_.entry_id;
    request.flags = 0;
    request.public_key = admin_.public_key;

    EXPECT_EQ(db_.modifyUser(request), proto::router::kErrorAccessDenied);
    EXPECT_EQ(db_.removeUser(admin_.entry_id), proto::router::kErrorAccessDenied);
}

//--------------------------------------------------------------------------------------------------
// A password rotation revokes every device token of the user: the tokens were issued to the
// credentials that are gone.
TEST_F(RouterDatabaseTest, PasswordRotationRevokesDeviceTokens)
{
    RouterUser rotated = makeUser("admin", kAllSessions);
    rotated.entry_id = admin_.entry_id;

    std::string token;
    ASSERT_TRUE(db_.issueClientDeviceToken(admin_.entry_id, "127.0.0.1", &token));

    bool password_changed = false;
    ASSERT_EQ(db_.modifyUser(rotated, &password_changed), proto::router::kErrorOk);
    EXPECT_TRUE(password_changed);
    EXPECT_EQ(findUser(admin_.entry_id).verifier, rotated.verifier);

    qint64 user_id = 0;
    EXPECT_FALSE(db_.findClientDeviceToken(token, &user_id));
}

//--------------------------------------------------------------------------------------------------
// A new public key with the old salt and verifier is a rotation too - the identity the client
// signs with is a different one from now on.
TEST_F(RouterDatabaseTest, PublicKeyChangeAloneIsRotation)
{
    RouterUser rotated = admin_;
    rotated.public_key = Random::byteArray(32);

    bool password_changed = false;
    ASSERT_EQ(db_.modifyUser(rotated, &password_changed), proto::router::kErrorOk);
    EXPECT_TRUE(password_changed);
    EXPECT_EQ(findUser(admin_.entry_id).public_key, rotated.public_key);
}

//--------------------------------------------------------------------------------------------------
// An administrator sees every workspace by its session type, so a workspace can be created
// without one - and without any member at all.
TEST_F(RouterDatabaseTest, WorkspaceNeedsNoAdminInItsAccessList)
{
    qint64 entry_id = -1;
    ASSERT_EQ(db_.addWorkspace("alpha", std::string_view(), {}, &entry_id),
              proto::router::kErrorOk);
    ASSERT_GT(entry_id, 0);

    proto::router::WorkspaceList list;
    db_.workspaceListForAdmin(0, &list);
    ASSERT_EQ(list.workspace_size(), 1);
    EXPECT_EQ(list.workspace(0).user_id_size(), 0);
}

//--------------------------------------------------------------------------------------------------
// A duplicate name is answered before anything is created.
TEST_F(RouterDatabaseTest, DuplicateWorkspaceNameIsAlreadyExists)
{
    ASSERT_GT(addWorkspace("alpha", {admin_.entry_id}), 0);

    qint64 entry_id = -1;
    EXPECT_EQ(db_.addWorkspace("alpha", std::string_view(), {}, &entry_id),
              proto::router::kErrorAlreadyExists);
}

//--------------------------------------------------------------------------------------------------
// I4: a modification based on a stale revision is rejected; a successful one increments the
// stored revision.
TEST_F(RouterDatabaseTest, RevisionGuardsConcurrentModification)
{
    const qint64 workspace_id = addWorkspace("alpha", {admin_.entry_id});
    ASSERT_GT(workspace_id, 0);
    ASSERT_EQ(workspaceRevision(workspace_id), 1);

    EXPECT_EQ(db_.modifyWorkspace(workspace_id, 1, "beta", std::string_view(),
                                  {admin_.entry_id}),
              proto::router::kErrorOk);
    EXPECT_EQ(workspaceRevision(workspace_id), 2);

    // A save built on the old snapshot must not overwrite the change above.
    EXPECT_EQ(db_.modifyWorkspace(workspace_id, 1, "gamma", std::string_view(),
                                  {admin_.entry_id}),
              proto::router::kErrorConflict);
    EXPECT_EQ(db_.findWorkspace(workspace_id).name, "beta");
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterDatabaseTest, ModifyWorkspaceGrantsAndRevokes)
{
    RouterUser client = makeUser("client", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_EQ(db_.addUser(client), proto::router::kErrorOk);
    client = findUser("client");

    const qint64 workspace_id = addWorkspace("alpha", {admin_.entry_id});
    ASSERT_GT(workspace_id, 0);

    ASSERT_EQ(db_.modifyWorkspace(workspace_id, 1, "alpha", std::string_view(),
                                  {admin_.entry_id, client.entry_id}),
              proto::router::kErrorOk);

    std::set<qint64> workspace_ids;
    ASSERT_TRUE(db_.workspaceAccessIdsForUser(client.entry_id, &workspace_ids));
    EXPECT_TRUE(workspace_ids.contains(workspace_id));

    ASSERT_EQ(db_.modifyWorkspace(workspace_id, 2, "alpha", std::string_view(),
                                  {admin_.entry_id}),
              proto::router::kErrorOk);

    ASSERT_TRUE(db_.workspaceAccessIdsForUser(client.entry_id, &workspace_ids));
    EXPECT_TRUE(workspace_ids.empty());
}

//--------------------------------------------------------------------------------------------------
// A host belongs to one workspace at a time. The second claim loses, and the loser is told to
// refetch instead of being told the host is gone.
TEST_F(RouterDatabaseTest, HostBelongsToOneWorkspace)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const qint64 workspace_id = addWorkspace("alpha", {admin_.entry_id});
    ASSERT_GT(workspace_id, 0);
    const qint64 other_id = addWorkspace("beta", {admin_.entry_id});
    ASSERT_GT(other_id, 0);

    ASSERT_EQ(moveHost(host_id, workspace_id), proto::router::kErrorOk);
    EXPECT_EQ(findHost(host_id).workspace_id(), workspace_id);

    EXPECT_EQ(moveHost(host_id, other_id), proto::router::kErrorConflict);
    EXPECT_EQ(findHost(host_id).workspace_id(), workspace_id);

    // A workspace that is gone reads the same way: the sender acted on a stale snapshot.
    EXPECT_EQ(db_.modifyHost(host_id, 12345, 0, "display", std::string_view()),
              proto::router::kErrorConflict);
    EXPECT_EQ(db_.modifyHost(HostId(54321), workspace_id, 0, "display", std::string_view()),
              proto::router::kErrorNotFound);
}

//--------------------------------------------------------------------------------------------------
// A released host cannot keep the place and the note it had inside the workspace it left.
TEST_F(RouterDatabaseTest, ReleasedHostLosesItsGroupAndComment)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const qint64 workspace_id = addWorkspace("alpha", {admin_.entry_id});
    ASSERT_GT(workspace_id, 0);

    qint64 group_id = -1;
    ASSERT_EQ(db_.addGroup(workspace_id, 0, "group", std::string_view(), &group_id),
              proto::router::kErrorOk);

    ASSERT_EQ(db_.modifyHost(host_id, workspace_id, group_id, "display", "comment"),
              proto::router::kErrorOk);
    ASSERT_FALSE(findHost(host_id).comment().empty());

    ASSERT_EQ(moveHost(host_id, 0), proto::router::kErrorOk);

    const proto::router::Host host = findHost(host_id);
    EXPECT_EQ(host.workspace_id(), 0);
    EXPECT_EQ(host.group_id(), 0);
    EXPECT_TRUE(host.comment().empty());
    EXPECT_EQ(host.display_name(), "display");
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterDatabaseTest, RemoveWorkspaceReleasesHostsAndAccess)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const qint64 workspace_id = addWorkspace("alpha", {admin_.entry_id});
    ASSERT_GT(workspace_id, 0);
    ASSERT_EQ(moveHost(host_id, workspace_id), proto::router::kErrorOk);

    ASSERT_EQ(db_.removeWorkspace(workspace_id), proto::router::kErrorOk);

    std::set<qint64> workspace_ids;
    ASSERT_TRUE(db_.workspaceAccessIdsForUser(admin_.entry_id, &workspace_ids));
    EXPECT_TRUE(workspace_ids.empty());
    EXPECT_EQ(findHost(host_id).workspace_id(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterDatabaseTest, RemoveUserCascadesAccessEntries)
{
    RouterUser client = makeUser("client", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_EQ(db_.addUser(client), proto::router::kErrorOk);
    client = findUser("client");

    const qint64 workspace_id = addWorkspace(
        "alpha", {admin_.entry_id, client.entry_id});
    ASSERT_GT(workspace_id, 0);

    ASSERT_EQ(db_.removeUser(client.entry_id), proto::router::kErrorOk);

    proto::router::WorkspaceList list;
    db_.workspaceListForAdmin(workspace_id, &list);
    ASSERT_EQ(list.workspace_size(), 1);
    EXPECT_EQ(list.workspace(0).user_id_size(), 1);
}

//--------------------------------------------------------------------------------------------------
// I3: the membership of a workspace changes on the user paths too - removing a user moves the
// revision, so a workspace save built from the old member list conflicts instead of silently
// granting the access back.
TEST_F(RouterDatabaseTest, RemovedUserBumpsWorkspaceRevision)
{
    RouterUser client = makeUser("client", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_EQ(db_.addUser(client), proto::router::kErrorOk);
    client = findUser("client");

    const qint64 workspace_id = addWorkspace(
        "alpha", {admin_.entry_id, client.entry_id});
    ASSERT_GT(workspace_id, 0);
    ASSERT_EQ(workspaceRevision(workspace_id), 1);

    ASSERT_EQ(db_.removeUser(client.entry_id), proto::router::kErrorOk);
    EXPECT_EQ(workspaceRevision(workspace_id), 2);

    EXPECT_EQ(db_.modifyWorkspace(workspace_id, 1, "alpha", std::string_view(),
                                  {admin_.entry_id}),
              proto::router::kErrorConflict);
}

//--------------------------------------------------------------------------------------------------
// An access entry for a user that is gone is how a concurrent delete looks: the sender still
// saw the user in its snapshot, so the answer is a conflict resolved by refetching.
TEST_F(RouterDatabaseTest, EntryForDeletedUserIsConflict)
{
    RouterUser client = makeUser("client", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_EQ(db_.addUser(client), proto::router::kErrorOk);
    client = findUser("client");
    ASSERT_EQ(db_.removeUser(client.entry_id), proto::router::kErrorOk);

    qint64 entry_id = -1;
    EXPECT_EQ(db_.addWorkspace("alpha", std::string_view(),
                               {admin_.entry_id, client.entry_id}, &entry_id),
              proto::router::kErrorConflict);

    // The transaction rolled back: the name is still free.
    EXPECT_GT(addWorkspace("alpha", {admin_.entry_id}), 0);
}

//--------------------------------------------------------------------------------------------------
// A lost create/rename race over the user name answers kErrorAlreadyExists, not the internal
// error of the UNIQUE constraint.
TEST_F(RouterDatabaseTest, DuplicateUserNameIsAlreadyExists)
{
    ASSERT_EQ(db_.addUser(makeUser("bob", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);
    EXPECT_EQ(db_.addUser(makeUser("bob", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorAlreadyExists);

    ASSERT_EQ(db_.addUser(makeUser("alice", proto::router::SESSION_TYPE_CLIENT)),
              proto::router::kErrorOk);

    RouterUser renamed = makeUser("bob", proto::router::SESSION_TYPE_CLIENT);
    renamed.entry_id = findUser("alice").entry_id;
    EXPECT_EQ(db_.modifyUser(renamed), proto::router::kErrorAlreadyExists);
    EXPECT_TRUE(findUser("alice").isValid());
}

//--------------------------------------------------------------------------------------------------
// The very first AUTOINCREMENT insert into the database happens before sqlite_sequence exists;
// approving a host on such a database must succeed with the first permanent id.
TEST(RouterDatabaseFreshTest, AddHostBeforeAnyAutoincrementInsert)
{
    QTemporaryDir temp_dir;
    ASSERT_TRUE(temp_dir.isValid());

    Database db;
    ASSERT_TRUE(db.open(temp_dir.path() + "/router.db3"));

    ASSERT_TRUE(db.addHost("hash-1", "hwid-1"));

    HostId host_id = kInvalidHostId;
    ASSERT_EQ(db.hostId("hash-1", &host_id), proto::router::kErrorOk);
    EXPECT_EQ(host_id, HostId(1));
}

//--------------------------------------------------------------------------------------------------
// A database created before the revision column existed gets it backfilled on open, with the
// default every pre-existing row starts from.
TEST(RouterDatabaseMigrationTest, RevisionColumnBackfilled)
{
    QTemporaryDir temp_dir;
    ASSERT_TRUE(temp_dir.isValid());
    const QString file_path = temp_dir.path() + "/router.db3";

    {
        SqlDatabase legacy;
        ASSERT_TRUE(legacy.open(file_path));
        ASSERT_TRUE(legacy.exec("CREATE TABLE \"workspaces\" ("
                                "\"id\" INTEGER UNIQUE,"
                                "\"name\" TEXT NOT NULL UNIQUE,"
                                "\"comment\" BLOB NOT NULL DEFAULT X'',"
                                "PRIMARY KEY(\"id\" AUTOINCREMENT))"));
        ASSERT_TRUE(legacy.exec("INSERT INTO workspaces (id, name) VALUES (NULL, 'legacy')"));
    }

    Database db;
    ASSERT_TRUE(db.open(file_path));

    SqlDatabase raw;
    ASSERT_TRUE(raw.open(file_path));
    SqlQuery query(raw, "SELECT revision FROM workspaces WHERE name='legacy'");
    ASSERT_EQ(query.next(), SqlQuery::StepResult::ROW);
    EXPECT_EQ(query.columnInt64(0), 1);
}
