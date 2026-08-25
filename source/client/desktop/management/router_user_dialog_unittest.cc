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

#include "client/desktop/management/router_user_dialog.h"

#include <QLineEdit>
#include <QTest>
#include <QTreeWidget>

#include <gtest/gtest.h>

#include "client/router_controller.h"
#include "common/desktop/password_edit.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"

class RouterUserDialogTestPeer
{
public:
    static void receiveTokenList(RouterUserDialog& dialog, const proto::router::UserTokenList& list)
    {
        dialog.onTokenListReceived(list);
    }

    static void receiveResetOtpResult(RouterUserDialog& dialog, const proto::router::UserResult& result)
    {
        dialog.onResetOtpResultReceived(result);
    }

    static void receiveRevokeResult(RouterUserDialog& dialog, const proto::router::UserTokenResult& result)
    {
        dialog.onRevokeResultReceived(result);
    }

    static void setClosing(RouterUserDialog& dialog) { dialog.closing_ = true; }

    static void addToken(RouterUserDialog& dialog, qint64 token_id)
    {
        RouterUserDialog::Token token;
        token.token_id = token_id;
        dialog.tokens_.append(token);
    }

    static void setPendingRevoke(RouterUserDialog& dialog, const QList<qint64>& token_ids)
    {
        dialog.pending_revoke_token_ids_ = token_ids;
    }

    static int tokenCount(const RouterUserDialog& dialog) { return dialog.tokens_.size(); }
};

//--------------------------------------------------------------------------------------------------
// The account edit begins once, however often the notice repeats: every keystroke of the name
// says "the account is being edited" again, and the password typed in between must survive it.
TEST(RouterUserDialogTest, TypingTheNameKeepsTheEnteredPassword)
{
    // The dialog reaches the controller for its signals; there is no worker in this stand.
    RouterController controller;

    RouterUserDialog dialog(1, 0, nullptr);
    dialog.show();

    auto* password = dialog.findChild<PasswordEdit*>("edit_password");
    auto* password_retry = dialog.findChild<PasswordEdit*>("edit_password_retry");
    auto* username = dialog.findChild<QLineEdit*>("edit_username");
    ASSERT_TRUE(password && password_retry && username);

    QTest::keyClicks(password, "Secret1");
    QTest::keyClicks(password_retry, "Secret1");
    QTest::keyClicks(username, "bob");

    EXPECT_EQ(password->password().toString(), QString("Secret1"));
    EXPECT_EQ(password_retry->password().toString(), QString("Secret1"));
}

//--------------------------------------------------------------------------------------------------
// A device list that failed to load must not pass for an empty one, because an empty tab reads as
// "no device can log in without a code". Until a list has arrived the failure grays the tab out,
// and the first list to arrive brings it back.
TEST(RouterUserDialogTest, FailedTokenListGraysTheSessionsTab)
{
    RouterController controller;

    RouterUserDialog dialog(1, 7, nullptr);
    dialog.show();

    auto* tab = dialog.findChild<QWidget*>("tab_sessions");
    ASSERT_TRUE(tab);
    EXPECT_TRUE(tab->isEnabled());

    proto::router::UserTokenList failed;
    failed.set_error_code(proto::router::kErrorInternalError);
    RouterUserDialogTestPeer::receiveTokenList(dialog, failed);
    EXPECT_FALSE(tab->isEnabled());

    proto::router::UserTokenList list;
    list.set_error_code(proto::router::kErrorOk);
    RouterUserDialogTestPeer::receiveTokenList(dialog, list);
    EXPECT_TRUE(tab->isEnabled());
}

//--------------------------------------------------------------------------------------------------
// The row of the token behind the asking session is the one an administrator must not revoke by
// accident, so it is set in bold with a tooltip. The list also arrives whole every time, and a
// refetch replaces the rows instead of stacking them.
TEST(RouterUserDialogTest, TokenListMarksTheSessionRowAndReplacesTheOld)
{
    RouterController controller;

    RouterUserDialog dialog(1, 7, nullptr);
    dialog.show();

    proto::router::UserTokenList list;
    list.set_error_code(proto::router::kErrorOk);
    list.set_current_token_id(9);
    list.add_token()->set_token_id(5);
    list.add_token()->set_token_id(9);

    RouterUserDialogTestPeer::receiveTokenList(dialog, list);

    auto* tree = dialog.findChild<QTreeWidget*>("tree_tokens");
    ASSERT_TRUE(tree);
    ASSERT_EQ(tree->topLevelItemCount(), 2);

    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        const QTreeWidgetItem* item = tree->topLevelItem(i);
        const bool mine = item->data(0, Qt::UserRole).toLongLong() == 9;
        EXPECT_EQ(item->font(0).bold(), mine);
        EXPECT_EQ(!item->toolTip(0).isEmpty(), mine);
    }

    // The same list again does not stack a second copy of the rows.
    RouterUserDialogTestPeer::receiveTokenList(dialog, list);
    EXPECT_EQ(tree->topLevelItemCount(), 2);
}

//--------------------------------------------------------------------------------------------------
// Replies landing on a dialog that is already going away must change nothing in it. The guard
// that keeps a late save result from stacking a message box covers the reset and revoke replies
// the same way.
TEST(RouterUserDialogTest, ResultsAfterTheDialogStartedClosingChangeNothing)
{
    RouterController controller;

    RouterUserDialog dialog(1, 7, nullptr);
    dialog.show();

    RouterUserDialogTestPeer::addToken(dialog, 5);
    RouterUserDialogTestPeer::setPendingRevoke(dialog, { 5 });
    RouterUserDialogTestPeer::setClosing(dialog);

    proto::router::UserResult reset;
    reset.set_error_code(proto::router::kErrorOk);
    RouterUserDialogTestPeer::receiveResetOtpResult(dialog, reset);
    EXPECT_EQ(RouterUserDialogTestPeer::tokenCount(dialog), 1);

    proto::router::UserTokenResult revoke;
    revoke.set_error_code(proto::router::kErrorOk);
    RouterUserDialogTestPeer::receiveRevokeResult(dialog, revoke);
    EXPECT_EQ(RouterUserDialogTestPeer::tokenCount(dialog), 1);
}
