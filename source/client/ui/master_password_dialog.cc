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

#include "client/ui/master_password_dialog.h"

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "client/master_password.h"
#include "common/ui/msg_box.h"
#include "ui_master_password_dialog.h"

#include <QAbstractButton>
#include <QPushButton>
#include <QTimer>

//--------------------------------------------------------------------------------------------------
MasterPasswordDialog::MasterPasswordDialog(Mode mode, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::MasterPasswordDialog>()),
      mode_(mode)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    switch (mode_)
    {
        case Mode::SET:
            setWindowTitle(tr("Set Master Password"));
            ui->label_description->setText(tr("Set the master password to encrypt the address book."));
            ui->label_current->setVisible(false);
            ui->edit_current->setVisible(false);
            ui->edit_new->setFocus();
            break;

        case Mode::CHANGE:
            setWindowTitle(tr("Change Master Password"));
            ui->label_description->setText(tr("Enter your current password and choose a new one."));
            ui->edit_current->setFocus();
            break;
    }

    connect(ui->button_box, &QDialogButtonBox::clicked, this, &MasterPasswordDialog::onButtonBoxClicked);

    QTimer::singleShot(0, this, [this](){ setFixedHeight(sizeHint().height()); });
}

//--------------------------------------------------------------------------------------------------
MasterPasswordDialog::~MasterPasswordDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void MasterPasswordDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->button_box->standardButton(button) != QDialogButtonBox::Ok)
    {
        reject();
        return;
    }

    bool ok = false;

    switch (mode_)
    {
        case Mode::SET:
            ok = applySet();
            break;
        case Mode::CHANGE:
            ok = applyChange();
            break;
    }

    if (ok)
        accept();
}

//--------------------------------------------------------------------------------------------------
bool MasterPasswordDialog::applySet()
{
    SecureString new_password(ui->edit_new->text());
    SecureString confirm(ui->edit_confirm->text());

    if (!MasterPassword::isSafePassword(new_password))
    {
        QString unsafe = tr("Password you entered does not meet the security requirements!");
        QString safe = tr("The password must contain lowercase and uppercase characters, "
                          "numbers and should not be shorter than %n characters.",
                          "", MasterPassword::kSafePasswordLength);
        QString question = tr("Do you want to enter a different password?");

        MsgBox message_box(MsgBox::Warning,
                                   tr("Warning"),
                                   QString("<b>%1</b><br/>%2<br/>%3").arg(unsafe, safe, question),
                                   MsgBox::Yes | MsgBox::No,
                                   this);
        if (message_box.exec() == MsgBox::Yes)
            return false;
    }

    if (new_password != confirm)
    {
        MsgBox::warning(this, tr("Passwords do not match."));
        return false;
    }

    if (!MasterPassword::setNew(new_password))
    {
        MsgBox::warning(this, tr("Unable to set master password."));
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool MasterPasswordDialog::applyChange()
{
    SecureString current(ui->edit_current->text());
    SecureString new_password(ui->edit_new->text());
    SecureString confirm(ui->edit_confirm->text());

    if (current.isEmpty())
    {
        MsgBox::warning(this, tr("Enter the current password."));
        return false;
    }

    if (!MasterPassword::isSafePassword(new_password))
    {
        QString unsafe = tr("Password you entered does not meet the security requirements!");
        QString safe = tr("The password must contain lowercase and uppercase characters, "
                          "numbers and should not be shorter than %n characters.",
                          "", MasterPassword::kSafePasswordLength);
        QString question = tr("Do you want to enter a different password?");

        MsgBox message_box(MsgBox::Warning,
                                   tr("Warning"),
                                   QString("<b>%1</b><br/>%2<br/>%3").arg(unsafe, safe, question),
                                   MsgBox::Yes | MsgBox::No,
                                   this);
        if (message_box.exec() == MsgBox::Yes)
            return false;
    }

    if (new_password != confirm)
    {
        MsgBox::warning(this, tr("New passwords do not match."));
        return false;
    }

    if (!MasterPassword::change(current, new_password))
    {
        MsgBox::warning(this, tr("Invalid current password or unable to change it."));
        return false;
    }

    return true;
}
