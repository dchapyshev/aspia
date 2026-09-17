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
      shared_(new Switch(tr("Use existing"))),
      username_(new LineEdit()),
      password_(new LineEdit()),
      note_(new Label(tr("The user name and the password are stored on this device only and "
                         "are not sent to the router. Leave both empty to forget them."),
                      Label::Role::CAPTION)),
      credential_(new ComboBox()),
      error_(new Label(QString(), Label::Role::CAPTION))
{
    username_->setLabel(tr("User Name"));

    password_->setLabel(tr("Password"));
    password_->setEchoMode(QLineEdit::Password);

    credential_->setLabel(tr("Credentials"));

    error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    error_->setWordWrap(true);
    error_->setVisible(false);

    note_->setWordWrap(true);

    Button* save = new Button(tr("Save"), Button::Role::FILLED);

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(error_);
    form_layout->addWidget(shared_);
    form_layout->addWidget(username_);
    form_layout->addWidget(password_);
    form_layout->addWidget(note_);
    form_layout->addWidget(credential_);
    form_layout->addWidget(save);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(shared_, &Switch::toggled, this, &RouterHostEditor::onSharedToggled);
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

    username_->clear();
    password_->clear();
    error_->setVisible(false);

    qint64 credential_id = 0;

    std::optional<RouterHostConfig> credentials =
        Database::instance().findRouterHost(router_id_, host_id_);
    if (credentials.has_value())
    {
        username_->setText(credentials->username());
        password_->setText(credentials->password().toString());
        credential_id = credentials->credentialId();
    }

    loadCredentials(credential_id);
    username_->setFocus();
    return true;
}

//--------------------------------------------------------------------------------------------------
void RouterHostEditor::loadCredentials(qint64 selected_credential_id)
{
    credential_->clear();
    QList<CredentialConfig> credentials;
    Database::instance().credentialList(&credentials);
    for (const CredentialConfig& credential : std::as_const(credentials))
        credential_->addItem(credential.displayName(), QVariant::fromValue(credential.id()));

    const int index = credential_->findData(QVariant::fromValue(selected_credential_id));
    credential_->setCurrentIndex(index >= 0 ? index : 0);

    // Nothing to share until a record of credentials is added.
    shared_->setEnabled(!credentials.isEmpty());
    shared_->setChecked(index >= 0);
    onSharedToggled(shared_->isChecked());
}

//--------------------------------------------------------------------------------------------------
void RouterHostEditor::onSharedToggled(bool checked)
{
    if (checked && username_->text().isEmpty() != password_->text().isEmpty())
    {
        username_->clear();
        password_->clear();
    }

    username_->setVisible(!checked);
    password_->setVisible(!checked);
    note_->setVisible(!checked);
    credential_->setVisible(checked);
}

//--------------------------------------------------------------------------------------------------
void RouterHostEditor::onSaveClicked()
{
    const qint64 credential_id = shared_->isChecked() ? credential_->currentData().toLongLong() : 0;
    const QString username = username_->text();
    const QString password = password_->text();

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
    error_->setText(message);
    error_->setVisible(true);
}
