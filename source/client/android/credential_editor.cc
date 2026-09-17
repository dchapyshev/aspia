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

#include "client/android/credential_editor.h"

#include <QVBoxLayout>

#include <optional>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "base/peer/user.h"
#include "client/config.h"
#include "client/database.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/message_dialog.h"
#include "common/android/scroll_area.h"

namespace {

constexpr int kFormMargin = 16;
constexpr int kFormSpacing = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
CredentialEditor::CredentialEditor(QWidget* parent)
    : QWidget(parent),
      edit_name_(new LineEdit()),
      edit_username_(new LineEdit()),
      edit_password_(new LineEdit()),
      label_error_(new Label(QString(), Label::Role::CAPTION))
{
    edit_name_->setLabel(tr("Name"));
    edit_username_->setLabel(tr("User Name"));
    edit_password_->setLabel(tr("Password"));
    edit_password_->setEchoMode(QLineEdit::Password);

    // A fixed hex keeps the error color readable on both light and dark surfaces and survives the
    // palette reset that the caption role applies on theme changes.
    label_error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    label_error_->setWordWrap(true);
    label_error_->setVisible(false);

    Button* save = new Button(tr("Save"), Button::Role::FILLED);

    // The delete action is destructive, so its text is tinted red and it shows only when editing.
    button_delete_ = new Button(tr("Delete"), Button::Role::TEXT);
    button_delete_->setAccentColor(Controls::errorColor());
    button_delete_->hide();

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(label_error_);
    form_layout->addWidget(edit_name_);
    form_layout->addWidget(edit_username_);
    form_layout->addWidget(edit_password_);
    form_layout->addWidget(save);
    form_layout->addWidget(button_delete_);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(save, &Button::clicked, this, &CredentialEditor::onSaveClicked);
    connect(button_delete_, &Button::clicked, this, &CredentialEditor::onDeleteClicked);
}

//--------------------------------------------------------------------------------------------------
CredentialEditor::~CredentialEditor() = default;

//--------------------------------------------------------------------------------------------------
void CredentialEditor::prepareForAdd()
{
    credential_id_ = -1;

    edit_name_->clear();
    edit_username_->clear();
    edit_password_->clear();
    label_error_->setVisible(false);
    button_delete_->hide();

    edit_name_->setFocus();
}

//--------------------------------------------------------------------------------------------------
bool CredentialEditor::prepareForEdit(qint64 credential_id)
{
    std::optional<CredentialConfig> credential = Database::instance().findCredential(credential_id);
    if (!credential.has_value())
    {
        LOG(ERROR) << "Credentials not found:" << credential_id;
        return false;
    }

    credential_id_ = credential_id;

    edit_name_->setText(credential->displayName());
    edit_username_->setText(credential->username());
    edit_password_->setText(credential->password().toString());
    label_error_->setVisible(false);
    button_delete_->show();

    edit_name_->setFocus();
    return true;
}

//--------------------------------------------------------------------------------------------------
void CredentialEditor::onSaveClicked()
{
    const QString name = edit_name_->text();
    if (name.isEmpty())
    {
        showError(tr("Name cannot be empty."));
        edit_name_->setFocus();
        return;
    }

    if (name.length() > CredentialConfig::kMaxNameLength)
    {
        showError(tr("Too long name. The maximum length of the name is %n characters.",
                     "", CredentialConfig::kMaxNameLength));
        edit_name_->setFocus();
        edit_name_->selectAll();
        return;
    }

    if (!User::isValidUserName(edit_username_->text()))
    {
        showError(tr("The user name can not be empty and can contain only alphabet characters,"
                     " numbers and \"_\", \"-\", \".\" characters."));
        edit_username_->setFocus();
        edit_username_->selectAll();
        return;
    }

    if (edit_password_->text().isEmpty())
    {
        showError(tr("Password cannot be empty."));
        edit_password_->setFocus();
        return;
    }

    CredentialConfig credential;
    credential.setId(credential_id_);
    credential.setType(CredentialConfig::Type::HOST);
    credential.setDisplayName(name);
    credential.setUsername(edit_username_->text());
    credential.setPassword(SecureString(edit_password_->text()));

    Database& db = Database::instance();

    const bool saved = (credential_id_ < 0) ? db.addCredential(credential)
                                            : db.modifyCredential(credential);
    if (!saved)
    {
        showError(tr("Failed to save the credentials."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void CredentialEditor::onDeleteClicked()
{
    if (!MessageDialog::confirm(this, tr("Delete Credentials"),
                                tr("Delete the credentials \"%1\"?").arg(edit_name_->text()), tr("Delete")))
    {
        return;
    }

    if (!Database::instance().removeCredential(credential_id_))
    {
        showError(tr("Failed to delete the credentials."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void CredentialEditor::showError(const QString& message)
{
    label_error_->setText(message);
    label_error_->setVisible(true);
}
