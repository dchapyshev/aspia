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

#include "client/desktop/credential_dialog.h"

#include <QAbstractButton>
#include <QPushButton>

#include "base/logging.h"
#include "base/peer/user.h"
#include "client/database.h"
#include "common/desktop/msg_box.h"
#include "ui_credential_dialog.h"

//--------------------------------------------------------------------------------------------------
CredentialDialog::CredentialDialog(qint64 credential_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::CredentialDialog>()),
      credential_id_(credential_id)
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);

    ui->edit_password->setShowPasswordButtonVisible(true);

    if (credential_id_ != -1)
    {
        setWindowTitle(tr("Edit Credentials"));

        std::optional<CredentialConfig> credential =
            Database::instance().findCredential(credential_id_);
        if (credential.has_value())
        {
            ui->edit_name->setText(credential->displayName());
            ui->edit_username->setText(credential->username());
            ui->edit_password->setPassword(credential->password());
        }
        else
        {
            LOG(ERROR) << "Unable to find credentials with id" << credential_id_;
        }
    }
    else
    {
        setWindowTitle(tr("Add Credentials"));
    }

    connect(ui->button_box, &QDialogButtonBox::clicked, this, &CredentialDialog::onButtonBoxClicked);
    ui->edit_name->setFocus();
}

//--------------------------------------------------------------------------------------------------
CredentialDialog::~CredentialDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void CredentialDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->button_box->standardButton(button) != QDialogButtonBox::Ok)
    {
        reject();
        return;
    }

    const QString name = ui->edit_name->text();
    if (name.isEmpty())
    {
        MsgBox::warning(this, tr("Name cannot be empty."));
        ui->edit_name->setFocus();
        return;
    }

    if (name.length() > CredentialConfig::kMaxNameLength)
    {
        MsgBox::warning(this,
            tr("Too long name. The maximum length of the name is %n characters.",
               "", CredentialConfig::kMaxNameLength));
        ui->edit_name->setFocus();
        ui->edit_name->selectAll();
        return;
    }

    if (!User::isValidUserName(ui->edit_username->text()))
    {
        MsgBox::warning(this,
            tr("The user name can not be empty and can contain only"
               " alphabet characters, numbers and \"_\", \"-\", \".\" characters."));
        ui->edit_username->setFocus();
        ui->edit_username->selectAll();
        return;
    }

    if (ui->edit_password->password().isEmpty())
    {
        MsgBox::warning(this, tr("Password cannot be empty."));
        ui->edit_password->setFocus();
        return;
    }

    CredentialConfig credential;
    credential.setId(credential_id_);
    credential.setType(CredentialConfig::Type::HOST);
    credential.setDisplayName(name);
    credential.setUsername(ui->edit_username->text());
    credential.setPassword(ui->edit_password->password());

    Database& db = Database::instance();

    if (credential_id_ == -1)
    {
        if (!db.addCredential(credential))
        {
            MsgBox::warning(this, tr("Unable to add credentials"));
            LOG(INFO) << "Unable to add credentials to database";
            return;
        }

        credential_id_ = credential.id();
    }
    else
    {
        if (!db.modifyCredential(credential))
        {
            MsgBox::warning(this, tr("Unable to modify credentials"));
            LOG(INFO) << "Unable to modify credentials in database";
            return;
        }
    }

    accept();
}
