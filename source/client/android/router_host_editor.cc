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

#include "client/android/router_host_editor.h"

#include <QVBoxLayout>

#include <optional>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "client/config.h"
#include "client/database.h"
#include "common/android/button.h"
#include "common/android/combo_box.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/scroll_area.h"
#include "common/android/switch.h"

namespace {

constexpr int kFormMargin = 16;
constexpr int kFormSpacing = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterHostEditor::RouterHostEditor(QWidget* parent)
    : QWidget(parent),
      switch_saved_credentials_(new Switch(tr("Use existing"))),
      edit_username_(new LineEdit()),
      edit_password_(new LineEdit()),
      label_note_(new Label(tr("The user name and the password are stored on this device only and "
                         "are not sent to the router. Leave both empty to forget them."),
                      Label::Role::CAPTION)),
      combo_credential_(new ComboBox()),
      label_error_(new Label(QString(), Label::Role::CAPTION))
{
    edit_username_->setLabel(tr("User Name"));

    edit_password_->setLabel(tr("Password"));
    edit_password_->setEchoMode(QLineEdit::Password);

    combo_credential_->setLabel(tr("Credentials"));

    label_error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    label_error_->setWordWrap(true);
    label_error_->setVisible(false);

    label_note_->setWordWrap(true);

    Button* save = new Button(tr("Save"), Button::Role::FILLED);

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(label_error_);
    form_layout->addWidget(switch_saved_credentials_);
    form_layout->addWidget(edit_username_);
    form_layout->addWidget(edit_password_);
    form_layout->addWidget(label_note_);
    form_layout->addWidget(combo_credential_);
    form_layout->addWidget(save);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(switch_saved_credentials_, &Switch::toggled, this, &RouterHostEditor::onSavedCredentialsToggled);
    connect(save, &Button::clicked, this, &RouterHostEditor::onSaveClicked);
}

//--------------------------------------------------------------------------------------------------
RouterHostEditor::~RouterHostEditor() = default;

//--------------------------------------------------------------------------------------------------
bool RouterHostEditor::prepareForEdit(qint64 router_id, HostId host_id)
{
    // A temporary host id is handed out at random and comes back for another machine, so what was
    // saved under it would be sent to a host the user never gave it to.
    if (router_id <= 0 || host_id == kInvalidHostId || isTempHostId(host_id))
    {
        LOG(ERROR) << "Credentials cannot be kept for host" << host_id << "of router" << router_id;
        return false;
    }

    router_id_ = router_id;
    host_id_ = host_id;

    edit_username_->clear();
    edit_password_->clear();
    label_error_->setVisible(false);

    qint64 credential_id = 0;

    std::optional<RouterHostConfig> credentials =
        Database::instance().findRouterHost(router_id_, host_id_);
    if (credentials.has_value())
    {
        edit_username_->setText(credentials->username());
        edit_password_->setText(credentials->password().toString());
        credential_id = credentials->credentialId();
    }

    loadCredentials(credential_id);
    edit_username_->setFocus();
    return true;
}

//--------------------------------------------------------------------------------------------------
void RouterHostEditor::loadCredentials(qint64 selected_credential_id)
{
    combo_credential_->clear();
    QList<CredentialConfig> credentials;
    Database::instance().credentialList(&credentials);
    for (const CredentialConfig& credential : std::as_const(credentials))
        combo_credential_->addItem(credential.displayName(), QVariant::fromValue(credential.id()));

    const int index = combo_credential_->findData(QVariant::fromValue(selected_credential_id));
    combo_credential_->setCurrentIndex(index >= 0 ? index : 0);

    // Nothing to share until a record of credentials is added.
    switch_saved_credentials_->setEnabled(!credentials.isEmpty());
    switch_saved_credentials_->setChecked(index >= 0);
    onSavedCredentialsToggled(switch_saved_credentials_->isChecked());
}

//--------------------------------------------------------------------------------------------------
void RouterHostEditor::onSavedCredentialsToggled(bool checked)
{
    if (checked && edit_username_->text().isEmpty() != edit_password_->text().isEmpty())
    {
        edit_username_->clear();
        edit_password_->clear();
    }

    edit_username_->setVisible(!checked);
    edit_password_->setVisible(!checked);
    label_note_->setVisible(!checked);
    combo_credential_->setVisible(checked);
}

//--------------------------------------------------------------------------------------------------
void RouterHostEditor::onSaveClicked()
{
    const qint64 credential_id =
        switch_saved_credentials_->isChecked() ? combo_credential_->currentData().toLongLong() : 0;
    const QString username = edit_username_->text();
    const QString password = edit_password_->text();

    if (username.isEmpty() != password.isEmpty())
    {
        showError(tr("Enter both the user name and the password, or leave both empty."));
        return;
    }

    Database& db = Database::instance();

    if (username.isEmpty() && credential_id <= 0)
    {
        if (!db.removeRouterHost(router_id_, host_id_))
        {
            showError(tr("Failed to save the credentials."));
            return;
        }

        emit sig_accepted();
        return;
    }

    RouterHostConfig credentials;
    credentials.setRouterId(router_id_);
    credentials.setHostId(host_id_);
    credentials.setCredentialId(credential_id);
    credentials.setUsername(username);
    credentials.setPassword(SecureString(password));

    bool saved = false;

    if (db.findRouterHost(router_id_, host_id_).has_value())
        saved = db.modifyRouterHost(credentials);
    else
        saved = db.addRouterHost(credentials);

    if (!saved)
    {
        showError(tr("Failed to save the credentials."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void RouterHostEditor::showError(const QString& message)
{
    label_error_->setText(message);
    label_error_->setVisible(true);
}
