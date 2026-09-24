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

#include "client/desktop/credentials/credential_export_dialog.h"

#include <QAbstractButton>
#include <QIcon>
#include <QPushButton>
#include <QTimer>

#include "base/logging.h"
#include "client/credential_file.h"
#include "client/database.h"
#include "client/master_password.h"
#include "client/desktop/file_dialog.h"
#include "client/desktop/credentials/ui_credential_export_dialog.h"
#include "common/desktop/msg_box.h"

namespace {

enum Column
{
    COLUMN_NAME = 0,
    COLUMN_USERNAME = 1
};

} // namespace

//--------------------------------------------------------------------------------------------------
CredentialExportDialog::CredentialExportDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::CredentialExportDialog>())
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);

    ui->edit_password->setShowPasswordButtonVisible(true);
    ui->button_box->button(QDialogButtonBox::Save)->setEnabled(false);

    connect(ui->button_check_all, &QPushButton::clicked,
            this, &CredentialExportDialog::onCheckAllButtonPressed);
    connect(ui->button_check_none, &QPushButton::clicked,
            this, &CredentialExportDialog::onCheckNoneButtonPressed);
    connect(ui->button_box, &QDialogButtonBox::clicked,
            this, &CredentialExportDialog::onButtonBoxClicked);

    connect(ui->tree_credentials, &QTreeWidget::itemChanged, this, [this]()
    {
        ui->button_box->button(QDialogButtonBox::Save)->setEnabled(!checkedCredentials().isEmpty());
    });

    QTimer::singleShot(MilliSeconds::zero(), this, &CredentialExportDialog::onLoadData);
}

//--------------------------------------------------------------------------------------------------
CredentialExportDialog::~CredentialExportDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void CredentialExportDialog::onCheckAllButtonPressed()
{
    LOG(INFO) << "[ACTION] Check all button pressed";
    setCheckState(Qt::Checked);
}

//--------------------------------------------------------------------------------------------------
void CredentialExportDialog::onCheckNoneButtonPressed()
{
    LOG(INFO) << "[ACTION] Check none button pressed";
    setCheckState(Qt::Unchecked);
}

//--------------------------------------------------------------------------------------------------
void CredentialExportDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->button_box->standardButton(button) != QDialogButtonBox::Save)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        reject();
        return;
    }

    const QList<CredentialConfig> credentials = checkedCredentials();
    if (credentials.isEmpty() || !checkPassword())
        return;

    const QString file_path = FileDialog::getSaveFileName(
        this,
        tr("Export Credentials"),
        tr("Aspia Credentials (*.aspia-credentials);;All files (*)"));

    if (file_path.isEmpty())
    {
        LOG(INFO) << "[ACTION] Cancelled by user";
        return;
    }

    switch (CredentialFile::exportToFile(credentials, file_path, ui->edit_password->password()))
    {
        case CredentialFile::Result::SUCCESS:
            break;

        case CredentialFile::Result::FILE_ERROR:
            MsgBox::warning(this, tr("Unable to write the file."));
            return;

        default:
            MsgBox::warning(this, tr("Failed to export the credentials."));
            return;
    }

    MsgBox::information(this, tr("Export completed successfully.\nCredentials exported: %1")
                                  .arg(credentials.size()));
    accept();
}

//--------------------------------------------------------------------------------------------------
void CredentialExportDialog::onLoadData()
{
    if (Database::instance().credentialList(&credentials_) == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the list of credentials";
        MsgBox::warning(this, tr("Failed to read the credentials."));
        reject();
        return;
    }

    for (int i = 0; i < credentials_.size(); ++i)
    {
        const CredentialConfig& credential = credentials_[i];

        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(COLUMN_NAME, credential.displayName());
        item->setText(COLUMN_USERNAME, credential.username());
        item->setData(COLUMN_NAME, Qt::UserRole, i);

        // A record whose data could not be read has nothing to export.
        if (credential.isValid())
        {
            item->setIcon(COLUMN_NAME, QIcon(":/img/keys.svg"));
            item->setCheckState(COLUMN_NAME, Qt::Unchecked);
        }
        else
        {
            item->setIcon(COLUMN_NAME, QIcon(":/img/key-corrupted.svg"));
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }

        ui->tree_credentials->addTopLevelItem(item);
    }

    ui->tree_credentials->sortItems(COLUMN_NAME, Qt::AscendingOrder);
    ui->tree_credentials->resizeColumnToContents(COLUMN_NAME);
}

//--------------------------------------------------------------------------------------------------
void CredentialExportDialog::setCheckState(Qt::CheckState state)
{
    for (int i = 0; i < ui->tree_credentials->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = ui->tree_credentials->topLevelItem(i);
        if (item->flags() & Qt::ItemIsEnabled)
            item->setCheckState(COLUMN_NAME, state);
    }
}

//--------------------------------------------------------------------------------------------------
QList<CredentialConfig> CredentialExportDialog::checkedCredentials() const
{
    QList<CredentialConfig> credentials;

    for (int i = 0; i < ui->tree_credentials->topLevelItemCount(); ++i)
    {
        const QTreeWidgetItem* item = ui->tree_credentials->topLevelItem(i);
        if (item->checkState(COLUMN_NAME) == Qt::Checked)
            credentials.append(credentials_[item->data(COLUMN_NAME, Qt::UserRole).toInt()]);
    }

    return credentials;
}

//--------------------------------------------------------------------------------------------------
bool CredentialExportDialog::checkPassword()
{
    const SecureString password = ui->edit_password->password();

    if (password.size() < MasterPassword::kMinPasswordLength)
    {
        MsgBox::warning(this, tr("The password can not be shorter than %n characters.",
                                 "", MasterPassword::kMinPasswordLength));
        ui->edit_password->setFocus();
        return false;
    }

    if (!MasterPassword::isSafePassword(password))
    {
        QString unsafe = tr("Password you entered does not meet the security requirements!");
        QString safe = tr("The password must contain lowercase and uppercase characters, "
                          "numbers and should not be shorter than %n characters.",
                          "", MasterPassword::kSafePasswordLength);
        QString question = tr("Do you want to enter a different password?");

        if (MsgBox::warning(this, QString("<b>%1</b><br/>%2<br/>%3").arg(unsafe, safe, question),
                            MsgBox::Yes | MsgBox::No) == MsgBox::Yes)
        {
            ui->edit_password->setFocus();
            return false;
        }
    }

    return true;
}
