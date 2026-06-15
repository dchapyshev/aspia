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

#include "client/android/password_dialog.h"

#include <QVBoxLayout>

#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"

//--------------------------------------------------------------------------------------------------
PasswordDialog::PasswordDialog(Mode mode, QWidget* parent)
    : Dialog(parent),
      mode_(mode),
      password_(new LineEdit(this)),
      error_(new Label(QString(), Label::Role::CAPTION, this))
{
    if (mode_ == Mode::SET)
    {
        setTitle(tr("Set Password"));
        setText(tr("Enter a password to encrypt the address book."));
    }
    else
    {
        setTitle(tr("Enter Password"));
        setText(tr("Enter the password to decrypt the address book."));
    }

    password_->setLabel(tr("Password"));
    password_->setEchoMode(QLineEdit::Password);

    error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    error_->setWordWrap(true);
    error_->setVisible(false);

    contentLayout()->addWidget(error_);
    contentLayout()->addWidget(password_);

    if (mode_ == Mode::SET)
    {
        confirm_ = new LineEdit(this);
        confirm_->setLabel(tr("Confirm Password"));
        confirm_->setEchoMode(QLineEdit::Password);
        contentLayout()->addWidget(confirm_);
    }

    Button* cancel = addButton(tr("Cancel"), Button::Role::TEXT);
    Button* accept = addButton(tr("OK"), Button::Role::FILLED);

    connect(cancel, &Button::clicked, this, &PasswordDialog::reject);
    connect(accept, &Button::clicked, this, &PasswordDialog::onAcceptClicked);
}

//--------------------------------------------------------------------------------------------------
PasswordDialog::~PasswordDialog() = default;

//--------------------------------------------------------------------------------------------------
SecureString PasswordDialog::password() const
{
    return SecureString(password_->text());
}

//--------------------------------------------------------------------------------------------------
void PasswordDialog::onAcceptClicked()
{
    if (password_->text().isEmpty())
    {
        showError(tr("Password cannot be empty."));
        password_->setFocus();
        return;
    }

    if (mode_ == Mode::SET && password_->text() != confirm_->text())
    {
        showError(tr("Passwords do not match."));
        confirm_->setFocus();
        confirm_->selectAll();
        return;
    }

    accept();
}

//--------------------------------------------------------------------------------------------------
void PasswordDialog::showError(const QString& message)
{
    error_->setText(message);
    error_->setVisible(true);
}
