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

#include "client/desktop/credentials/credential_import_dialog.h"

#include <QAbstractButton>
#include <QIcon>
#include <QPushButton>

#include "base/logging.h"
#include "client/credential_file.h"
#include "client/database.h"
#include "client/desktop/file_dialog.h"
#include "client/desktop/credentials/ui_credential_import_dialog.h"
#include "common/desktop/msg_box.h"

namespace {

enum Column
{
    COLUMN_NAME = 0,
    COLUMN_USERNAME = 1,
    COLUMN_STATUS = 2
};

} // namespace

//--------------------------------------------------------------------------------------------------
CredentialImportDialog::CredentialImportDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::CredentialImportDialog>())
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);

    ui->edit_password->setShowPasswordButtonVisible(true);

    import_button_ = ui->button_box->addButton(tr("Import"), QDialogButtonBox::AcceptRole);
    import_button_->setAutoDefault(false);
    ui->button_open->setDefault(true);

    connect(ui->button_browse, &QPushButton::clicked,
            this, &CredentialImportDialog::onBrowseButtonPressed);
    connect(ui->button_open, &QPushButton::clicked,
            this, &CredentialImportDialog::onOpenButtonPressed);
    connect(ui->button_check_all, &QPushButton::clicked,
            this, &CredentialImportDialog::onCheckAllButtonPressed);
    connect(ui->button_check_none, &QPushButton::clicked,
            this, &CredentialImportDialog::onCheckNoneButtonPressed);
    connect(ui->button_box, &QDialogButtonBox::clicked,
            this, &CredentialImportDialog::onButtonBoxClicked);

    // The list shows the file that was opened, so another file empties it.
    connect(ui->edit_file, &QLineEdit::textChanged, this, &CredentialImportDialog::clearCredentials);
    connect(ui->edit_password, &QLineEdit::textEdited, this, &CredentialImportDialog::updateButtons);
    connect(ui->tree_credentials, &QTreeWidget::itemChanged, this, &CredentialImportDialog::updateButtons);

    updateButtons();
}

//--------------------------------------------------------------------------------------------------
CredentialImportDialog::~CredentialImportDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void CredentialImportDialog::onBrowseButtonPressed()
{
    const QString file_path = FileDialog::getOpenFileName(
        this,
        tr("Import Credentials"),
        tr("Aspia Credentials (*.aspia-credentials);;All files (*)"));

    if (file_path.isEmpty())
        return;

    ui->edit_file->setText(file_path);
    ui->edit_password->setFocus();
}

//--------------------------------------------------------------------------------------------------
void CredentialImportDialog::onOpenButtonPressed()
{
    LOG(INFO) << "[ACTION] Open credentials file";

    clearCredentials();

    QList<CredentialConfig> credentials;

    switch (CredentialFile::importFromFile(ui->edit_file->text(), ui->edit_password->password(), &credentials))
    {
        case CredentialFile::Result::SUCCESS:
            break;

        case CredentialFile::Result::WRONG_PASSWORD:
            MsgBox::warning(this, tr("Unable to decrypt the file with the specified password."));
            ui->edit_password->setFocus();
            ui->edit_password->selectAll();
            return;

        case CredentialFile::Result::FILE_ERROR:
            MsgBox::warning(this, tr("Unable to read the file."));
            return;

        case CredentialFile::Result::UNSUPPORTED_VERSION:
            MsgBox::warning(this, tr("Unsupported file format version."));
            return;

        case CredentialFile::Result::INVALID_FORMAT:
            MsgBox::warning(this, tr("The file is not a valid credentials file."));
            return;

        default:
            MsgBox::warning(this, tr("Failed to import the credentials."));
            return;
    }

    Database& db = Database::instance();

    for (int i = 0; i < credentials.size(); ++i)
    {
        const CredentialConfig& credential = credentials[i];

        CredentialConfig existing;
        const bool exists =
            db.findCredentialByGuid(credential.guid(), &existing) != Database::FindResult::NOT_FOUND;

        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setIcon(COLUMN_NAME, QIcon(exists ? ":/img/key-warning.svg" : ":/img/key-normal.svg"));
        item->setText(COLUMN_NAME, credential.displayName());
        item->setText(COLUMN_USERNAME, credential.username());
        item->setText(COLUMN_STATUS, exists ? tr("Exists") : tr("New"));
        item->setData(COLUMN_NAME, Qt::UserRole, i);
        item->setCheckState(COLUMN_NAME, Qt::Checked);

        ui->tree_credentials->addTopLevelItem(item);
    }

    credentials_ = std::move(credentials);

    ui->tree_credentials->sortItems(COLUMN_NAME, Qt::AscendingOrder);
    ui->tree_credentials->resizeColumnToContents(COLUMN_NAME);

    updateButtons();
}

//--------------------------------------------------------------------------------------------------
void CredentialImportDialog::onCheckAllButtonPressed()
{
    LOG(INFO) << "[ACTION] Check all button pressed";
    setCheckState(Qt::Checked);
}

//--------------------------------------------------------------------------------------------------
void CredentialImportDialog::onCheckNoneButtonPressed()
{
    LOG(INFO) << "[ACTION] Check none button pressed";
    setCheckState(Qt::Unchecked);
}

//--------------------------------------------------------------------------------------------------
void CredentialImportDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (button != import_button_)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        reject();
        return;
    }

    LOG(INFO) << "[ACTION] Import credentials";

    Database& db = Database::instance();

    int added = 0;
    int replaced = 0;

    for (CredentialConfig credential : checkedCredentials())
    {
        CredentialConfig existing;
        const Database::FindResult found = db.findCredentialByGuid(credential.guid(), &existing);

        bool written = false;

        if (found == Database::FindResult::NOT_FOUND)
        {
            written = db.addCredential(credential);
            if (written)
                ++added;
        }
        else if (found != Database::FindResult::FAILED)
        {
            credential.setId(existing.id());

            written = db.modifyCredential(credential);
            if (written)
                ++replaced;
        }

        if (!written)
        {
            MsgBox::warning(this,
                tr("Unable to write credentials \"%1\". The import was interrupted.\n"
                   "Credentials added: %2\n"
                   "Credentials replaced: %3")
                    .arg(credential.displayName()).arg(added).arg(replaced));
            reject();
            return;
        }
    }

    MsgBox::information(this,
        tr("Import completed successfully.\n"
           "Credentials added: %1\n"
           "Credentials replaced: %2").arg(added).arg(replaced));
    accept();
}

//--------------------------------------------------------------------------------------------------
void CredentialImportDialog::clearCredentials()
{
    ui->tree_credentials->clear();
    credentials_.clear();
    updateButtons();
}

//--------------------------------------------------------------------------------------------------
void CredentialImportDialog::setCheckState(Qt::CheckState state)
{
    for (int i = 0; i < ui->tree_credentials->topLevelItemCount(); ++i)
        ui->tree_credentials->topLevelItem(i)->setCheckState(COLUMN_NAME, state);
}

//--------------------------------------------------------------------------------------------------
void CredentialImportDialog::updateButtons()
{
    ui->button_open->setEnabled(!ui->edit_file->text().isEmpty() && !ui->edit_password->password().isEmpty());
    import_button_->setEnabled(!checkedCredentials().isEmpty());
}

//--------------------------------------------------------------------------------------------------
QList<CredentialConfig> CredentialImportDialog::checkedCredentials() const
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
