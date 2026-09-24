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

#include <QTimer>
#include <QVBoxLayout>

#include "base/time_types.h"
#include "base/crypto/secure_string.h"
#include "client/database.h"
#include "client/master_password.h"
#include "client/android/biometric_gate.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"

//--------------------------------------------------------------------------------------------------
MasterPasswordDialog::MasterPasswordDialog(Mode mode, QWidget* parent)
    : Dialog(parent),
      edit_current_(nullptr),
      edit_password_(new LineEdit(this)),
      edit_confirm_(nullptr),
      label_error_(new Label(QString(), Label::Role::CAPTION, this)),
      mode_(mode)
{
    const bool create = (mode_ == Mode::CREATE);
    const bool change = (mode_ == Mode::CHANGE);

    setBlurBehind(true);

    switch (mode_)
    {
        case Mode::CREATE:
            setTitle(tr("Set Master Password"));
            setText(tr("Set a master password required to unlock the application."));
            break;

        case Mode::UNLOCK:
            setTitle(tr("Unlock"));
            setText(tr("Enter the master password."));
            break;

        case Mode::CHANGE:
            setTitle(tr("Change Master Password"));
            setText(tr("Enter your current password and choose a new one."));
            break;
    }

    edit_password_->setLabel(change ? tr("New Password") : tr("Password"));
    edit_password_->setEchoMode(QLineEdit::Password);

    // A fixed hex keeps the error color readable on both light and dark cards and survives the
    // palette reset that the caption role applies on theme changes.
    label_error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    label_error_->setWordWrap(true);
    label_error_->setVisible(false);

    QVBoxLayout* content = contentLayout();
    content->addWidget(label_error_);

    if (change)
    {
        edit_current_ = new LineEdit(this);
        edit_current_->setLabel(tr("Current Password"));
        edit_current_->setEchoMode(QLineEdit::Password);
        content->addWidget(edit_current_);
    }

    content->addWidget(edit_password_);

    if (create || change)
    {
        edit_confirm_ = new LineEdit(this);
        edit_confirm_->setLabel(tr("Confirm Password"));
        edit_confirm_->setEchoMode(QLineEdit::Password);
        content->addWidget(edit_confirm_);
    }

    Button* cancel = addButton(tr("Cancel"), Button::Role::TEXT);
    Button* accept = addButton(change ? tr("Change") : tr("OK"), Button::Role::FILLED);

    connect(cancel, &Button::clicked, this, &MasterPasswordDialog::reject);
    connect(accept, &Button::clicked, this, &MasterPasswordDialog::onAccept);

    // The system already shows a biometric prompt automatically when unlock is available; the
    // password entry stays as the fallback if the user dismisses it.
    const bool biometric_available = mode_ == Mode::UNLOCK &&
        Database::instance().isBiometricUnlockEnabled() &&
        BiometricGate::status() == BiometricGate::Status::AVAILABLE;
    if (biometric_available)
    {
        QTimer::singleShot(MilliSeconds(0), this, &MasterPasswordDialog::tryBiometricUnlock);
    }
    else
    {
        // Typing can start right away. With the biometric prompt the focus stays away so the
        // keyboard does not pop under the system sheet.
        (change ? edit_current_ : edit_password_)->setFocus();
    }
}

//--------------------------------------------------------------------------------------------------
MasterPasswordDialog::~MasterPasswordDialog() = default;

//--------------------------------------------------------------------------------------------------
void MasterPasswordDialog::onAccept()
{
    if (mode_ == Mode::UNLOCK)
    {
        switch (MasterPassword::unlock(SecureString(edit_password_->text())))
        {
            case MasterPassword::Result::SUCCESS:
                break;

            case MasterPassword::Result::INVALID_PASSWORD:
                showError(tr("Invalid master password."));
                edit_password_->setFocus();
                edit_password_->selectAll();
                return;

            default:
                showError(tr("Unable to unlock the database."));
                return;
        }

        accept();
        return;
    }

    // CREATE and CHANGE both set a new password and require a matching confirmation.
    if (edit_password_->text().length() < MasterPassword::kMinPasswordLength)
    {
        showError(tr("The password can not be shorter than %n characters.",
                     "", MasterPassword::kMinPasswordLength));
        edit_password_->setFocus();
        return;
    }

    if (edit_password_->text() != edit_confirm_->text())
    {
        showError(tr("The entered passwords do not match."));
        edit_confirm_->setFocus();
        edit_confirm_->selectAll();
        return;
    }

    if (mode_ == Mode::CREATE)
    {
        if (MasterPassword::setNew(SecureString(edit_password_->text())) != MasterPassword::Result::SUCCESS)
        {
            showError(tr("Unable to set master password."));
            return;
        }
    }
    else
    {
        switch (MasterPassword::change(SecureString(edit_current_->text()),
                                       SecureString(edit_password_->text())))
        {
            case MasterPassword::Result::SUCCESS:
                break;

            case MasterPassword::Result::INVALID_PASSWORD:
                showError(tr("Invalid current password."));
                edit_current_->setFocus();
                edit_current_->selectAll();
                return;

            case MasterPassword::Result::UNREADABLE_RECORD:
                showError(tr("Some records of the database are damaged. Fix or delete them and try again."));
                return;

            default:
                showError(tr("Unable to change the password."));
                return;
        }
    }

    // The master key changed, so any biometric enrollment bound to the old key no longer works.
    Database::instance().clearBiometricUnlock();
    BiometricGate::deleteKey();

    accept();
}

//--------------------------------------------------------------------------------------------------
void MasterPasswordDialog::tryBiometricUnlock()
{
    Database& db = Database::instance();

    std::optional<BiometricGate::Blob> blob = BiometricGate::unpack(db.biometricBlob());
    if (!blob.has_value())
        return;

    BiometricGate::Prompt prompt;
    prompt.title = tr("Unlock");
    prompt.negative_text = tr("Use password");

    // unlock() spins a nested event loop while the system prompt is shown; without this the dialog
    // buttons stay clickable and accept() could be re-entered in the middle of the prompt.
    setEnabled(false);
    BiometricGate::UnlockResult result = BiometricGate::unlock(*blob, prompt);
    setEnabled(true);

    switch (result.result)
    {
        case BiometricGate::Result::SUCCESS:
            if (MasterPassword::unlockWithKey(result.key))
            {
                accept();
                return;
            }
            // The blob decrypted but did not match the stored verifier; the enrollment is broken.
            dropBiometric(tr("Biometric unlock failed. Enter the master password."));
            break;

        case BiometricGate::Result::KEY_INVALIDATED:
            dropBiometric(tr("Biometrics changed. Enter the master password to continue."));
            break;

        case BiometricGate::Result::FAILED:
            showError(tr("Biometric unlock failed. Enter the master password."));
            break;

        case BiometricGate::Result::CANCELED:
            // The user chose to enter the password instead; stay silent.
            break;
    }

    // Every path past the switch falls back to manual entry.
    edit_password_->setFocus();
}

//--------------------------------------------------------------------------------------------------
void MasterPasswordDialog::showError(const QString& message)
{
    label_error_->setText(message);
    label_error_->setVisible(true);
}

//--------------------------------------------------------------------------------------------------
void MasterPasswordDialog::dropBiometric(const QString& message)
{
    Database::instance().clearBiometricUnlock();
    BiometricGate::deleteKey();

    showError(message);
}
