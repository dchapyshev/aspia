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

#include "client/android/master_password_dialog.h"

#include <QVBoxLayout>

#include "base/crypto/secure_string.h"
#include "client/master_password.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"

//--------------------------------------------------------------------------------------------------
MasterPasswordDialog::MasterPasswordDialog(Mode mode, QWidget* parent)
    : Dialog(parent),
      current_(nullptr),
      password_(new LineEdit(this)),
      confirm_(nullptr),
      error_(new Label(QString(), Label::Role::CAPTION, this)),
      mode_(mode)
{
    const bool create = (mode_ == Mode::CREATE);
    const bool change = (mode_ == Mode::CHANGE);

    switch (mode_)
    {
        case Mode::CREATE:
            setTitle(tr("Set Master Password"));
            setText(tr("Set a master password required to unlock the application."));
            break;

        case Mode::UNLOCK:
            setTitle(tr("Unlock"));
            setText(tr("Enter the master password to unlock the application."));
            break;

        case Mode::CHANGE:
            setTitle(tr("Change Master Password"));
            setText(tr("Enter your current password and choose a new one."));
            break;
    }

    password_->setLabel(change ? tr("New Password") : tr("Password"));
    password_->setEchoMode(QLineEdit::Password);

    // A fixed hex keeps the error color readable on both light and dark cards and survives the
    // palette reset that the caption role applies on theme changes.
    error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    error_->setWordWrap(true);
    error_->setVisible(false);

    QVBoxLayout* content = contentLayout();
    content->addWidget(error_);

    if (change)
    {
        current_ = new LineEdit(this);
        current_->setLabel(tr("Current Password"));
        current_->setEchoMode(QLineEdit::Password);
        content->addWidget(current_);
    }

    content->addWidget(password_);

    if (create || change)
    {
        confirm_ = new LineEdit(this);
        confirm_->setLabel(tr("Confirm Password"));
        confirm_->setEchoMode(QLineEdit::Password);
        content->addWidget(confirm_);
    }

    addButton(tr("Cancel"), Button::Role::TEXT);
    Button* accept = addButton(change ? tr("Change") : (create ? tr("OK") : tr("Unlock")),
                               Button::Role::FILLED);

    connect(accept, &Button::clicked, this, &MasterPasswordDialog::onAccept);
}

//--------------------------------------------------------------------------------------------------
MasterPasswordDialog::~MasterPasswordDialog() = default;

//--------------------------------------------------------------------------------------------------
void MasterPasswordDialog::onAccept()
{
    if (mode_ == Mode::UNLOCK)
    {
        if (!MasterPassword::unlock(SecureString(password_->text())))
        {
            showError(tr("Invalid master password."));
            password_->setFocus();
            password_->selectAll();
            return;
        }

        accept();
        return;
    }

    // CREATE and CHANGE both set a new password and require a matching confirmation.
    if (password_->text().isEmpty())
    {
        showError(tr("Password cannot be empty."));
        password_->setFocus();
        return;
    }

    if (password_->text() != confirm_->text())
    {
        showError(tr("The entered passwords do not match."));
        confirm_->setFocus();
        confirm_->selectAll();
        return;
    }

    if (mode_ == Mode::CREATE)
    {
        if (!MasterPassword::setNew(SecureString(password_->text())))
        {
            showError(tr("Unable to set master password."));
            return;
        }
    }
    else
    {
        if (!MasterPassword::change(SecureString(current_->text()),
                                    SecureString(password_->text())))
        {
            showError(tr("Invalid current password or unable to change it."));
            current_->setFocus();
            current_->selectAll();
            return;
        }
    }

    accept();
}

//--------------------------------------------------------------------------------------------------
void MasterPasswordDialog::showError(const QString& message)
{
    error_->setText(message);
    error_->setVisible(true);
}
