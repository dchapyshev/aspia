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

#include "client/android/credential_import_widget.h"

#include <QVBoxLayout>

#include <algorithm>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "client/credential_file.h"
#include "client/database.h"
#include "client/android/credential_selection_list.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/icon_button.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/message_dialog.h"

namespace {

constexpr int kPanelMargin = 16;
constexpr int kPanelSpacing = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
CredentialImportWidget::CredentialImportWidget(QWidget* parent)
    : QWidget(parent),
      list_(new CredentialSelectionList()),
      label_description_(new Label(QString(), Label::Role::CAPTION)),
      edit_password_(new LineEdit()),
      label_error_(new Label(QString(), Label::Role::CAPTION)),
      button_open_(new Button(tr("Open"), Button::Role::FILLED)),
      button_import_(new Button(QString(), Button::Role::FILLED)),
      button_select_all_(new IconButton(":/img/material/done_all.svg", this))
{
    // The action lives in the app bar; AppBar::setActions() reparents and shows it. Hidden by
    // default so it does not linger in this widget.
    button_select_all_->hide();

    label_description_->setWordWrap(true);

    edit_password_->setLabel(tr("Password"));
    edit_password_->setEchoMode(QLineEdit::Password);
    edit_password_->setShowPasswordButtonVisible(true);

    // A fixed hex keeps the error color readable on both light and dark surfaces and survives the
    // palette reset that the caption role applies on theme changes.
    label_error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    label_error_->setWordWrap(true);
    label_error_->setVisible(false);

    QVBoxLayout* panel_layout = new QVBoxLayout();
    panel_layout->setContentsMargins(kPanelMargin, kPanelMargin, kPanelMargin, kPanelMargin);
    panel_layout->setSpacing(kPanelSpacing);
    panel_layout->addWidget(label_description_);
    panel_layout->addWidget(edit_password_);
    panel_layout->addWidget(label_error_);
    panel_layout->addWidget(button_open_);
    panel_layout->addWidget(button_import_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(list_, 1);
    layout->addLayout(panel_layout);

    connect(button_select_all_, &IconButton::clicked, list_, &CredentialSelectionList::toggleAll);
    connect(list_, &CredentialSelectionList::sig_selectionChanged,
            this, &CredentialImportWidget::updateImportButton);
    connect(edit_password_, &LineEdit::returnPressed, this, &CredentialImportWidget::onOpenClicked);
    connect(button_open_, &Button::clicked, this, &CredentialImportWidget::onOpenClicked);
    connect(button_import_, &Button::clicked, this, &CredentialImportWidget::onImportClicked);
}

//--------------------------------------------------------------------------------------------------
CredentialImportWidget::~CredentialImportWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> CredentialImportWidget::appBarActions() const
{
    return { button_select_all_ };
}

//--------------------------------------------------------------------------------------------------
void CredentialImportWidget::prepare(const QString& file_path)
{
    clear();
    file_path_ = file_path;

    setOpened(false);
    edit_password_->setFocus();
}

//--------------------------------------------------------------------------------------------------
void CredentialImportWidget::clear()
{
    file_path_.clear();
    credentials_.clear();
    list_->setItems({});

    edit_password_->clear();
    edit_password_->setEchoMode(QLineEdit::Password);
    label_error_->setVisible(false);
}

//--------------------------------------------------------------------------------------------------
void CredentialImportWidget::onOpenClicked()
{
    if (edit_password_->text().isEmpty())
    {
        showError(tr("Password cannot be empty."));
        edit_password_->setFocus();
        return;
    }

    QList<CredentialConfig> credentials;

    switch (CredentialFile::importFromFile(file_path_, SecureString(edit_password_->text()), &credentials))
    {
        case CredentialFile::Result::SUCCESS:
            break;

        case CredentialFile::Result::WRONG_PASSWORD:
            showError(tr("Unable to decrypt the file with the specified password."));
            edit_password_->setFocus();
            edit_password_->selectAll();
            return;

        case CredentialFile::Result::FILE_ERROR:
            showError(tr("Unable to read the file."));
            return;

        case CredentialFile::Result::UNSUPPORTED_VERSION:
            showError(tr("Unsupported file format version."));
            return;

        case CredentialFile::Result::INVALID_FORMAT:
            showError(tr("The file is not a valid credentials file."));
            return;

        default:
            showError(tr("Failed to import the credentials."));
            return;
    }

    std::sort(credentials.begin(), credentials.end(),
              [](const CredentialConfig& first, const CredentialConfig& second)
    {
        return first.displayName().localeAwareCompare(second.displayName()) < 0;
    });

    Database& db = Database::instance();
    QList<CredentialSelectionList::Item> items;

    for (const CredentialConfig& credential : std::as_const(credentials))
    {
        CredentialConfig existing;
        const bool exists =
            db.findCredentialByGuid(credential.guid(), &existing) != Database::FindResult::NOT_FOUND;

        CredentialSelectionList::Item item;
        item.name = credential.displayName();
        item.details = credential.username();
        item.status = exists ? tr("Exists") : tr("New");
        item.checked = true;
        items.append(item);
    }

    credentials_ = std::move(credentials);

    edit_password_->clear();
    label_error_->setVisible(false);
    setOpened(true);

    list_->setItems(items);
}

//--------------------------------------------------------------------------------------------------
void CredentialImportWidget::onImportClicked()
{
    Database& db = Database::instance();

    int added = 0;
    int replaced = 0;

    for (int index : list_->checkedItems())
    {
        CredentialConfig credential = credentials_[index];

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
            MessageDialog::info(this, tr("Import Credentials"),
                tr("Unable to write credentials \"%1\". The import was interrupted.\n"
                   "Credentials added: %2\n"
                   "Credentials replaced: %3")
                    .arg(credential.displayName()).arg(added).arg(replaced));
            emit sig_finished();
            return;
        }
    }

    MessageDialog::info(this, tr("Import Credentials"),
        tr("Import completed successfully.\n"
           "Credentials added: %1\n"
           "Credentials replaced: %2").arg(added).arg(replaced));
    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void CredentialImportWidget::setOpened(bool opened)
{
    label_description_->setText(opened ?
        tr("The checked credentials will be imported, and the ones that already exist will be "
           "replaced.") :
        tr("Enter the password the file was exported with."));

    edit_password_->setVisible(!opened);
    button_open_->setVisible(!opened);
    button_import_->setVisible(opened);

    updateImportButton();
}

//--------------------------------------------------------------------------------------------------
void CredentialImportWidget::updateImportButton()
{
    const int count = static_cast<int>(list_->checkedItems().size());
    button_import_->setText(tr("Import (%1)").arg(count));
    button_import_->setEnabled(count > 0);
}

//--------------------------------------------------------------------------------------------------
void CredentialImportWidget::showError(const QString& message)
{
    label_error_->setText(message);
    label_error_->setVisible(true);
}
