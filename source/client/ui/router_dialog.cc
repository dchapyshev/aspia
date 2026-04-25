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

#include "client/ui/router_dialog.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QToolButton>

#include "common/ui/msg_box.h"
#include "base/logging.h"
#include "base/net/address.h"
#include "base/peer/user.h"
#include "build/build_config.h"
#include "client/local_data.h"
#include "client/database.h"
#include "proto/router.h"

namespace client {

//--------------------------------------------------------------------------------------------------
RouterDialog::RouterDialog(qint64 router_id, QWidget* parent)
    : QDialog(parent),
      router_id_(router_id)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    ui.combo_session_type->addItem(tr("Administrator"), proto::router::SESSION_TYPE_ADMIN);
    ui.combo_session_type->addItem(tr("Manager"), proto::router::SESSION_TYPE_MANAGER);
    ui.combo_session_type->addItem(tr("Client"), proto::router::SESSION_TYPE_CLIENT);
    ui.combo_session_type->setCurrentIndex(2);

    if (router_id_ != -1)
    {
        std::optional<RouterData> router = Database::instance().findRouter(router_id_);
        if (router.has_value())
        {
            base::Address address(DEFAULT_ROUTER_TCP_PORT);
            address.setHost(router->address);
            address.setPort(router->port);

            ui.edit_name->setText(router->name);
            ui.edit_address->setText(address.toString());

            int session_type_index = ui.combo_session_type->findData(router->session_type);
            if (session_type_index != -1)
                ui.combo_session_type->setCurrentIndex(session_type_index);

            ui.edit_username->setText(router->username);
            ui.edit_password->setText(router->password);
        }
        else
        {
            LOG(ERROR) << "Unable to find router with id" << router_id_;
        }
    }

    connect(ui.buttonbox, &QDialogButtonBox::clicked, this, &RouterDialog::onButtonBoxClicked);
    connect(ui.button_show_password, &QToolButton::toggled,
            this, &RouterDialog::onShowPasswordButtonToggled);
}

//--------------------------------------------------------------------------------------------------
RouterDialog::~RouterDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        reject();
        return;
    }

    QString address_text = ui.edit_address->text();
    base::Address address = base::Address::fromString(address_text, DEFAULT_ROUTER_TCP_PORT);
    if (!address.isValid())
    {
        LOG(ERROR) << "Invalid router address entered";
        showError(tr("An invalid router address was entered."));
        ui.edit_address->setFocus();
        ui.edit_address->selectAll();
        return;
    }

    QString username = ui.edit_username->text();
    if (!base::User::isValidUserName(username))
    {
        LOG(ERROR) << "Invalid user name entered";
        showError(tr("The user name can not be empty and can contain only"
                     " alphabet characters, numbers and ""_"", ""-"", ""."" characters."));
        ui.edit_username->setFocus();
        ui.edit_username->selectAll();
        return;
    }

    QString password = ui.edit_password->text();
    if (!base::User::isValidPassword(password))
    {
        LOG(ERROR) << "Invalid password entered";
        showError(tr("Password cannot be empty."));
        ui.edit_password->setFocus();
        ui.edit_password->selectAll();
        return;
    }

    RouterData data;
    data.id = router_id_;
    data.name = ui.edit_name->text();
    data.address = address.host();
    data.port = address.port();
    data.session_type = ui.combo_session_type->currentData().toUInt();
    data.username = username;
    data.password = password;

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
void RouterDialog::onShowPasswordButtonToggled(bool checked)
{
    if (checked)
    {
        ui.edit_password->setEchoMode(QLineEdit::Normal);
        ui.edit_password->setInputMethodHints(Qt::ImhNone);
    }
    else
    {
        ui.edit_password->setEchoMode(QLineEdit::Password);
        ui.edit_password->setInputMethodHints(Qt::ImhHiddenText | Qt::ImhSensitiveData |
                                              Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterDialog::showError(const QString& message)
{
    common::MsgBox::warning(this, message);
}

} // namespace client
