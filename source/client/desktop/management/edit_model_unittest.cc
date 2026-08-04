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

#include "client/desktop/management/user_edit_model.h"
#include "client/desktop/management/workspace_edit_model.h"

#include <gtest/gtest.h>

#include "base/peer/user.h"

namespace {

constexpr qint64 kWorkspaceId = 7;
constexpr qint64 kAdminId = 1;
constexpr qint64 kClientId = 2;
constexpr qint64 kKeylessId = 3;

//--------------------------------------------------------------------------------------------------
WorkspaceEditModel::User makeUser(qint64 id, bool is_admin, const char* key = "key")
{
    WorkspaceEditModel::User user;
    user.entry_id = id;
    user.is_admin = is_admin;
    user.name = QStringLiteral("user-%1").arg(id);
    user.public_key = QByteArray(key);
    return user;
}

//--------------------------------------------------------------------------------------------------
WorkspaceEditModel::HostInfo makeHost(quint64 id, qint64 workspace_id)
{
    WorkspaceEditModel::HostInfo host;
    host.host_id = id;
    host.workspace_id = workspace_id;
    host.computer_name = QStringLiteral("host-%1").arg(id);
    return host;
}

//--------------------------------------------------------------------------------------------------
WorkspaceEditModel::WorkspaceInfo makeWorkspace(
    qint64 id, qint64 revision, const QSet<qint64>& access_ids)
{
    WorkspaceEditModel::WorkspaceInfo workspace;
    workspace.entry_id = id;
    workspace.revision = revision;
    workspace.name = QStringLiteral("workspace-%1").arg(id);
    workspace.access_ids = access_ids;
    return workspace;
}

//--------------------------------------------------------------------------------------------------
RouterUser makeRecord(quint32 flags)
{
    RouterUser user;
    user.entry_id = kClientId;
    user.name = QStringLiteral("bob");
    user.flags = flags;
    user.public_key = QByteArray("key");
    return user;
}

//--------------------------------------------------------------------------------------------------
// The standard modify-mode fixture state: workspace 7 at revision 1, admin has access, one
// unassigned host 100 and one owned host 200.
void loadDefault(WorkspaceEditModel* model)
{
    ASSERT_TRUE(model->applyWorkspaceList({makeWorkspace(kWorkspaceId, 1, {kAdminId})}));
    model->applyUserList({makeUser(kAdminId, true), makeUser(kClientId, false)});
    model->applyHostList({makeHost(100, 0), makeHost(200, kWorkspaceId)});
    ASSERT_TRUE(model->isLoaded());
}

} // namespace

//--------------------------------------------------------------------------------------------------
// The race that motivated the pairing: another console adds a host (bumping the revision), our
// refetch has already delivered the new revision but not yet the new host list. A save issued
// in that window must not pair the fresh revision with the stale host set - it would pass the
// revision check and silently release the new host. With the pairing the save carries the OLD
// revision, which the router rejects with "conflict".
TEST(WorkspaceEditModel, RevisionCommittedOnlyTogetherWithHostSnapshot)
{
    WorkspaceEditModel model(kWorkspaceId);
    loadDefault(&model);
    EXPECT_EQ(model.baseRevision(), 1);

    // Refetch cycle: the workspace reply (revision 2) landed, the host reply is still in
    // flight. The save must still be based on revision 1.
    ASSERT_TRUE(model.applyWorkspaceList({makeWorkspace(kWorkspaceId, 2, {kAdminId})}));
    EXPECT_EQ(model.baseRevision(), 1);

    // The paired host reply lands (with the host 300 the other console added): only now the
    // revision moves, together with the host snapshot that matches it.
    model.applyHostList(
        {makeHost(100, 0), makeHost(200, kWorkspaceId), makeHost(300, kWorkspaceId)});
    EXPECT_EQ(model.baseRevision(), 2);
    EXPECT_TRUE(model.effectiveHostIds().contains(300));
}

//--------------------------------------------------------------------------------------------------
TEST(WorkspaceEditModel, IntentsSurviveRefetch)
{
    WorkspaceEditModel model(kWorkspaceId);
    loadDefault(&model);

    model.grantUser(kClientId);
    model.claimHost(100);
    model.releaseHost(200);

    // A refetch with identical server state must not disturb the edits.
    ASSERT_TRUE(model.applyWorkspaceList({makeWorkspace(kWorkspaceId, 1, {kAdminId})}));
    model.applyUserList({makeUser(kAdminId, true), makeUser(kClientId, false)});
    model.applyHostList({makeHost(100, 0), makeHost(200, kWorkspaceId)});

    EXPECT_TRUE(model.effectiveAccessIds().contains(kClientId));
    EXPECT_TRUE(model.effectiveHostIds().contains(100));
    EXPECT_FALSE(model.effectiveHostIds().contains(200));
}

//--------------------------------------------------------------------------------------------------
TEST(WorkspaceEditModel, KeyedAdminsAutoIncludedAndNotRevocable)
{
    WorkspaceEditModel model(kWorkspaceId);

    // The admin is not in the server access list (created concurrently) - it must be included
    // anyway: the router accepts a workspace only with every keyed administrator present.
    ASSERT_TRUE(model.applyWorkspaceList({makeWorkspace(kWorkspaceId, 1, {})}));
    model.applyUserList({makeUser(kAdminId, true)});
    model.applyHostList({});

    EXPECT_TRUE(model.effectiveAccessIds().contains(kAdminId));
    EXPECT_FALSE(model.canRevokeUser(kAdminId));

    // Even an explicit revoke cannot drop it.
    model.revokeUser(kAdminId);
    EXPECT_TRUE(model.effectiveAccessIds().contains(kAdminId));
}

//--------------------------------------------------------------------------------------------------
TEST(WorkspaceEditModel, VanishedEntriesAreDropped)
{
    WorkspaceEditModel model(kWorkspaceId);
    loadDefault(&model);

    model.grantUser(kClientId);
    model.claimHost(100);

    // The user and the host were deleted from another console: the refetch no longer lists
    // them, and the save must not send entries the router cannot resolve.
    ASSERT_TRUE(model.applyWorkspaceList({makeWorkspace(kWorkspaceId, 2, {kAdminId})}));
    model.applyUserList({makeUser(kAdminId, true)});
    model.applyHostList({makeHost(200, kWorkspaceId)});

    EXPECT_FALSE(model.effectiveAccessIds().contains(kClientId));
    EXPECT_FALSE(model.effectiveHostIds().contains(100));
}

//--------------------------------------------------------------------------------------------------
TEST(WorkspaceEditModel, KeylessUserNotOfferedButKeptAsMember)
{
    WorkspaceEditModel model(kWorkspaceId);

    // The keyless client user is a member per the server (grandfathered); the keyless
    // non-member must not be offered - nobody can seal the group key for it.
    ASSERT_TRUE(
        model.applyWorkspaceList({makeWorkspace(kWorkspaceId, 1, {kAdminId, kClientId})}));

    WorkspaceEditModel::User keyless_member = makeUser(kClientId, false, "");
    WorkspaceEditModel::User keyless_candidate = makeUser(kKeylessId, false, "");
    model.applyUserList({makeUser(kAdminId, true), keyless_member, keyless_candidate});
    model.applyHostList({});

    const QList<WorkspaceEditModel::User> available = model.availableUsers();
    EXPECT_TRUE(available.isEmpty());

    QSet<qint64> member_ids;
    for (const WorkspaceEditModel::User& user : model.memberUsers())
        member_ids.insert(user.entry_id);
    EXPECT_TRUE(member_ids.contains(kClientId));
}

//--------------------------------------------------------------------------------------------------
TEST(WorkspaceEditModel, SaveAttachesKeyOnlyForNewGrants)
{
    WorkspaceEditModel model(kWorkspaceId);
    loadDefault(&model);

    model.grantUser(kClientId);

    for (const WorkspaceEditModel::AccessEntry& entry : model.accessEntriesForSave())
    {
        if (entry.user_id == kAdminId)
        {
            // Already granted per the server: the router keeps the stored sealed key.
            EXPECT_TRUE(entry.public_key.isEmpty());
        }
        else
        {
            // A new grant carries the seal target.
            EXPECT_EQ(entry.user_id, kClientId);
            EXPECT_FALSE(entry.public_key.isEmpty());
        }
    }
}

//--------------------------------------------------------------------------------------------------
TEST(WorkspaceEditModel, GrantRevokeGrantYieldsGrant)
{
    WorkspaceEditModel model(kWorkspaceId);
    loadDefault(&model);

    model.grantUser(kClientId);
    model.revokeUser(kClientId);
    model.grantUser(kClientId);
    EXPECT_TRUE(model.effectiveAccessIds().contains(kClientId));

    model.revokeUser(kClientId);
    EXPECT_FALSE(model.effectiveAccessIds().contains(kClientId));
}

//--------------------------------------------------------------------------------------------------
TEST(WorkspaceEditModel, DeletedWorkspaceIsDetected)
{
    WorkspaceEditModel model(kWorkspaceId);
    loadDefault(&model);

    // The refetch no longer contains the edited workspace: deleted from another console.
    EXPECT_FALSE(model.applyWorkspaceList({makeWorkspace(kWorkspaceId + 1, 1, {})}));
}

//--------------------------------------------------------------------------------------------------
TEST(WorkspaceEditModel, ForeignHostsAreInvisible)
{
    WorkspaceEditModel model(kWorkspaceId);
    ASSERT_TRUE(model.applyWorkspaceList({makeWorkspace(kWorkspaceId, 1, {kAdminId})}));
    model.applyUserList({makeUser(kAdminId, true)});
    model.applyHostList({makeHost(100, 0), makeHost(200, kWorkspaceId), makeHost(300, 99)});

    EXPECT_FALSE(model.effectiveHostIds().contains(300));
    EXPECT_EQ(model.availableHosts().size(), 1);
    EXPECT_EQ(model.hostsInWorkspace().size(), 1);

    // The release warning applies only to hosts the server has in this workspace.
    EXPECT_TRUE(model.isServerHost(200));
    EXPECT_FALSE(model.isServerHost(100));
}

//--------------------------------------------------------------------------------------------------
TEST(WorkspaceEditModel, CreateModeCollectsAllNames)
{
    WorkspaceEditModel model(0);
    EXPECT_TRUE(model.applyWorkspaceList(
        {makeWorkspace(1, 1, {}), makeWorkspace(2, 1, {})}));
    EXPECT_EQ(model.otherNames().size(), 2);
}

//--------------------------------------------------------------------------------------------------
// A failed save must leave the model untouched so the retry is built from the same true state.
// The model has no mutating save API at all - this test pins that the save assembly itself is
// side-effect free.
TEST(WorkspaceEditModel, SaveAssemblyDoesNotMutateState)
{
    WorkspaceEditModel model(kWorkspaceId);
    loadDefault(&model);
    model.grantUser(kClientId);

    const QSet<qint64> access_before = model.effectiveAccessIds();
    const qint64 revision_before = model.baseRevision();

    (void)model.accessEntriesForSave();
    (void)model.hostIdsForSave();

    EXPECT_EQ(model.effectiveAccessIds(), access_before);
    EXPECT_EQ(model.baseRevision(), revision_before);
}

//--------------------------------------------------------------------------------------------------
// The scenario of the silent-overwrite bug: console A opens the dialog, console B disables the
// user, console A clicks OK without editing anything. The save must be a no-op - not an echo
// of the stale snapshot that would re-enable the user.
TEST(UserEditModel, UnchangedSaveIsNoOpOverConcurrentChange)
{
    UserEditModel model(kClientId);
    ASSERT_TRUE(model.applySnapshot(makeRecord(User::ENABLED), true, {}));
    model.setAccountChanged(false);

    EXPECT_TRUE(model.isNoOpSave());

    // Console B disabled the user; the refetch delivered it. Still nothing edited here.
    ASSERT_TRUE(model.applySnapshot(makeRecord(0), true, {}));
    EXPECT_FALSE(model.desiredEnabled());
    EXPECT_TRUE(model.isNoOpSave());
}

//--------------------------------------------------------------------------------------------------
// The regression the review caught: after a failed save the retry compared the checkbox with a
// snapshot that had been overwritten by the intended value, misread "no changes" and closed
// without sending. The model never lets a save mutate the snapshot, so the retry still counts
// as an edit.
TEST(UserEditModel, RetryAfterFailedSaveStillSends)
{
    UserEditModel model(kClientId);
    ASSERT_TRUE(model.applySnapshot(makeRecord(User::ENABLED), true, {}));
    model.setAccountChanged(false);

    model.setEnabledIntent(false);
    EXPECT_FALSE(model.isNoOpSave());
    EXPECT_EQ(model.flagsForSave(), 0u);

    // The save failed (conflict, internal error - does not matter): nothing in the model
    // changed, the second click must still produce a request.
    EXPECT_FALSE(model.isNoOpSave());
    EXPECT_EQ(model.flagsForSave(), 0u);
}

//--------------------------------------------------------------------------------------------------
TEST(UserEditModel, ToggleBackReattachesToRefetches)
{
    UserEditModel model(kClientId);
    ASSERT_TRUE(model.applySnapshot(makeRecord(User::ENABLED), true, {}));
    model.setAccountChanged(false);

    model.setEnabledIntent(false);
    EXPECT_TRUE(model.enabledTouched());

    // The operator undid the click: no edit remains, the checkbox follows the server again.
    model.setEnabledIntent(true);
    EXPECT_FALSE(model.enabledTouched());

    ASSERT_TRUE(model.applySnapshot(makeRecord(0), true, {}));
    EXPECT_FALSE(model.desiredEnabled());
    EXPECT_TRUE(model.isNoOpSave());
}

//--------------------------------------------------------------------------------------------------
TEST(UserEditModel, IntentSurvivesRefetchUntilServerMatches)
{
    UserEditModel model(kClientId);
    ASSERT_TRUE(model.applySnapshot(makeRecord(User::ENABLED), true, {}));
    model.setAccountChanged(false);

    model.setEnabledIntent(false);

    // A refetch with the unchanged server state keeps the edit.
    ASSERT_TRUE(model.applySnapshot(makeRecord(User::ENABLED), true, {}));
    EXPECT_FALSE(model.desiredEnabled());
    EXPECT_FALSE(model.isNoOpSave());

    // Another console applied the same change: the intent dissolves into the snapshot and the
    // pending OK becomes a no-op instead of a redundant save.
    ASSERT_TRUE(model.applySnapshot(makeRecord(0), true, {}));
    EXPECT_FALSE(model.enabledTouched());
    EXPECT_FALSE(model.desiredEnabled());
    EXPECT_TRUE(model.isNoOpSave());
}

//--------------------------------------------------------------------------------------------------
TEST(UserEditModel, DeletedRecordIsDetected)
{
    UserEditModel model(kClientId);
    ASSERT_TRUE(model.applySnapshot(makeRecord(User::ENABLED), true, {}));
    EXPECT_FALSE(model.applySnapshot(RouterUser(), false, {}));
}

//--------------------------------------------------------------------------------------------------
TEST(UserEditModel, CreateModeDefaults)
{
    UserEditModel model(0);
    ASSERT_TRUE(model.applySnapshot(RouterUser(), false, {QStringLiteral("admin")}));

    EXPECT_TRUE(model.accountChanged());
    EXPECT_TRUE(model.desiredEnabled());
    EXPECT_FALSE(model.isNoOpSave());
    EXPECT_EQ(model.flagsForSave(), quint32(User::ENABLED));
    EXPECT_EQ(model.otherNames().size(), 1);

    model.setEnabledIntent(false);
    EXPECT_EQ(model.flagsForSave(), 0u);
}
