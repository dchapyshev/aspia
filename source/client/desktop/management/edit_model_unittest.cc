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
WorkspaceEditModel::User makeUser(qint64 id)
{
    WorkspaceEditModel::User user;
    user.entry_id = id;
    user.name = QString("user-%1").arg(id);
    return user;
}

//--------------------------------------------------------------------------------------------------
WorkspaceEditModel::WorkspaceInfo makeWorkspace(
    qint64 id, qint64 revision, const QSet<qint64>& access_ids)
{
    WorkspaceEditModel::WorkspaceInfo workspace;
    workspace.entry_id = id;
    workspace.revision = revision;
    workspace.name = QString("workspace-%1").arg(id);
    workspace.access_ids = access_ids;
    return workspace;
}

//--------------------------------------------------------------------------------------------------
RouterUser makeRecord(quint32 flags)
{
    RouterUser user;
    user.entry_id = kClientId;
    user.name = "bob";
    user.flags = flags;
    user.public_key = QByteArray("key");
    return user;
}

//--------------------------------------------------------------------------------------------------
// The standard modify-mode fixture state: workspace 7 at revision 1 with the admin as its only
// member, and a page of the user list holding both users.
void loadDefault(WorkspaceEditModel* model)
{
    ASSERT_TRUE(model->applyWorkspaceList({makeWorkspace(kWorkspaceId, 1, {kAdminId})}));
    model->applyUserPage({makeUser(kAdminId), makeUser(kClientId)});
    model->applyMemberUser(makeUser(kAdminId));
    ASSERT_TRUE(model->isLoaded());
}

} // namespace

//--------------------------------------------------------------------------------------------------
// The revision the save is built on is the one of the last workspace reply.
TEST(WorkspaceEditModel, RevisionFollowsTheWorkspaceReply)
{
    WorkspaceEditModel model(kWorkspaceId);
    loadDefault(&model);
    EXPECT_EQ(model.baseRevision(), 1);

    ASSERT_TRUE(model.applyWorkspaceList({makeWorkspace(kWorkspaceId, 2, {kAdminId})}));
    EXPECT_EQ(model.baseRevision(), 2);
}

//--------------------------------------------------------------------------------------------------
TEST(WorkspaceEditModel, IntentsSurviveRefetch)
{
    WorkspaceEditModel model(kWorkspaceId);
    loadDefault(&model);

    model.grantUser(kClientId);

    // A refetch with identical server state must not disturb the edits.
    ASSERT_TRUE(model.applyWorkspaceList({makeWorkspace(kWorkspaceId, 1, {kAdminId})}));
    model.applyUserPage({makeUser(kAdminId), makeUser(kClientId)});

    EXPECT_TRUE(model.effectiveAccessIds().contains(kClientId));
}

//--------------------------------------------------------------------------------------------------
// The record of a member was deleted from another console. Its entry goes with it, because the
// router refuses an access entry for a user it cannot find.
TEST(WorkspaceEditModel, MissingMemberIsDropped)
{
    WorkspaceEditModel model(kWorkspaceId);
    loadDefault(&model);

    model.grantUser(kClientId);
    EXPECT_TRUE(model.effectiveAccessIds().contains(kClientId));

    model.applyMissingUser(kClientId);
    EXPECT_FALSE(model.effectiveAccessIds().contains(kClientId));

    // The record is back (a lookup of the next refetch found it): so is the entry.
    model.applyMemberUser(makeUser(kClientId));
    EXPECT_TRUE(model.effectiveAccessIds().contains(kClientId));
}

//--------------------------------------------------------------------------------------------------
// The users of the page that are not members yet are the candidates; the members are not offered
// again.
TEST(WorkspaceEditModel, MembersAreNotOfferedAgain)
{
    WorkspaceEditModel model(kWorkspaceId);

    ASSERT_TRUE(model.applyWorkspaceList({makeWorkspace(kWorkspaceId, 1, {kClientId})}));
    model.applyUserPage({makeUser(kAdminId), makeUser(kClientId), makeUser(kKeylessId)});
    model.applyMemberUser(makeUser(kClientId));

    QSet<qint64> available_ids;
    for (const WorkspaceEditModel::User& user : model.availableUsers())
        available_ids.insert(user.entry_id);
    EXPECT_EQ(available_ids, QSet<qint64>({kAdminId, kKeylessId}));

    QSet<qint64> member_ids;
    for (const WorkspaceEditModel::User& user : model.memberUsers())
        member_ids.insert(user.entry_id);
    EXPECT_EQ(member_ids, QSet<qint64>({kClientId}));
}

//--------------------------------------------------------------------------------------------------
// A user granted from the page of candidates is known by name already: it must not turn into a
// bare id while a lookup that would answer what is on the screen is in flight.
TEST(WorkspaceEditModel, GrantedUserKeepsTheNameOfThePage)
{
    WorkspaceEditModel model(kWorkspaceId);
    loadDefault(&model);

    model.grantUser(kClientId);

    ASSERT_EQ(model.memberUsers().size(), 2);
    for (const WorkspaceEditModel::User& user : model.memberUsers())
        EXPECT_FALSE(user.name.isEmpty());

    EXPECT_TRUE(model.unresolvedMemberIds().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The membership is shown whole, so a member the current page does not carry is listed anyway and
// its name is asked for separately.
TEST(WorkspaceEditModel, MemberOutsideThePageIsListedAndLookedUp)
{
    WorkspaceEditModel model(kWorkspaceId);

    ASSERT_TRUE(model.applyWorkspaceList({makeWorkspace(kWorkspaceId, 1, {kKeylessId})}));
    model.applyUserPage({makeUser(kAdminId), makeUser(kClientId)});

    ASSERT_EQ(model.memberUsers().size(), 1);
    EXPECT_EQ(model.memberUsers().front().entry_id, kKeylessId);
    EXPECT_TRUE(model.memberUsers().front().name.isEmpty());
    EXPECT_EQ(model.unresolvedMemberIds(), QList<qint64>({kKeylessId}));

    model.applyMemberUser(makeUser(kKeylessId));
    EXPECT_EQ(model.memberUsers().front().name, QString("user-%1").arg(kKeylessId));
    EXPECT_TRUE(model.unresolvedMemberIds().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The save carries the complete membership, the granted user included.
TEST(WorkspaceEditModel, SaveCarriesTheWholeMembership)
{
    WorkspaceEditModel model(kWorkspaceId);
    loadDefault(&model);

    model.grantUser(kClientId);

    const QList<qint64> user_ids = model.accessUserIdsForSave();
    EXPECT_EQ(QSet<qint64>(user_ids.begin(), user_ids.end()),
              QSet<qint64>({kAdminId, kClientId}));
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

    (void)model.accessUserIdsForSave();

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
    ASSERT_TRUE(model.applySnapshot(makeRecord(User::ENABLED), true));
    model.setAccountChanged(false);

    EXPECT_TRUE(model.isNoOpSave());

    // Console B disabled the user; the refetch delivered it. Still nothing edited here.
    ASSERT_TRUE(model.applySnapshot(makeRecord(0), true));
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
    ASSERT_TRUE(model.applySnapshot(makeRecord(User::ENABLED), true));
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
    ASSERT_TRUE(model.applySnapshot(makeRecord(User::ENABLED), true));
    model.setAccountChanged(false);

    model.setEnabledIntent(false);
    EXPECT_TRUE(model.enabledTouched());

    // The operator undid the click: no edit remains, the checkbox follows the server again.
    model.setEnabledIntent(true);
    EXPECT_FALSE(model.enabledTouched());

    ASSERT_TRUE(model.applySnapshot(makeRecord(0), true));
    EXPECT_FALSE(model.desiredEnabled());
    EXPECT_TRUE(model.isNoOpSave());
}

//--------------------------------------------------------------------------------------------------
TEST(UserEditModel, IntentSurvivesRefetchUntilServerMatches)
{
    UserEditModel model(kClientId);
    ASSERT_TRUE(model.applySnapshot(makeRecord(User::ENABLED), true));
    model.setAccountChanged(false);

    model.setEnabledIntent(false);

    // A refetch with the unchanged server state keeps the edit.
    ASSERT_TRUE(model.applySnapshot(makeRecord(User::ENABLED), true));
    EXPECT_FALSE(model.desiredEnabled());
    EXPECT_FALSE(model.isNoOpSave());

    // Another console applied the same change: the intent dissolves into the snapshot and the
    // pending OK becomes a no-op instead of a redundant save.
    ASSERT_TRUE(model.applySnapshot(makeRecord(0), true));
    EXPECT_FALSE(model.enabledTouched());
    EXPECT_FALSE(model.desiredEnabled());
    EXPECT_TRUE(model.isNoOpSave());
}

//--------------------------------------------------------------------------------------------------
TEST(UserEditModel, DeletedRecordIsDetected)
{
    UserEditModel model(kClientId);
    ASSERT_TRUE(model.applySnapshot(makeRecord(User::ENABLED), true));
    EXPECT_FALSE(model.applySnapshot(RouterUser(), false));
}

//--------------------------------------------------------------------------------------------------
TEST(UserEditModel, CreateModeDefaults)
{
    UserEditModel model(0);
    ASSERT_TRUE(model.applySnapshot(RouterUser(), false));

    EXPECT_TRUE(model.accountChanged());
    EXPECT_TRUE(model.desiredEnabled());
    EXPECT_FALSE(model.isNoOpSave());
    EXPECT_EQ(model.flagsForSave(), quint32(User::ENABLED));

    model.setEnabledIntent(false);
    EXPECT_EQ(model.flagsForSave(), 0u);
}
