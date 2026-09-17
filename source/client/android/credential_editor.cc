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
      name_(new LineEdit()),
      username_(new LineEdit()),
      password_(new LineEdit()),
      error_(new Label(QString(), Label::Role::CAPTION))
{
    name_->setLabel(tr("Name"));
    username_->setLabel(tr("User Name"));
    password_->setLabel(tr("Password"));
    password_->setEchoMode(QLineEdit::Password);

    // A fixed hex keeps the error color readable on both light and dark surfaces and survives the
    // palette reset that the caption role applies on theme changes.
    error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    error_->setWordWrap(true);
    error_->setVisible(false);

    Button* save = new Button(tr("Save"), Button::Role::FILLED);

    // The delete action is destructive, so its text is tinted red and it shows only when editing.
    delete_button_ = new Button(tr("Delete"), Button::Role::TEXT);
    delete_button_->setAccentColor(Controls::errorColor());
    delete_button_->hide();

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(error_);
    form_layout->addWidget(name_);
    form_layout->addWidget(username_);
    form_layout->addWidget(password_);
    form_layout->addWidget(save);
    form_layout->addWidget(delete_button_);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(save, &Button::clicked, this, &CredentialEditor::onSaveClicked);
    connect(delete_button_, &Button::clicked, this, &CredentialEditor::onDeleteClicked);
}

//--------------------------------------------------------------------------------------------------
CredentialEditor::~CredentialEditor() = default;

//--------------------------------------------------------------------------------------------------
void CredentialEditor::prepareForAdd()
{
    credential_id_ = -1;

    name_->clear();
    username_->clear();
    password_->clear();
    error_->setVisible(false);
    delete_button_->hide();

    name_->setFocus();
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

    name_->setText(credential->displayName());
    username_->setText(credential->username());
    password_->setText(credential->password().toString());
    error_->setVisible(false);
    delete_button_->show();

    name_->setFocus();
    return true;
}

//--------------------------------------------------------------------------------------------------
void CredentialEditor::onSaveClicked()
{
    const QString name = name_->text();
    if (name.isEmpty())
    {
        showError(tr("Name cannot be empty."));
        name_->setFocus();
        return;
    }

    if (name.length() > CredentialConfig::kMaxNameLength)
    {
        showError(tr("Too long name. The maximum length of the name is %n characters.",
                     "", CredentialConfig::kMaxNameLength));
        name_->setFocus();
        name_->selectAll();
        return;
    }

    if (username_->text().isEmpty())
    {
        showError(tr("User name cannot be empty."));
        username_->setFocus();
        return;
    }

    if (password_->text().isEmpty())
    {
        showError(tr("Password cannot be empty."));
        password_->setFocus();
        return;
    }

    CredentialConfig credential;
    credential.setId(credential_id_);
    credential.setType(CredentialConfig::Type::HOST);
    credential.setDisplayName(name);
    credential.setUsername(username_->text());
    credential.setPassword(SecureString(password_->text()));

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
                                tr("Delete the credentials \"%1\"?").arg(name_->text()), tr("Delete")))
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
    error_->setText(message);
    error_->setVisible(true);
}
