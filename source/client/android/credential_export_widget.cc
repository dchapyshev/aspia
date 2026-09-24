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

#include "client/android/credential_export_widget.h"

#include <QFileDialog>
#include <QVBoxLayout>

#include <algorithm>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "client/credential_file.h"
#include "client/database.h"
#include "client/master_password.h"
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
CredentialExportWidget::CredentialExportWidget(QWidget* parent)
    : QWidget(parent),
      list_(new CredentialSelectionList()),
      edit_password_(new LineEdit()),
      label_error_(new Label(QString(), Label::Role::CAPTION)),
      button_save_(new Button(QString(), Button::Role::FILLED)),
      button_select_all_(new IconButton(":/img/material/done_all.svg", this))
{
    // The action lives in the app bar; AppBar::setActions() reparents and shows it. Hidden by
    // default so it does not linger in this widget.
    button_select_all_->hide();

    Label* description = new Label(tr("The file will be encrypted with this password."),
                                   Label::Role::CAPTION);
    description->setWordWrap(true);

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
    panel_layout->addWidget(description);
    panel_layout->addWidget(edit_password_);
    panel_layout->addWidget(label_error_);
    panel_layout->addWidget(button_save_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(list_, 1);
    layout->addLayout(panel_layout);

    connect(button_select_all_, &IconButton::clicked, list_, &CredentialSelectionList::toggleAll);
    connect(list_, &CredentialSelectionList::sig_selectionChanged,
            this, &CredentialExportWidget::updateSaveButton);
    connect(button_save_, &Button::clicked, this, &CredentialExportWidget::onSaveClicked);
}

//--------------------------------------------------------------------------------------------------
CredentialExportWidget::~CredentialExportWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> CredentialExportWidget::appBarActions() const
{
    return { button_select_all_ };
}

//--------------------------------------------------------------------------------------------------
void CredentialExportWidget::prepare()
{
    credentials_.clear();

    edit_password_->clear();
    edit_password_->setEchoMode(QLineEdit::Password);
    label_error_->setVisible(false);

    if (Database::instance().credentialList(&credentials_) == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the list of credentials";
        showError(tr("Failed to read the credentials."));
    }

    std::sort(credentials_.begin(), credentials_.end(),
              [](const CredentialConfig& first, const CredentialConfig& second)
    {
        return first.displayName().localeAwareCompare(second.displayName()) < 0;
    });

    QList<CredentialSelectionList::Item> items;

    for (const CredentialConfig& credential : std::as_const(credentials_))
    {
        // A record whose data could not be read has nothing to export.
        CredentialSelectionList::Item item;
        item.name = credential.displayName();
        item.enabled = credential.isValid();
        item.details = item.enabled ? credential.username() : tr("Could not be read");
        items.append(item);
    }

    list_->setItems(items);
}

//--------------------------------------------------------------------------------------------------
void CredentialExportWidget::onSaveClicked()
{
    const QList<CredentialConfig> credentials = checkedCredentials();
    if (credentials.isEmpty() || !checkPassword())
        return;

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Credentials"), "aspia_credentials.aspia-credentials",
        tr("Aspia Credentials (*.aspia-credentials)"));
    if (path.isEmpty())
        return;

    switch (CredentialFile::exportToFile(credentials, path, SecureString(edit_password_->text())))
    {
        case CredentialFile::Result::SUCCESS:
            break;

        case CredentialFile::Result::FILE_ERROR:
            showError(tr("Unable to write the file."));
            return;

        default:
            showError(tr("Failed to export the credentials."));
            return;
    }

    MessageDialog::info(this, tr("Export Credentials"),
                        tr("Export completed successfully.\nCredentials exported: %1")
                            .arg(credentials.size()));
    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void CredentialExportWidget::updateSaveButton()
{
    const int count = static_cast<int>(checkedCredentials().size());
    button_save_->setText(tr("Save (%1)").arg(count));
    button_save_->setEnabled(count > 0);
}

//--------------------------------------------------------------------------------------------------
QList<CredentialConfig> CredentialExportWidget::checkedCredentials() const
{
    QList<CredentialConfig> credentials;

    for (int index : list_->checkedItems())
        credentials.append(credentials_[index]);

    return credentials;
}

//--------------------------------------------------------------------------------------------------
bool CredentialExportWidget::checkPassword()
{
    const SecureString password(edit_password_->text());

    if (password.size() < MasterPassword::kMinPasswordLength)
    {
        showError(tr("The password can not be shorter than %n characters.",
                     "", MasterPassword::kMinPasswordLength));
        edit_password_->setFocus();
        return false;
    }

    if (!MasterPassword::isSafePassword(password))
    {
        const QString text =
            tr("The password must contain lowercase and uppercase characters, numbers and should "
               "not be shorter than %n characters.", "", MasterPassword::kSafePasswordLength);

        if (!MessageDialog::confirm(this, tr("Weak Password"), text, tr("Continue")))
        {
            edit_password_->setFocus();
            return false;
        }
    }

    label_error_->setVisible(false);
    return true;
}

//--------------------------------------------------------------------------------------------------
void CredentialExportWidget::showError(const QString& message)
{
    label_error_->setText(message);
    label_error_->setVisible(true);
}
