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

#include "client/desktop/router_dialog.h"

#include <QAbstractButton>
#include <QComboBox>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "base/net/address.h"
#include "base/peer/user.h"
#include "build/build_config.h"
#include "client/config.h"
#include "client/database.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/password_edit.h"
#include "proto/router.h"
#include "ui_router_dialog.h"

//--------------------------------------------------------------------------------------------------
RouterDialog::RouterDialog(qint64 router_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterDialog>()),
      router_id_(router_id)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->combo_session_type->addItem(tr("Administrator"), proto::router::SESSION_TYPE_ADMIN);
    ui->combo_session_type->addItem(tr("Manager"), proto::router::SESSION_TYPE_MANAGER);
    ui->combo_session_type->addItem(tr("Operator"), proto::router::SESSION_TYPE_OPERATOR);
    ui->combo_session_type->setCurrentIndex(2);

    if (router_id_ != -1)
    {
        std::optional<RouterConfig> router = Database::instance().findRouter(router_id_);
        if (router.has_value())
        {
            ui->edit_name->setText(router->displayName());
            ui->edit_address->setText(router->address());

            int session_type_index = ui->combo_session_type->findData(router->sessionType());
            if (session_type_index != -1)
                ui->combo_session_type->setCurrentIndex(session_type_index);

            ui->edit_username->setText(router->username());
            ui->edit_password->setPassword(router->password());
        }
        else
        {
            LOG(ERROR) << "Unable to find router with id" << router_id_;
        }
    }

    ui->edit_password->setShowPasswordButtonVisible(true);
    connect(ui->buttonbox, &QDialogButtonBox::clicked, this, &RouterDialog::onButtonBoxClicked);
}

//--------------------------------------------------------------------------------------------------
RouterDialog::~RouterDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui->buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        reject();
        return;
    }

    if (ui->edit_name->text().length() > RouterConfig::kMaxNameLength)
    {
        LOG(ERROR) << "Too long router name entered";
        showError(tr("Too long name. The maximum length of the name is %n characters.",
                     "", RouterConfig::kMaxNameLength));
        ui->edit_name->setFocus();
        ui->edit_name->selectAll();
        return;
    }

    QString address_text = ui->edit_address->text();
    Address address = Address::fromString(address_text, DEFAULT_ROUTER_CLIENT_TCP_PORT);
    if (!address.isValid())
    {
        LOG(ERROR) << "Invalid router address entered";
        showError(tr("An invalid router address was entered."));
        ui->edit_address->setFocus();
        ui->edit_address->selectAll();
        return;
    }

    QString username = ui->edit_username->text();
    if (!User::isValidUserName(username))
    {
        LOG(ERROR) << "Invalid user name entered";
        showError(tr("The user name can not be empty and can contain only"
                     " alphabet characters, numbers and ""_"", ""-"", ""."" characters."));
        ui->edit_username->setFocus();
        ui->edit_username->selectAll();
        return;
    }

    SecureString password = ui->edit_password->password();
    if (!User::isValidPassword(password))
    {
        LOG(ERROR) << "Invalid password entered";
        showError(tr("Password cannot be empty."));
        ui->edit_password->setFocus();
        ui->edit_password->selectAll();
        return;
    }

    RouterConfig data;
    data.setRouterId(router_id_);
    data.setDisplayName(ui->edit_name->text());
    data.setAddress(address_text);
    data.setSessionType(
        static_cast<proto::router::SessionType>(ui->combo_session_type->currentData().toUInt()));
    data.setUsername(username);
    data.setPassword(password);

    Database& db = Database::instance();

    if (router_id_ < 0)
    {
        if (!db.addRouter(data))
        {
            LOG(ERROR) << "Failed to add router to database";
            showError(tr("Failed to save the router."));
            return;
        }
    }
    else
    {
        // The token is not edited here. The login of this record writes a fresh one whenever a
        // TOTP code is accepted, so it is read now instead of when the dialog opened and a token
        // issued meanwhile survives the save. It belongs to the account of the record, and an
        // edit that changes the account leaves it behind, because presented for another one it
        // would only be refused.
        const std::optional<RouterConfig> stored = db.findRouter(router_id_);
        if (!stored.has_value())
        {
            LOG(ERROR) << "Failed to re-read router" << router_id_;
            showError(tr("Failed to save the router."));
            return;
        }
        if (stored->hasSameParams(data))
            data.setDeviceToken(stored->deviceToken());

        if (!db.modifyRouter(data))
        {
            LOG(ERROR) << "Failed to modify router in database";
            showError(tr("Failed to save the router."));
            return;
        }
    }

    accept();
}

//--------------------------------------------------------------------------------------------------
void RouterDialog::showError(const QString& message)
{
    MsgBox::warning(this, message);
}
