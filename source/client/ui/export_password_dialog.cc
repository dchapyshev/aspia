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

#include "client/ui/export_password_dialog.h"

#include <QPushButton>
#include <QTimer>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "client/master_password.h"
#include "common/ui/msg_box.h"
#include "common/ui/password_edit.h"
#include "ui_export_password_dialog.h"

//--------------------------------------------------------------------------------------------------
ExportPasswordDialog::ExportPasswordDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::ExportPasswordDialog>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    connect(ui->button_encrypt, &QPushButton::clicked,
            this, &ExportPasswordDialog::onEncryptClicked);
    connect(ui->button_cancel, &QPushButton::clicked, this, &QDialog::reject);

    ui->edit_password->setFocus();

    QTimer::singleShot(0, this, [this](){ setFixedHeight(sizeHint().height()); });
}

//--------------------------------------------------------------------------------------------------
ExportPasswordDialog::~ExportPasswordDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
SecureString ExportPasswordDialog::password() const
{
    return ui->edit_password->password();
}

//--------------------------------------------------------------------------------------------------
void ExportPasswordDialog::onEncryptClicked()
{
    SecureString password = ui->edit_password->password();
    if (password.isEmpty())
    {
        MsgBox::warning(this, tr("Password cannot be empty."));
        return;
    }

    SecureString confirm = ui->edit_confirm->password();
    if (password != confirm)
    {
        MsgBox::warning(this, tr("Passwords do not match."));
        return;
    }

    if (!MasterPassword::isSafePassword(password))
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
            return;
    }

    accept();
}
