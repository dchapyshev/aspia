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

#include "client/android/authorization_dialog.h"

#include <QVBoxLayout>

#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/switch.h"

//--------------------------------------------------------------------------------------------------
AuthorizationDialog::AuthorizationDialog(bool one_time_password_available,
                                         bool save_credentials_available, QWidget* parent)
    : Dialog(parent),
      username_(new LineEdit(this)),
      password_(new LineEdit(this)),
      error_(new Label(QString(), Label::Role::CAPTION, this))
{
    setTitle(tr("Authorization"));
    setText(tr("Enter the credentials to connect to the host."));

    error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    error_->setWordWrap(true);
    error_->setVisible(false);

    username_->setLabel(tr("Username"));

    password_->setLabel(tr("Password"));
    password_->setEchoMode(QLineEdit::Password);

    contentLayout()->addWidget(error_);

    if (one_time_password_available)
    {
        one_time_password_ = new Switch(tr("One-time password"), this);
        one_time_password_->setChecked(true);
        connect(one_time_password_, &Switch::toggled,
                this, &AuthorizationDialog::onOneTimePasswordToggled);
        contentLayout()->addWidget(one_time_password_);
    }

    contentLayout()->addWidget(username_);
    contentLayout()->addWidget(password_);

    if (save_credentials_available)
    {
        save_credentials_ = new Switch(tr("Save credentials"), this);
        save_credentials_->setChecked(true);
        contentLayout()->addWidget(save_credentials_);
    }

    // The one-time password switch defaults to on, so the user name field starts hidden and there
    // is nothing to keep: such a password is good for one connection.
    const bool one_time = one_time_password_ && one_time_password_->isChecked();

    username_->setVisible(!one_time);
    if (save_credentials_)
        save_credentials_->setVisible(!one_time);

    Button* cancel = addButton(tr("Cancel"), Button::Role::TEXT);
    Button* accept = addButton(tr("Connect"), Button::Role::FILLED);

    connect(cancel, &Button::clicked, this, &AuthorizationDialog::reject);
    connect(accept, &Button::clicked, this, &AuthorizationDialog::onAcceptClicked);
}

//--------------------------------------------------------------------------------------------------
AuthorizationDialog::~AuthorizationDialog() = default;

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::setUserName(const QString& username)
{
    username_->setText(username);

    // A saved user name means a named-user connection, so the one-time password path does not apply.
    if (!username.isEmpty() && one_time_password_)
        one_time_password_->setChecked(false);
}

//--------------------------------------------------------------------------------------------------
QString AuthorizationDialog::userName() const
{
    if (one_time_password_ && one_time_password_->isChecked())
        return QString();
    return username_->text();
}

//--------------------------------------------------------------------------------------------------
SecureString AuthorizationDialog::password() const
{
    return SecureString(password_->text());
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationDialog::isSaveCredentialsChecked() const
{
    // The switch is hidden while a one-time password is asked for, and what the user was never
    // shown is not his answer.
    const bool one_time = one_time_password_ && one_time_password_->isChecked();
    return save_credentials_ && !one_time && save_credentials_->isChecked();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::onAcceptClicked()
{
    const bool one_time = one_time_password_ && one_time_password_->isChecked();

    if (!one_time && username_->text().isEmpty())
    {
        showError(tr("Username cannot be empty."));
        username_->setFocus();
        return;
    }

    if (password_->text().isEmpty())
    {
        showError(tr("Password cannot be empty."));
        password_->setFocus();
        return;
    }

    accept();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::onOneTimePasswordToggled(bool checked)
{
    username_->setVisible(!checked);
    if (checked)
        username_->clear();

    if (save_credentials_)
        save_credentials_->setVisible(!checked);
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::showError(const QString& message)
{
    error_->setText(message);
    error_->setVisible(true);
}
