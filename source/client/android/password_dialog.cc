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
      edit_password_(new LineEdit(this)),
      label_error_(new Label(QString(), Label::Role::CAPTION, this))
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

    edit_password_->setLabel(tr("Password"));
    edit_password_->setEchoMode(QLineEdit::Password);

    label_error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    label_error_->setWordWrap(true);
    label_error_->setVisible(false);

    contentLayout()->addWidget(label_error_);
    contentLayout()->addWidget(edit_password_);

    if (mode_ == Mode::SET)
    {
        edit_confirm_ = new LineEdit(this);
        edit_confirm_->setLabel(tr("Confirm Password"));
        edit_confirm_->setEchoMode(QLineEdit::Password);
        contentLayout()->addWidget(edit_confirm_);
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
    return SecureString(edit_password_->text());
}

//--------------------------------------------------------------------------------------------------
void PasswordDialog::onAcceptClicked()
{
    if (edit_password_->text().isEmpty())
    {
        showError(tr("Password cannot be empty."));
        edit_password_->setFocus();
        return;
    }

    if (mode_ == Mode::SET && edit_password_->text() != edit_confirm_->text())
    {
        showError(tr("Passwords do not match."));
        edit_confirm_->setFocus();
        edit_confirm_->selectAll();
        return;
    }

    accept();
}

//--------------------------------------------------------------------------------------------------
void PasswordDialog::showError(const QString& message)
{
    label_error_->setText(message);
    label_error_->setVisible(true);
}
