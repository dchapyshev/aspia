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

#include "client/desktop/credentials/credential_dialog.h"

#include <QAbstractButton>
#include <QIcon>
#include <QLayout>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>

#include "base/logging.h"
#include "base/os_account.h"
#include "base/peer/user.h"
#include "client/database.h"
#include "client/desktop/credentials/ui_credential_dialog.h"
#include "common/desktop/msg_box.h"

//--------------------------------------------------------------------------------------------------
CredentialDialog::CredentialDialog(qint64 credential_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::CredentialDialog>()),
      credential_id_(credential_id)
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);

    ui->combo_type->addItem(QIcon(":/img/key-aspia.svg"), tr("Aspia Credentials"),
                            static_cast<int>(CredentialConfig::Type::ASPIA));
    ui->combo_type->addItem(QIcon(":/img/key-os.svg"), tr("OS Credentials"),
                            static_cast<int>(CredentialConfig::Type::OS));

    ui->edit_password->setShowPasswordButtonVisible(true);

    const QString row(
        "<tr><td valign=\"top\">&#8226;</td><td style=\"padding-left: 6px; padding-bottom: 3px;\">%1</td></tr>");

    ui->label_hint->setText("<table cellspacing=\"0\" cellpadding=\"0\">" +
        row.arg(tr("Enter the user name, for example %1. On a computer in a domain, this is a domain account.")
                    .toHtmlEscaped().arg("<b>john</b>")) +
        row.arg(tr("For an account of another domain, add the domain, for example %1 or %2.")
                    .toHtmlEscaped().arg("<b>CORP\\john</b>", "<b>john@corp.com</b>")) +
        row.arg(tr("For a local account on a computer in a domain, add %1 before the name, for example %2.")
                    .toHtmlEscaped().arg("<b>.\\</b>", "<b>.\\john</b>")) +
        "</table>");

    setWindowTitle(credential_id_ != -1 ? tr("Edit Credentials") : tr("Add Credentials"));

    onTypeChanged();

    connect(ui->combo_type, &QComboBox::currentIndexChanged, this, &CredentialDialog::onTypeChanged);
    connect(ui->button_box, &QDialogButtonBox::clicked, this, &CredentialDialog::onButtonBoxClicked);
    ui->edit_name->setFocus();

    QTimer::singleShot(MilliSeconds::zero(), this, &CredentialDialog::onLoadData);
}

//--------------------------------------------------------------------------------------------------
CredentialDialog::~CredentialDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool CredentialDialog::hasHeightForWidth() const
{
    // The height follows the width in resizeEvent(). With height-for-width reported, Windows does not
    // let the user narrow the dialog below the width at which the hint fits into the fixed height.
    return false;
}

//--------------------------------------------------------------------------------------------------
void CredentialDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);

    if (event->size().width() != event->oldSize().width())
        fitHeight();
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

    const CredentialConfig::Type type =
        static_cast<CredentialConfig::Type>(ui->combo_type->currentData().toInt());

    if (type == CredentialConfig::Type::OS)
    {
        if (!OsAccount::isValidUserName(ui->edit_username->text()))
        {
            MsgBox::warning(this, tr("Invalid user name."));
            ui->edit_username->setFocus();
            ui->edit_username->selectAll();
            return;
        }
    }
    else if (!User::isValidUserName(ui->edit_username->text()))
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
    credential.setType(type);
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

//--------------------------------------------------------------------------------------------------
void CredentialDialog::onTypeChanged()
{
    const CredentialConfig::Type type =
        static_cast<CredentialConfig::Type>(ui->combo_type->currentData().toInt());

    ui->label_hint->setVisible(type == CredentialConfig::Type::OS);
    fitHeight();
}

//--------------------------------------------------------------------------------------------------
void CredentialDialog::onLoadData()
{
    if (credential_id_ == -1)
        return;

    CredentialConfig credential;
    const Database::FindResult found = Database::instance().findCredential(credential_id_, &credential);
    if (found == Database::FindResult::NOT_FOUND || found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to find credentials with id" << credential_id_;
        MsgBox::warning(this, tr("Failed to read the credentials."));
        reject();
        return;
    }

    ui->combo_type->setCurrentIndex(ui->combo_type->findData(static_cast<int>(credential.type())));
    ui->combo_type->setEnabled(false);
    ui->edit_name->setText(credential.displayName());
    ui->edit_username->setText(credential.username());
    ui->edit_password->setPassword(credential.password());

    if (found == Database::FindResult::UNREADABLE)
    {
        LOG(ERROR) << "Data of credentials" << credential_id_ << "could not be read";
        MsgBox::warning(this, tr("The credentials are damaged. Enter them again."));
    }
}

//--------------------------------------------------------------------------------------------------
void CredentialDialog::fitHeight()
{
    // The wrapped hint needs more lines in a narrower dialog. QWidget::heightForWidth() never goes
    // below the current minimum height, so the layout is asked directly.
    QLayout* dialog_layout = layout();
    setFixedHeight(dialog_layout->hasHeightForWidth() ?
        dialog_layout->totalHeightForWidth(width()) : sizeHint().height());
}
