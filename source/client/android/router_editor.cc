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

#include "client/android/router_editor.h"

#include <QVBoxLayout>

#include <optional>

#include "base/crypto/secure_string.h"
#include "base/net/address.h"
#include "base/peer/user.h"
#include "build/build_config.h"
#include "client/config.h"
#include "client/database.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/message_dialog.h"
#include "common/android/scroll_area.h"
#include "proto/router.h"

namespace {

constexpr int kFormMargin = 16;
constexpr int kFormSpacing = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterEditor::RouterEditor(QWidget* parent)
    : QWidget(parent),
      name_(new LineEdit()),
      address_(new LineEdit()),
      username_(new LineEdit()),
      password_(new LineEdit()),
      error_(new Label(QString(), Label::Role::CAPTION))
{
    name_->setLabel(tr("Name"));
    address_->setLabel(tr("Address"));
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
    form_layout->addWidget(address_);
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

    connect(save, &Button::clicked, this, &RouterEditor::onSaveClicked);
    connect(delete_button_, &Button::clicked, this, &RouterEditor::onDeleteClicked);
}

//--------------------------------------------------------------------------------------------------
RouterEditor::~RouterEditor() = default;

//--------------------------------------------------------------------------------------------------
void RouterEditor::prepareForAdd()
{
    router_id_ = -1;
    encrypted_device_token_.clear();

    name_->clear();
    address_->clear();
    username_->clear();
    password_->clear();
    error_->setVisible(false);
    delete_button_->hide();

    name_->setFocus();
}

//--------------------------------------------------------------------------------------------------
void RouterEditor::prepareForEdit(qint64 router_id)
{
    std::optional<RouterConfig> router = Database::instance().findRouter(router_id);
    if (!router.has_value())
        return;

    router_id_ = router_id;
    encrypted_device_token_ = router->encryptedDeviceToken();

    name_->setText(router->displayName());
    address_->setText(router->address());
    username_->setText(router->username());
    password_->setText(router->password().toString());
    error_->setVisible(false);
    delete_button_->show();

    name_->setFocus();
}

//--------------------------------------------------------------------------------------------------
void RouterEditor::onSaveClicked()
{
    const QString address_text = address_->text();
    Address address = Address::fromString(address_text, DEFAULT_ROUTER_CLIENT_TCP_PORT);
    if (!address.isValid())
    {
        showError(tr("An invalid router address was entered."));
        address_->setFocus();
        address_->selectAll();
        return;
    }

    const QString username = username_->text();
    if (!User::isValidUserName(username))
    {
        showError(tr("The user name can not be empty and can contain only alphabet characters,"
                     " numbers and \"_\", \"-\", \".\" characters."));
        username_->setFocus();
        username_->selectAll();
        return;
    }

    SecureString password(password_->text());
    if (!User::isValidPassword(password))
    {
        showError(tr("Password cannot be empty."));
        password_->setFocus();
        password_->selectAll();
        return;
    }

    RouterConfig data;
    data.setRouterId(router_id_);
    data.setDisplayName(name_->text());
    data.setAddress(address_text);
    data.setSessionType(proto::router::SESSION_TYPE_CLIENT);
    data.setUsername(username);
    data.setPassword(password);
    data.setEncryptedDeviceToken(encrypted_device_token_);

    Database& db = Database::instance();
    const bool saved = (router_id_ < 0) ? db.addRouter(data) : db.modifyRouter(data);
    if (!saved)
    {
        showError(tr("Failed to save the router."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void RouterEditor::onDeleteClicked()
{
    if (!MessageDialog::confirm(this, tr("Delete Router"),
                                tr("Delete the router \"%1\"?").arg(name_->text()), tr("Delete")))
    {
        return;
    }

    if (!Database::instance().removeRouter(router_id_))
    {
        showError(tr("Failed to delete the router."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void RouterEditor::showError(const QString& message)
{
    error_->setText(message);
    error_->setVisible(true);
}
