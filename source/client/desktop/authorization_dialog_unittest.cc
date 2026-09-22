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
#include <QComboBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QTest>

#include <gtest/gtest.h>

#include "base/crypto/secure_string.h"
#include "client/config.h"
#include "client/settings.h"

class AuthorizationDialogTest : public testing::Test
{
public:
    // The dialog takes the one-time password choice from the settings when it opens and writes
    // it back when it closes, so every test says what it starts from.
    static void rememberOneTimePassword(bool checked)
    {
        Settings settings;
        settings.setOneTimePasswordChecked(checked);
    }

    static QRadioButton* oneTimePasswordButton(const AuthorizationDialog& dialog)
    {
        return dialog.findChild<QRadioButton*>("radio_one_time_password");
    }

    static QCheckBox* saveCredentialsBox(const AuthorizationDialog& dialog)
    {
        return dialog.findChild<QCheckBox*>("checkbox_save_credentials");
    }

    static QLineEdit* userNameEdit(const AuthorizationDialog& dialog)
    {
        return dialog.findChild<QLineEdit*>("edit_username");
    }

    static QRadioButton* savedCredentialsButton(const AuthorizationDialog& dialog)
    {
        return dialog.findChild<QRadioButton*>("radio_saved_credentials");
    }

    static QComboBox* credentialCombo(const AuthorizationDialog& dialog)
    {
        return dialog.findChild<QComboBox*>("combo_credential");
    }

    static CredentialConfig credential(qint64 id, const QString& name, const QString& username,
                                       const QString& password)
    {
        CredentialConfig credential;
        credential.setId(id);
        credential.setDisplayName(name);
        credential.setUsername(username);
        credential.setPassword(SecureString(password));
        return credential;
    }
};

//--------------------------------------------------------------------------------------------------
// A record that carries a user name is a connection under that name, whatever was chosen the last
// time the dialog was open.
TEST_F(AuthorizationDialogTest, SavedUserNameTurnsOffTheOneTimePassword)
{
    rememberOneTimePassword(true);

    AuthorizationDialog dialog;
    dialog.setOneTimePasswordEnabled(true);
    dialog.setUserName("admin");
    dialog.show();

    QRadioButton* one_time = oneTimePasswordButton(dialog);
    QLineEdit* username = userNameEdit(dialog);
    ASSERT_TRUE(one_time && username);

    EXPECT_FALSE(one_time->isChecked());
    EXPECT_TRUE(username->isEnabled());
    EXPECT_EQ(dialog.userName(), "admin");
}

//--------------------------------------------------------------------------------------------------
// A one-time password is the host naming itself, so a connection made with it carries no user
// name. The field keeps what was typed in it, because the pair it belongs to can be chosen again.
TEST_F(AuthorizationDialogTest, TurningOnTheOneTimePasswordDropsTheUserName)
{
    rememberOneTimePassword(false);

    AuthorizationDialog dialog;
    dialog.setOneTimePasswordEnabled(true);
    dialog.setUserName("admin");
    dialog.show();

    QRadioButton* one_time = oneTimePasswordButton(dialog);
    QLineEdit* username = userNameEdit(dialog);
    ASSERT_TRUE(one_time && username);

    QTest::mouseClick(one_time, Qt::LeftButton);

    EXPECT_TRUE(one_time->isChecked());
    EXPECT_FALSE(username->isEnabled());
    EXPECT_EQ(username->text(), "admin");
    EXPECT_TRUE(dialog.userName().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A one-time password is asked of the host itself, and the name of the record has nothing to do
// with it. A name the field still holds is not the answer of the user: handed over, it would send
// the connection under a user the host does not know.
TEST_F(AuthorizationDialogTest, NameBehindTheHiddenFieldIsNotHandedOver)
{
    rememberOneTimePassword(true);

    // The choice is remembered, but the host of this record is reached without a router, so it
    // does not count yet and the name is asked for.
    AuthorizationDialog dialog;
    dialog.show();

    QLineEdit* username = userNameEdit(dialog);
    ASSERT_TRUE(username);

    QTest::keyClicks(username, "admin");
    ASSERT_EQ(dialog.userName(), "admin");

    // The record turns out to be reached through a router, and the remembered choice now counts.
    dialog.setOneTimePasswordEnabled(true);

    EXPECT_FALSE(username->isEnabled());
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
// The choice is kept between connections, and it is the one the user made. A record that carries
// a user name puts the dialog back on the pair for that dialog only.
TEST_F(AuthorizationDialogTest, SavedUserNameLeavesTheRememberedChoiceAlone)
{
    rememberOneTimePassword(true);

    {
        AuthorizationDialog dialog;
        dialog.setOneTimePasswordEnabled(true);
        dialog.setUserName("admin");
        dialog.show();

        QRadioButton* one_time = oneTimePasswordButton(dialog);
        ASSERT_TRUE(one_time);
        EXPECT_FALSE(one_time->isChecked());
    }

    Settings settings;
    EXPECT_TRUE(settings.isOneTimePasswordChecked());
}

//--------------------------------------------------------------------------------------------------
// What the user chose is what comes back the next time the dialog opens.
TEST_F(AuthorizationDialogTest, ChoiceOfTheUserIsRemembered)
{
    rememberOneTimePassword(false);

    {
        AuthorizationDialog dialog;
        dialog.setOneTimePasswordEnabled(true);
        dialog.show();

        QRadioButton* one_time = oneTimePasswordButton(dialog);
        ASSERT_TRUE(one_time);

        QTest::mouseClick(one_time, Qt::LeftButton);
        ASSERT_TRUE(one_time->isChecked());
    }

    Settings settings;
    EXPECT_TRUE(settings.isOneTimePasswordChecked());
}

//--------------------------------------------------------------------------------------------------
// A host is entered with the pair of the record the user picked, and the record itself is what a
// link to it is saved as.
TEST_F(AuthorizationDialogTest, RecordOfTheManagerIsHandedOverInPlaceOfTheFields)
{
    rememberOneTimePassword(false);

    AuthorizationDialog dialog;
    dialog.setSavedCredentials({ credential(1, "first", "first-user", "first-password"),
                            credential(2, "second", "second-user", "second-password") });
    dialog.show();

    QLineEdit* username = userNameEdit(dialog);
    QRadioButton* saved = savedCredentialsButton(dialog);
    QComboBox* combo = credentialCombo(dialog);
    ASSERT_TRUE(username && saved && combo);

    // What was typed before the record was chosen stays in the field it was typed in, and the
    // host is not entered with it any more.
    QTest::keyClicks(username, "typed");

    EXPECT_TRUE(saved->isVisible());
    saved->setChecked(true);
    combo->setCurrentIndex(combo->findData(QVariant::fromValue<qint64>(2)));

    EXPECT_FALSE(username->isEnabled());
    EXPECT_EQ(dialog.userName(), "second-user");
    EXPECT_EQ(dialog.password().toString(), "second-password");
    EXPECT_EQ(dialog.credentialId(), 2);
}

//--------------------------------------------------------------------------------------------------
// With the choice left on the pair, it is the one typed here, and there is no link to save.
TEST_F(AuthorizationDialogTest, PairTypedByHandLeavesNoLinkBehind)
{
    rememberOneTimePassword(false);

    AuthorizationDialog dialog;
    dialog.setSavedCredentials({ credential(1, "first", "first-user", "first-password") });
    dialog.show();

    QLineEdit* username = userNameEdit(dialog);
    ASSERT_TRUE(username);

    QTest::keyClicks(username, "admin");

    EXPECT_TRUE(username->isEnabled());
    EXPECT_EQ(dialog.userName(), "admin");
    EXPECT_EQ(dialog.credentialId(), 0);
}

//--------------------------------------------------------------------------------------------------
// An address book that holds no record has nothing to offer, and the choice cannot be made.
TEST_F(AuthorizationDialogTest, ChoiceIsNotOfferedWithoutARecordToChoose)
{
    rememberOneTimePassword(false);

    AuthorizationDialog dialog;
    dialog.show();

    QRadioButton* saved = savedCredentialsButton(dialog);
    ASSERT_TRUE(saved);

    saved->setChecked(true);

    EXPECT_FALSE(saved->isVisible());
    EXPECT_EQ(dialog.credentialId(), 0);
}

//--------------------------------------------------------------------------------------------------
// A record that did not open hands over an empty pair. Offered, it would enter the host with
// nothing and leave behind a link to credentials the user was never shown.
TEST_F(AuthorizationDialogTest, RecordThatDidNotOpenIsNotOffered)
{
    rememberOneTimePassword(false);

    AuthorizationDialog dialog;
    dialog.setSavedCredentials({ credential(1, "office", QString(), QString()),
                                 credential(2, "home", "home-user", "home-password") });
    dialog.show();

    QRadioButton* saved = savedCredentialsButton(dialog);
    QComboBox* combo = credentialCombo(dialog);
    ASSERT_TRUE(saved && combo);

    EXPECT_TRUE(saved->isVisible());
    saved->setChecked(true);

    ASSERT_EQ(combo->count(), 1);
    EXPECT_EQ(combo->itemData(0).toLongLong(), 2);
    EXPECT_EQ(dialog.credentialId(), 2);
    EXPECT_EQ(dialog.userName(), "home-user");
}

//--------------------------------------------------------------------------------------------------
// With nothing else in the book, the choice is not offered at all.
TEST_F(AuthorizationDialogTest, ChoiceIsNotOfferedWhenTheOnlyRecordDidNotOpen)
{
    rememberOneTimePassword(false);

    AuthorizationDialog dialog;
    dialog.setSavedCredentials({ credential(1, "office", QString(), QString()) });
    dialog.show();

    QRadioButton* saved = savedCredentialsButton(dialog);
    ASSERT_TRUE(saved);

    saved->setChecked(true);

    EXPECT_FALSE(saved->isVisible());
    EXPECT_EQ(dialog.credentialId(), 0);
}

//--------------------------------------------------------------------------------------------------
// A one-time password is the host naming itself: the record is still there to be chosen, but it
// is not what the host is entered with while the password is.
TEST_F(AuthorizationDialogTest, OneTimePasswordLeavesTheRecordUnchosen)
{
    rememberOneTimePassword(true);

    AuthorizationDialog dialog;
    dialog.setSavedCredentials({ credential(1, "first", "first-user", "first-password") });
    dialog.setOneTimePasswordEnabled(true);
    dialog.show();

    QRadioButton* saved = savedCredentialsButton(dialog);
    QRadioButton* one_time = oneTimePasswordButton(dialog);
    ASSERT_TRUE(saved && one_time);

    EXPECT_TRUE(saved->isVisible());
    EXPECT_TRUE(one_time->isChecked());
    EXPECT_FALSE(saved->isChecked());
    EXPECT_TRUE(dialog.userName().isEmpty());
    EXPECT_EQ(dialog.credentialId(), 0);
}
