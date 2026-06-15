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

#include "client/android/router_edit_dialog.h"

#include <QVBoxLayout>

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
#include "proto/router.h"

//--------------------------------------------------------------------------------------------------
RouterEditDialog::RouterEditDialog(qint64 router_id, QWidget* parent)
    : Dialog(parent),
      name_(new LineEdit(this)),
      address_(new LineEdit(this)),
      username_(new LineEdit(this)),
      password_(new LineEdit(this)),
      error_(new Label(QString(), Label::Role::CAPTION, this)),
      router_id_(router_id)
{
    setTitle(router_id_ < 0 ? tr("Add Router") : tr("Edit Router"));

    name_->setLabel(tr("Name"));
    address_->setLabel(tr("Address"));
    username_->setLabel(tr("User Name"));
    password_->setLabel(tr("Password"));
    password_->setEchoMode(QLineEdit::Password);

    // A fixed hex keeps the error color readable on both light and dark cards and survives the
    // palette reset that the caption role applies on theme changes.
    error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    error_->setWordWrap(true);
    error_->setVisible(false);

    QVBoxLayout* content = contentLayout();
    content->addWidget(error_);
    content->addWidget(name_);
    content->addWidget(address_);
    content->addWidget(username_);
    content->addWidget(password_);

    if (router_id_ >= 0)
    {
        std::optional<RouterConfig> router = Database::instance().findRouter(router_id_);
        if (router.has_value())
        {
            name_->setText(router->displayName());
            address_->setText(router->address());
            username_->setText(router->username());
            password_->setText(router->password().toString());
            encrypted_device_token_ = router->encryptedDeviceToken();
        }
    }

    Button* cancel = addButton(tr("Cancel"), Button::Role::TEXT);
    Button* save = addButton(tr("Save"), Button::Role::FILLED);

    connect(cancel, &Button::clicked, this, &RouterEditDialog::reject);
    connect(save, &Button::clicked, this, &RouterEditDialog::onSaveClicked);
}

//--------------------------------------------------------------------------------------------------
RouterEditDialog::~RouterEditDialog() = default;

//--------------------------------------------------------------------------------------------------
void RouterEditDialog::onSaveClicked()
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

    accept();
}

//--------------------------------------------------------------------------------------------------
void RouterEditDialog::showError(const QString& message)
{
    error_->setText(message);
    error_->setVisible(true);
}
