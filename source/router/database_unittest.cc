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
#include <unordered_map>

#include "base/crypto/random.h"
#include "base/crypto/sealed_box.h"
#include "base/crypto/secure_byte_array.h"
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

        admin_ = db_.findUser("admin");
        ASSERT_EQ(admin_.entry_id, 1);
    }

    // One access entry for |user|, sealed to its current key and carrying the seal target the
    // router verifies.
    Workspace::Access accessEntry(const RouterUser& user, const SecureByteArray& gk)
    {
        Workspace::Access access;
        access.user_id = user.entry_id;
        access.wrapped_gk = toStdString(SealedBox::seal(gk, user.public_key));
        access.public_key = toStdString(user.public_key);
        return access;
    }

    // An entry for a user already present in the workspace: the router preserves the stored
    // wrapped_gk, so no key material travels with it.
    Workspace::Access existingEntry(qint64 user_id)
    {
        Workspace::Access access;
        access.user_id = user_id;
        return access;
    }

    qint64 addWorkspace(const QString& name, const std::vector<Workspace::Access>& access,
                        const std::set<HostId>& hosts = {})
    {
        qint64 entry_id = -1;
        const std::string_view error_code =
            db_.addWorkspace(name.toStdString(), std::string_view(), access, hosts, &entry_id);
        if (error_code != proto::router::kErrorOk)
            return -1;
        return entry_id;
    }

    qint64 workspaceRevision(qint64 workspace_id)
    {
        proto::router::WorkspaceList list;
        db_.workspaceListWithAllAccess(admin_.entry_id, workspace_id, &list);
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

    const RouterUser stored = db_.findUser("bob");
    EXPECT_GT(stored.entry_id, 1);
    EXPECT_EQ(stored.sessions, proto::router::SESSION_TYPE_CLIENT);
    EXPECT_EQ(stored.flags, quint32(User::ENABLED));
    EXPECT_EQ(stored.verifier, user.verifier);
    EXPECT_EQ(stored.public_key, user.public_key);
}

//--------------------------------------------------------------------------------------------------
// I1: the session mask is written once at creation; modifyUser must ignore the mask of the
// request even when the credentials are fully replaced.
TEST_F(RouterDatabaseTest, ModifyUserIgnoresSessionMask)
{
    RouterUser user = makeUser("bob", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_EQ(db_.addUser(user), proto::router::kErrorOk);
    const qint64 user_id = db_.findUser("bob").entry_id;

    RouterUser modified = makeUser("bob", kAllSessions);
    modified.entry_id = user_id;
    ASSERT_EQ(db_.modifyUser(modified, {}, admin_.entry_id), proto::router::kErrorOk);

    EXPECT_EQ(db_.findUser(user_id).sessions, proto::router::SESSION_TYPE_CLIENT);
}

//--------------------------------------------------------------------------------------------------
// A request with empty credentials changes only the flags: the stored name and credentials
// survive, so a stale snapshot cannot roll back a concurrent change.
TEST_F(RouterDatabaseTest, FlagsOnlyModifyChangesOnlyFlags)
{
    RouterUser user = makeUser("bob", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_EQ(db_.addUser(user), proto::router::kErrorOk);
    const RouterUser stored = db_.findUser("bob");

    RouterUser request;
    request.entry_id = stored.entry_id;
    request.name = "renamed";
    request.flags = 0; // Disabled.
    request.public_key = stored.public_key;

    bool password_changed = true;
    ASSERT_EQ(db_.modifyUser(request, {}, admin_.entry_id, &password_changed),
              proto::router::kErrorOk);
    EXPECT_FALSE(password_changed);

    const RouterUser after = db_.findUser(stored.entry_id);
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

    EXPECT_EQ(db_.modifyUser(request, {}, admin_.entry_id), proto::router::kErrorAccessDenied);
    EXPECT_EQ(db_.removeUser(admin_.entry_id), proto::router::kErrorAccessDenied);
}

//--------------------------------------------------------------------------------------------------
// I3: a password rotation invalidates the stored wrapped GKs, so the change must arrive with a
// re-sealed key for every access entry - otherwise nothing is applied.
TEST_F(RouterDatabaseTest, PasswordRotationRequiresCompleteKeySet)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace("alpha", {accessEntry(admin_, gk)});
    ASSERT_GT(workspace_id, 0);

    RouterUser rotated = makeUser("admin", kAllSessions);
    rotated.entry_id = admin_.entry_id;

    // A missing key means the sender's workspace list was stale - a conflict, not bad data.
    EXPECT_EQ(db_.modifyUser(rotated, {}, admin_.entry_id), proto::router::kErrorConflict);
    EXPECT_EQ(db_.findUser(admin_.entry_id).verifier, admin_.verifier);

    std::unordered_map<qint64, QByteArray> wrapped_keys;
    wrapped_keys.emplace(workspace_id, SealedBox::seal(gk, rotated.public_key));

    bool password_changed = false;
    EXPECT_EQ(db_.modifyUser(rotated, wrapped_keys, admin_.entry_id, &password_changed),
              proto::router::kErrorOk);
    EXPECT_TRUE(password_changed);
    EXPECT_EQ(db_.findUser(admin_.entry_id).verifier, rotated.verifier);

    proto::router::WorkspaceList list;
    db_.workspaceListWithAllAccess(admin_.entry_id, workspace_id, &list);
    ASSERT_EQ(list.workspace_size(), 1);
    ASSERT_EQ(list.workspace(0).access_size(), 1);
    EXPECT_EQ(list.workspace(0).access(0).wrapped_gk(),
              toStdString(wrapped_keys[workspace_id]));
}

//--------------------------------------------------------------------------------------------------
// I2: an administrator created after the workspaces receives an access entry for each of them
// from the wrapped keys of the request.
TEST_F(RouterDatabaseTest, NewAdminIsGrantedEveryWorkspace)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace("alpha", {accessEntry(admin_, gk)});
    ASSERT_GT(workspace_id, 0);

    RouterUser admin2 = makeUser("admin2", kAllSessions);
    std::unordered_map<qint64, QByteArray> wrapped_keys;
    wrapped_keys.emplace(workspace_id, SealedBox::seal(gk, admin2.public_key));

    ASSERT_EQ(db_.addUser(admin2, wrapped_keys, admin_.entry_id), proto::router::kErrorOk);

    std::set<qint64> workspace_ids;
    ASSERT_TRUE(db_.workspaceAccessIdsForUser(db_.findUser("admin2").entry_id,
                                              &workspace_ids));
    EXPECT_TRUE(workspace_ids.contains(workspace_id));
}

//--------------------------------------------------------------------------------------------------
// The grantor has access to the workspace, so a request without its key was built from a stale
// list - the whole insert must be rejected and rolled back.
TEST_F(RouterDatabaseTest, NewAdminRejectedWhenKeyMissing)
{
    const SecureByteArray gk(Random::byteArray(32));
    ASSERT_GT(addWorkspace("alpha", {accessEntry(admin_, gk)}), 0);

    RouterUser admin2 = makeUser("admin2", kAllSessions);
    EXPECT_EQ(db_.addUser(admin2, {}, admin_.entry_id), proto::router::kErrorConflict);

    // The transaction rolled back: the user does not exist.
    EXPECT_FALSE(db_.findUser("admin2").isValid());
}

//--------------------------------------------------------------------------------------------------
// A workspace the grantor cannot see is skipped (nobody can seal its key), not rejected.
TEST_F(RouterDatabaseTest, NewAdminSkipsWorkspaceGrantorCannotSee)
{
    const SecureByteArray gk(Random::byteArray(32));
    ASSERT_GT(addWorkspace("alpha", {accessEntry(admin_, gk)}), 0);

    RouterUser client = makeUser("client", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_EQ(db_.addUser(client), proto::router::kErrorOk);
    const qint64 client_id = db_.findUser("client").entry_id;

    RouterUser admin2 = makeUser("admin2", kAllSessions);
    ASSERT_EQ(db_.addUser(admin2, {}, client_id), proto::router::kErrorOk);

    std::set<qint64> workspace_ids;
    ASSERT_TRUE(db_.workspaceAccessIdsForUser(db_.findUser("admin2").entry_id,
                                              &workspace_ids));
    EXPECT_TRUE(workspace_ids.empty());
}

//--------------------------------------------------------------------------------------------------
// I2 from the workspace side: the initial access list must contain every administrator that has
// a key pair.
TEST_F(RouterDatabaseTest, AddWorkspaceRequiresEveryKeyedAdmin)
{
    RouterUser admin2 = makeUser("admin2", kAllSessions);
    ASSERT_EQ(db_.addUser(admin2, {}, admin_.entry_id), proto::router::kErrorOk);
    admin2 = db_.findUser("admin2");

    const SecureByteArray gk(Random::byteArray(32));
    EXPECT_EQ(addWorkspace("alpha", {accessEntry(admin_, gk)}), -1);
    EXPECT_GT(addWorkspace("alpha",
                           {accessEntry(admin_, gk), accessEntry(admin2, gk)}), 0);
}

//--------------------------------------------------------------------------------------------------
// A duplicate name must answer kErrorAlreadyExists even when the access list is incomplete: the
// more specific error is not masked by the admin coverage check.
TEST_F(RouterDatabaseTest, DuplicateWorkspaceNameBeatsAdminCoverage)
{
    const SecureByteArray gk(Random::byteArray(32));
    ASSERT_GT(addWorkspace("alpha", {accessEntry(admin_, gk)}), 0);

    qint64 entry_id = -1;
    EXPECT_EQ(db_.addWorkspace("alpha", std::string_view(), {}, {}, &entry_id),
              proto::router::kErrorAlreadyExists);
}

//--------------------------------------------------------------------------------------------------
// I3: an access entry sealed to a key the user no longer has is rejected with kErrorConflict
// and nothing is created.
TEST_F(RouterDatabaseTest, StaleSealTargetRejected)
{
    const SecureByteArray gk(Random::byteArray(32));

    Workspace::Access stale = accessEntry(admin_, gk);
    stale.public_key = toStdString(Random::byteArray(32));

    qint64 entry_id = -1;
    EXPECT_EQ(db_.addWorkspace("alpha", std::string_view(), {stale}, {}, &entry_id),
              proto::router::kErrorConflict);

    // The transaction rolled back: the name is still free.
    EXPECT_GT(addWorkspace("alpha", {accessEntry(admin_, gk)}), 0);
}

//--------------------------------------------------------------------------------------------------
// A keyless administrator (pre-upgrade record) is not required in the access list, but an entry
// for it is rejected: nobody can seal the group key to a key pair that does not exist.
TEST_F(RouterDatabaseTest, KeylessAdminNotRequiredButNotInsertable)
{
    RouterUser admin2 = makeUser("admin2", kAllSessions);
    ASSERT_EQ(db_.addUser(admin2, {}, admin_.entry_id), proto::router::kErrorOk);
    admin2 = db_.findUser("admin2");

    ASSERT_TRUE(execRaw(QString("UPDATE users SET public_key=X'' WHERE id=%1")
                            .arg(admin2.entry_id)));

    const SecureByteArray gk(Random::byteArray(32));
    EXPECT_GT(addWorkspace("alpha", {accessEntry(admin_, gk)}), 0);

    qint64 entry_id = -1;
    EXPECT_EQ(db_.addWorkspace("beta", std::string_view(),
                               {accessEntry(admin_, gk), accessEntry(admin2, gk)}, {}, &entry_id),
              proto::router::kErrorInvalidData);
}

//--------------------------------------------------------------------------------------------------
// I4: a modification based on a stale revision is rejected; a successful one increments the
// stored revision.
TEST_F(RouterDatabaseTest, RevisionGuardsConcurrentModification)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace("alpha", {accessEntry(admin_, gk)});
    ASSERT_GT(workspace_id, 0);
    ASSERT_EQ(workspaceRevision(workspace_id), 1);

    EXPECT_EQ(db_.modifyWorkspace(workspace_id, 1, "beta", std::string_view(),
                                  {existingEntry(admin_.entry_id)}, {}),
              proto::router::kErrorOk);
    EXPECT_EQ(workspaceRevision(workspace_id), 2);

    // A save built on the old snapshot must not overwrite the change above.
    EXPECT_EQ(db_.modifyWorkspace(workspace_id, 1, "gamma", std::string_view(),
                                  {existingEntry(admin_.entry_id)}, {}),
              proto::router::kErrorConflict);
    EXPECT_EQ(db_.findWorkspace(workspace_id).name, "beta");
}

//--------------------------------------------------------------------------------------------------
// The wrapped_gk of an already granted user is preserved: whatever the request carries for it
// is ignored.
TEST_F(RouterDatabaseTest, ModifyWorkspacePreservesExistingWrappedGk)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace("alpha", {accessEntry(admin_, gk)});
    ASSERT_GT(workspace_id, 0);

    proto::router::WorkspaceList before;
    db_.workspaceListWithAllAccess(admin_.entry_id, workspace_id, &before);
    ASSERT_EQ(before.workspace(0).access_size(), 1);
    const std::string original_gk = before.workspace(0).access(0).wrapped_gk();

    const SecureByteArray other_gk(Random::byteArray(32));
    ASSERT_EQ(db_.modifyWorkspace(workspace_id, 1, "alpha", std::string_view(),
                                  {accessEntry(admin_, other_gk)}, {}),
              proto::router::kErrorOk);

    proto::router::WorkspaceList after;
    db_.workspaceListWithAllAccess(admin_.entry_id, workspace_id, &after);
    ASSERT_EQ(after.workspace(0).access_size(), 1);
    EXPECT_EQ(after.workspace(0).access(0).wrapped_gk(), original_gk);
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterDatabaseTest, ModifyWorkspaceGrantsAndRevokes)
{
    RouterUser client = makeUser("client", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_EQ(db_.addUser(client), proto::router::kErrorOk);
    client = db_.findUser("client");

    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace("alpha", {accessEntry(admin_, gk)});
    ASSERT_GT(workspace_id, 0);

    ASSERT_EQ(db_.modifyWorkspace(workspace_id, 1, "alpha", std::string_view(),
                                  {existingEntry(admin_.entry_id), accessEntry(client, gk)}, {}),
              proto::router::kErrorOk);

    std::set<qint64> workspace_ids;
    ASSERT_TRUE(db_.workspaceAccessIdsForUser(client.entry_id, &workspace_ids));
    EXPECT_TRUE(workspace_ids.contains(workspace_id));

    ASSERT_EQ(db_.modifyWorkspace(workspace_id, 2, "alpha", std::string_view(),
                                  {existingEntry(admin_.entry_id)}, {}),
              proto::router::kErrorOk);

    ASSERT_TRUE(db_.workspaceAccessIdsForUser(client.entry_id, &workspace_ids));
    EXPECT_TRUE(workspace_ids.empty());
}

//--------------------------------------------------------------------------------------------------
// I2: an administrator with a key pair cannot be dropped from the access list.
TEST_F(RouterDatabaseTest, ModifyWorkspaceCannotDropKeyedAdmin)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace("alpha", {accessEntry(admin_, gk)});
    ASSERT_GT(workspace_id, 0);

    // The usual cause of a missing administrator is a stale user list, so the answer is a
    // conflict: a refetch auto-includes every administrator and the retry passes.
    EXPECT_EQ(db_.modifyWorkspace(workspace_id, 1, "alpha", std::string_view(), {}, {}),
              proto::router::kErrorConflict);
}

//--------------------------------------------------------------------------------------------------
// The host assignments live in the same transaction as the workspace: a bad host id rolls the
// whole creation back, and a claim of another workspace's host rolls the whole modify back.
TEST_F(RouterDatabaseTest, HostAssignmentsAreAtomic)
{
    ASSERT_TRUE(db_.addHost("hash-1", "hwid-1"));
    HostId host_id = kInvalidHostId;
    ASSERT_EQ(db_.hostId("hash-1", &host_id), proto::router::kErrorOk);

    const SecureByteArray gk(Random::byteArray(32));

    // A host that is not there is how a concurrent delete looks - a conflict, not "not found"
    // (which the sender reads as a missing workspace).
    qint64 entry_id = -1;
    EXPECT_EQ(db_.addWorkspace("alpha", std::string_view(), {accessEntry(admin_, gk)},
                               {HostId(12345)}, &entry_id),
              proto::router::kErrorConflict);

    // The rollback left the name free; the valid host is claimed with the creation.
    const qint64 workspace_id =
        addWorkspace("alpha", {accessEntry(admin_, gk)}, {host_id});
    ASSERT_GT(workspace_id, 0);
    EXPECT_EQ(findHost(host_id).workspace_id(), workspace_id);

    // A second workspace cannot steal the host, and the failed save must not apply anything -
    // including the rename that travelled with it.
    const qint64 other_id = addWorkspace("beta", {accessEntry(admin_, gk)});
    ASSERT_GT(other_id, 0);

    EXPECT_EQ(db_.modifyWorkspace(other_id, 1, "renamed", std::string_view(),
                                  {existingEntry(admin_.entry_id)}, {host_id}),
              proto::router::kErrorConflict);
    EXPECT_EQ(db_.findWorkspace(other_id).name, "beta");
    EXPECT_EQ(findHost(host_id).workspace_id(), workspace_id);
}

//--------------------------------------------------------------------------------------------------
// A released host cannot keep fields encrypted with the group key of the workspace it left.
TEST_F(RouterDatabaseTest, ReleasedHostLosesEncryptedFields)
{
    ASSERT_TRUE(db_.addHost("hash-1", "hwid-1"));
    HostId host_id = kInvalidHostId;
    ASSERT_EQ(db_.hostId("hash-1", &host_id), proto::router::kErrorOk);

    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id =
        addWorkspace("alpha", {accessEntry(admin_, gk)}, {host_id});
    ASSERT_GT(workspace_id, 0);

    ASSERT_TRUE(db_.modifyHost(host_id, 0, "display", "comment", "user", "password"));
    ASSERT_FALSE(findHost(host_id).comment().empty());

    ASSERT_EQ(db_.modifyWorkspace(workspace_id, 1, "alpha", std::string_view(),
                                  {existingEntry(admin_.entry_id)}, {}),
              proto::router::kErrorOk);

    const proto::router::Host host = findHost(host_id);
    EXPECT_EQ(host.workspace_id(), 0);
    EXPECT_TRUE(host.comment().empty());
    EXPECT_TRUE(host.user_name().empty());
    EXPECT_TRUE(host.password().empty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterDatabaseTest, RemoveWorkspaceReleasesHostsAndAccess)
{
    ASSERT_TRUE(db_.addHost("hash-1", "hwid-1"));
    HostId host_id = kInvalidHostId;
    ASSERT_EQ(db_.hostId("hash-1", &host_id), proto::router::kErrorOk);

    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id =
        addWorkspace("alpha", {accessEntry(admin_, gk)}, {host_id});
    ASSERT_GT(workspace_id, 0);

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
    client = db_.findUser("client");

    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace(
        "alpha", {accessEntry(admin_, gk), accessEntry(client, gk)});
    ASSERT_GT(workspace_id, 0);

    ASSERT_EQ(db_.removeUser(client.entry_id), proto::router::kErrorOk);

    proto::router::WorkspaceList list;
    db_.workspaceListWithAllAccess(admin_.entry_id, workspace_id, &list);
    ASSERT_EQ(list.workspace_size(), 1);
    EXPECT_EQ(list.workspace(0).access_size(), 1);
}

//--------------------------------------------------------------------------------------------------
// The repair path must not store keys sealed to a snapshot that no longer matches the stored
// key of the user: such an entry could never be unsealed and would block the user's own
// password changes forever.
TEST_F(RouterDatabaseTest, RepairSkipsStaleSnapshotAndAppliesFreshOne)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace("alpha", {accessEntry(admin_, gk)});
    ASSERT_GT(workspace_id, 0);

    RouterUser admin2 = makeUser("admin2", kAllSessions);
    std::unordered_map<qint64, QByteArray> wrapped_keys;
    wrapped_keys.emplace(workspace_id, SealedBox::seal(gk, admin2.public_key));
    ASSERT_EQ(db_.addUser(admin2, wrapped_keys, admin_.entry_id), proto::router::kErrorOk);
    admin2 = db_.findUser("admin2");

    // Induce the hole the repair path exists for.
    ASSERT_TRUE(execRaw(QString("DELETE FROM workspace_access WHERE user_id=%1")
                            .arg(admin2.entry_id)));

    // A flags-only request built from a stale key snapshot: the flags apply, the repair is
    // skipped - inserting the keys would store material sealed to the wrong pair.
    RouterUser stale;
    stale.entry_id = admin2.entry_id;
    stale.flags = User::ENABLED;
    stale.public_key = Random::byteArray(32);

    ASSERT_EQ(db_.modifyUser(stale, wrapped_keys, admin_.entry_id), proto::router::kErrorOk);

    std::set<qint64> workspace_ids;
    ASSERT_TRUE(db_.workspaceAccessIdsForUser(admin2.entry_id, &workspace_ids));
    EXPECT_TRUE(workspace_ids.empty());

    // The same request with the current snapshot repairs the access.
    RouterUser fresh;
    fresh.entry_id = admin2.entry_id;
    fresh.flags = User::ENABLED;
    fresh.public_key = admin2.public_key;

    ASSERT_EQ(db_.modifyUser(fresh, wrapped_keys, admin_.entry_id), proto::router::kErrorOk);

    ASSERT_TRUE(db_.workspaceAccessIdsForUser(admin2.entry_id, &workspace_ids));
    EXPECT_TRUE(workspace_ids.contains(workspace_id));
}

//--------------------------------------------------------------------------------------------------
// I3: a new public key with the old salt and verifier is a rotation too - the stored wrapped
// GKs are sealed to the old pair, so the same complete re-sealed set is required, and without
// it nothing (including the new key) may be stored.
TEST_F(RouterDatabaseTest, PublicKeyChangeAloneIsRotation)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace("alpha", {accessEntry(admin_, gk)});
    ASSERT_GT(workspace_id, 0);

    RouterUser swapped = admin_;
    const RouterUser fresh = makeUser("admin", kAllSessions);
    swapped.public_key = fresh.public_key;
    swapped.wrap_private_key = fresh.wrap_private_key;
    swapped.wrap_salt = fresh.wrap_salt;

    EXPECT_EQ(db_.modifyUser(swapped, {}, admin_.entry_id), proto::router::kErrorConflict);
    EXPECT_EQ(db_.findUser(admin_.entry_id).public_key, admin_.public_key);

    std::unordered_map<qint64, QByteArray> wrapped_keys;
    wrapped_keys.emplace(workspace_id, SealedBox::seal(gk, swapped.public_key));

    bool password_changed = false;
    EXPECT_EQ(db_.modifyUser(swapped, wrapped_keys, admin_.entry_id, &password_changed),
              proto::router::kErrorOk);
    EXPECT_TRUE(password_changed);
    EXPECT_EQ(db_.findUser(admin_.entry_id).public_key, swapped.public_key);

    proto::router::WorkspaceList list;
    db_.workspaceListWithAllAccess(admin_.entry_id, workspace_id, &list);
    ASSERT_EQ(list.workspace_size(), 1);
    ASSERT_EQ(list.workspace(0).access_size(), 1);
    EXPECT_EQ(list.workspace(0).access(0).wrapped_gk(),
              toStdString(wrapped_keys[workspace_id]));
}

//--------------------------------------------------------------------------------------------------
// I4: the membership of a workspace changes on the user paths too - granting a new
// administrator and removing a user both move the revision, so a workspace save built from the
// old member list conflicts instead of silently undoing the change.
TEST_F(RouterDatabaseTest, AccessChangesBumpRevision)
{
    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace("alpha", {accessEntry(admin_, gk)});
    ASSERT_GT(workspace_id, 0);
    ASSERT_EQ(workspaceRevision(workspace_id), 1);

    RouterUser admin2 = makeUser("admin2", kAllSessions);
    std::unordered_map<qint64, QByteArray> wrapped_keys;
    wrapped_keys.emplace(workspace_id, SealedBox::seal(gk, admin2.public_key));
    ASSERT_EQ(db_.addUser(admin2, wrapped_keys, admin_.entry_id), proto::router::kErrorOk);
    EXPECT_EQ(workspaceRevision(workspace_id), 2);

    EXPECT_EQ(db_.modifyWorkspace(workspace_id, 1, "alpha", std::string_view(),
                                  {existingEntry(admin_.entry_id)}, {}),
              proto::router::kErrorConflict);

    ASSERT_EQ(db_.removeUser(db_.findUser("admin2").entry_id),
              proto::router::kErrorOk);
    EXPECT_EQ(workspaceRevision(workspace_id), 3);
}

//--------------------------------------------------------------------------------------------------
// An access entry for a user that is gone is how a concurrent delete looks: the sender still
// saw the user in its snapshot, so the answer is a conflict resolved by refetching.
TEST_F(RouterDatabaseTest, EntryForDeletedUserIsConflict)
{
    RouterUser client = makeUser("client", proto::router::SESSION_TYPE_CLIENT);
    ASSERT_EQ(db_.addUser(client), proto::router::kErrorOk);
    client = db_.findUser("client");
    ASSERT_EQ(db_.removeUser(client.entry_id), proto::router::kErrorOk);

    const SecureByteArray gk(Random::byteArray(32));
    qint64 entry_id = -1;
    EXPECT_EQ(db_.addWorkspace("alpha", std::string_view(),
                               {accessEntry(admin_, gk), accessEntry(client, gk)}, {}, &entry_id),
              proto::router::kErrorConflict);

    // The transaction rolled back: the name is still free.
    EXPECT_GT(addWorkspace("alpha", {accessEntry(admin_, gk)}), 0);
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
    renamed.entry_id = db_.findUser("alice").entry_id;
    EXPECT_EQ(db_.modifyUser(renamed, {}, admin_.entry_id), proto::router::kErrorAlreadyExists);
    EXPECT_TRUE(db_.findUser("alice").isValid());
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
