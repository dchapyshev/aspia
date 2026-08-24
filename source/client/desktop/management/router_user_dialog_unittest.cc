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

#include <gtest/gtest.h>

#include "client/router_controller.h"
#include "common/desktop/password_edit.h"

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
