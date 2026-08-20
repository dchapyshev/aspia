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

#include "client/desktop/authorization_dialog.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QTest>

#include <gtest/gtest.h>

#include "client/settings.h"

class AuthorizationDialogTest : public testing::Test
{
public:
    // The dialog takes the state of the one-time password box from the settings when it opens and
    // writes it back when it closes, so every test says what it starts from.
    static void rememberOneTimePassword(bool checked)
    {
        Settings settings;
        settings.setOneTimePasswordChecked(checked);
    }

    static QCheckBox* oneTimePasswordBox(const AuthorizationDialog& dialog)
    {
        return dialog.findChild<QCheckBox*>("checkbox_one_time_password");
    }

    static QCheckBox* saveCredentialsBox(const AuthorizationDialog& dialog)
    {
        return dialog.findChild<QCheckBox*>("checkbox_save_credentials");
    }

    static QLineEdit* userNameEdit(const AuthorizationDialog& dialog)
    {
        return dialog.findChild<QLineEdit*>("edit_username");
    }
};

//--------------------------------------------------------------------------------------------------
// A record that carries a user name is a connection under that name, whatever the box was left at
// the last time the dialog was open.
TEST_F(AuthorizationDialogTest, SavedUserNameTurnsOffTheOneTimePassword)
{
    rememberOneTimePassword(true);

    AuthorizationDialog dialog;
    dialog.setOneTimePasswordEnabled(true);
    dialog.setUserName("admin");
    dialog.show();

    QCheckBox* one_time = oneTimePasswordBox(dialog);
    QLineEdit* username = userNameEdit(dialog);
    ASSERT_TRUE(one_time && username);

    EXPECT_FALSE(one_time->isChecked());
    EXPECT_TRUE(username->isVisible());
    EXPECT_EQ(dialog.userName(), "admin");
}

//--------------------------------------------------------------------------------------------------
// Turning the one-time password on drops the name entered before it: the field goes away, and what
// it held goes with it.
TEST_F(AuthorizationDialogTest, TurningOnTheOneTimePasswordDropsTheUserName)
{
    rememberOneTimePassword(false);

    AuthorizationDialog dialog;
    dialog.setOneTimePasswordEnabled(true);
    dialog.setUserName("admin");
    dialog.show();

    QCheckBox* one_time = oneTimePasswordBox(dialog);
    QLineEdit* username = userNameEdit(dialog);
    ASSERT_TRUE(one_time && username);

    QTest::mouseClick(one_time, Qt::LeftButton);

    EXPECT_TRUE(one_time->isChecked());
    EXPECT_FALSE(username->isVisible());
    EXPECT_TRUE(username->text().isEmpty());
    EXPECT_TRUE(dialog.userName().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A one-time password is asked of the host itself, and the name of the record has nothing to do
// with it. A name the field still holds while it is hidden is not the answer of the user: handed
// over, it would send the connection under a user the host does not know.
TEST_F(AuthorizationDialogTest, NameBehindTheHiddenFieldIsNotHandedOver)
{
    rememberOneTimePassword(true);

    // The box is ticked, but the host of this record is reached without a router, so the choice
    // does not count yet and the name is asked for.
    AuthorizationDialog dialog;
    dialog.show();

    QLineEdit* username = userNameEdit(dialog);
    ASSERT_TRUE(username);

    QTest::keyClicks(username, "admin");
    ASSERT_EQ(dialog.userName(), "admin");

    // The record turns out to be reached through a router, and the ticked box now counts.
    dialog.setOneTimePasswordEnabled(true);

    EXPECT_FALSE(username->isVisible());
    EXPECT_EQ(username->text(), "admin");
    EXPECT_TRUE(dialog.userName().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// What the user typed is what the connection is made with.
TEST_F(AuthorizationDialogTest, UserNameTypedByTheUserIsHandedOver)
{
    rememberOneTimePassword(false);

    AuthorizationDialog dialog;
    dialog.setOneTimePasswordEnabled(true);
    dialog.show();

    QLineEdit* username = userNameEdit(dialog);
    ASSERT_TRUE(username);

    QTest::keyClicks(username, "operator");

    EXPECT_EQ(dialog.userName(), "operator");
}

//--------------------------------------------------------------------------------------------------
// A one-time password is good for one connection, so there is nothing to keep and the box is not
// shown. What the user was never shown is not his answer.
TEST_F(AuthorizationDialogTest, CredentialsOfAOneTimeConnectionAreNotOffered)
{
    rememberOneTimePassword(true);

    AuthorizationDialog dialog;
    dialog.setOneTimePasswordEnabled(true);
    dialog.setSaveCredentialsVisible(true);
    dialog.show();

    QCheckBox* save = saveCredentialsBox(dialog);
    ASSERT_TRUE(save);

    save->setChecked(true);

    EXPECT_FALSE(save->isVisible());
    EXPECT_FALSE(dialog.isSaveCredentialsChecked());
}

//--------------------------------------------------------------------------------------------------
// A host reached without a router keeps nothing of its own, and the box is not shown there either.
TEST_F(AuthorizationDialogTest, CredentialsAreNotOfferedWhenTheyAreNotAskedFor)
{
    rememberOneTimePassword(false);

    AuthorizationDialog dialog;
    dialog.setOneTimePasswordEnabled(false);
    dialog.setSaveCredentialsVisible(false);
    dialog.show();

    QCheckBox* save = saveCredentialsBox(dialog);
    ASSERT_TRUE(save);

    save->setChecked(true);

    EXPECT_FALSE(save->isVisible());
    EXPECT_FALSE(dialog.isSaveCredentialsChecked());
}

//--------------------------------------------------------------------------------------------------
// The box keeps its state between connections, and that state is what the user chose. A record
// that carries a user name clears the box for the dialog it opens and for that dialog only.
TEST_F(AuthorizationDialogTest, SavedUserNameLeavesTheRememberedChoiceAlone)
{
    rememberOneTimePassword(true);

    {
        AuthorizationDialog dialog;
        dialog.setOneTimePasswordEnabled(true);
        dialog.setUserName("admin");
        dialog.show();

        QCheckBox* one_time = oneTimePasswordBox(dialog);
        ASSERT_TRUE(one_time);
        EXPECT_FALSE(one_time->isChecked());
    }

    Settings settings;
    EXPECT_TRUE(settings.isOneTimePasswordChecked());
}

//--------------------------------------------------------------------------------------------------
// What the user does with the box is what comes back the next time the dialog opens.
TEST_F(AuthorizationDialogTest, ChoiceOfTheUserIsRemembered)
{
    rememberOneTimePassword(false);

    {
        AuthorizationDialog dialog;
        dialog.setOneTimePasswordEnabled(true);
        dialog.show();

        QCheckBox* one_time = oneTimePasswordBox(dialog);
        ASSERT_TRUE(one_time);

        QTest::mouseClick(one_time, Qt::LeftButton);
        ASSERT_TRUE(one_time->isChecked());
    }

    Settings settings;
    EXPECT_TRUE(settings.isOneTimePasswordChecked());
}
