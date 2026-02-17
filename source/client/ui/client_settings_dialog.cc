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

#include "client/ui/client_settings_dialog.h"

#include "base/logging.h"
#include "base/net/address.h"
#include "base/peer/user.h"
#include "build/build_config.h"
#include "client/router_config_storage.h"
#include "client/ui/client_settings.h"

#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

namespace client {

//--------------------------------------------------------------------------------------------------
ClientSettingsDialog::ClientSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    QPushButton* cancel_button = ui.buttonbox->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    RouterConfigStorage config_storage;
    RouterConfig config = config_storage.routerConfig();

    base::Address address(DEFAULT_ROUTER_TCP_PORT);
    address.setHost(config.address);
    address.setPort(config.port);

    ui.checkbox_enable_router->setChecked(true);
    ui.edit_address->setText(address.toString());
    ui.edit_username->setText(config.username);
    ui.edit_password->setText(config.password);

    ClientSettings settings;
    ui.edit_display_name->setText(settings.displayName());

    if (!config_storage.isEnabled())
    {
        ui.checkbox_enable_router->setChecked(false);

        ui.label_address->setEnabled(false);
        ui.edit_address->setEnabled(false);

        ui.label_username->setEnabled(false);
        ui.edit_username->setEnabled(false);

        ui.label_password->setEnabled(false);
        ui.edit_password->setEnabled(false);
    }

    connect(ui.checkbox_enable_router, &QCheckBox::toggled, this, [this](bool checked)
    {
        LOG(INFO) << "[ACTION] Enable router:" << checked;

        ui.label_address->setEnabled(checked);
        ui.edit_address->setEnabled(checked);

        ui.label_username->setEnabled(checked);
        ui.edit_username->setEnabled(checked);

        ui.label_password->setEnabled(checked);
        ui.edit_password->setEnabled(checked);
    });

    connect(ui.buttonbox, &QDialogButtonBox::clicked,
            this, &ClientSettingsDialog::onButtonBoxClicked);
}

//--------------------------------------------------------------------------------------------------
ClientSettingsDialog::~ClientSettingsDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientSettingsDialog::closeEvent(QCloseEvent* event)
{
    LOG(INFO) << "Close event detected";
    QDialog::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void ClientSettingsDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        reject();
    }
    else
    {
        LOG(INFO) << "[ACTION] Accepted by user";

        bool enable_router = ui.checkbox_enable_router->isChecked();

        QString address_text = ui.edit_address->text();
        base::Address address = base::Address::fromString(address_text, DEFAULT_ROUTER_TCP_PORT);
        if (!address.isValid())
        {
            if (!enable_router && address_text.isEmpty())
            {
                LOG(INFO) << "Router disabled and address is empty";
            }
            else
            {
                LOG(ERROR) << "Invalid router address entered";
                showError(tr("An invalid router address was entered."));
                ui.edit_address->setFocus();
                ui.edit_address->selectAll();
                return;
            }
        }

        QString username = ui.edit_username->text();
        QString password = ui.edit_password->text();

        if (!base::User::isValidUserName(username))
        {
            if (!enable_router && username.isEmpty())
            {
                LOG(INFO) << "Router disabled and username is empty";
            }
            else
            {
                LOG(ERROR) << "Invalid user name entered";
                showError(tr("The user name can not be empty and can contain only"
                             " alphabet characters, numbers and ""_"", ""-"", ""."" characters."));
                ui.edit_username->setFocus();
                ui.edit_username->selectAll();
                return;
            }
        }

        if (!base::User::isValidPassword(password))
        {
            if (!enable_router && password.isEmpty())
            {
                LOG(INFO) << "Router disabled and password is empty";
            }
            else
            {
                LOG(ERROR) << "Invalid password entered";
                showError(tr("Password cannot be empty."));
                ui.edit_password->setFocus();
                ui.edit_password->selectAll();
                return;
            }
        }

        RouterConfig config;
        config.address = address.host();
        config.port = address.port();
        config.username = std::move(username);
        config.password = std::move(password);

        RouterConfigStorage config_storage;
        config_storage.setEnabled(ui.checkbox_enable_router->isChecked());
        config_storage.setRouterConfig(config);

        ClientSettings settings;
        settings.setDisplayName(ui.edit_display_name->text());

        accept();
    }

    close();
}

//--------------------------------------------------------------------------------------------------
void ClientSettingsDialog::showError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace client
