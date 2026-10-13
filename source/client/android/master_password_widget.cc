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

#include "client/android/master_password_widget.h"

#include <QVBoxLayout>

#include "base/crypto/secure_string.h"
#include "client/database.h"
#include "client/master_password.h"
#include "client/android/biometric_gate.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/scroll_area.h"

namespace {

constexpr int kFormMargin = 16;
constexpr int kFormSpacing = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
MasterPasswordWidget::MasterPasswordWidget(QWidget* parent)
    : QWidget(parent),
      edit_current_(new LineEdit()),
      edit_password_(new LineEdit()),
      edit_confirm_(new LineEdit()),
      label_error_(new Label(QString(), Label::Role::CAPTION))
{
    Label* label_description = new Label(tr("Enter your current password and choose a new one."));
    label_description->setWordWrap(true);

    edit_current_->setLabel(tr("Current Password"));
    edit_current_->setEchoMode(QLineEdit::Password);
    edit_password_->setLabel(tr("New Password"));
    edit_password_->setEchoMode(QLineEdit::Password);
    edit_confirm_->setLabel(tr("Confirm Password"));
    edit_confirm_->setEchoMode(QLineEdit::Password);

    label_error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    label_error_->setWordWrap(true);
    label_error_->setVisible(false);

    Button* change = new Button(tr("Change"), Button::Role::FILLED);

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(label_description);
    form_layout->addWidget(label_error_);
    form_layout->addWidget(edit_current_);
    form_layout->addWidget(edit_password_);
    form_layout->addWidget(edit_confirm_);
    form_layout->addWidget(change);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(edit_current_, &LineEdit::returnPressed, this, &MasterPasswordWidget::onChangeClicked);
    connect(edit_password_, &LineEdit::returnPressed, this, &MasterPasswordWidget::onChangeClicked);
    connect(edit_confirm_, &LineEdit::returnPressed, this, &MasterPasswordWidget::onChangeClicked);
    connect(change, &Button::clicked, this, &MasterPasswordWidget::onChangeClicked);
}

//--------------------------------------------------------------------------------------------------
MasterPasswordWidget::~MasterPasswordWidget() = default;

//--------------------------------------------------------------------------------------------------
void MasterPasswordWidget::prepare()
{
    clear();
    edit_current_->setFocus();
}

//--------------------------------------------------------------------------------------------------
void MasterPasswordWidget::clear()
{
    edit_current_->clear();
    edit_password_->clear();
    edit_confirm_->clear();
    label_error_->setVisible(false);
}

//--------------------------------------------------------------------------------------------------
void MasterPasswordWidget::onChangeClicked()
{
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

    switch (MasterPassword::change(SecureString(edit_current_->text()), SecureString(edit_password_->text())))
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

    // The master key changed, so any biometric enrollment bound to the old key no longer works.
    Database::instance().clearBiometricUnlock();
    BiometricGate::deleteKey();

    clear();
    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void MasterPasswordWidget::showError(const QString& message)
{
    label_error_->setText(message);
    label_error_->setVisible(true);
}
