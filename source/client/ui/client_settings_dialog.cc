//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/net/address.h"
#include "base/peer/user.h"
#include "client/router_config_storage.h"

#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

namespace client {

//--------------------------------------------------------------------------------------------------
ClientSettingsDialog::ClientSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
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
    ui.edit_address->setText(QString::fromStdU16String(address.toString()));
    ui.edit_username->setText(QString::fromStdU16String(config.username));
    ui.edit_password->setText(QString::fromStdU16String(config.password));

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
        ui.label_address->setEnabled(checked);
        ui.edit_address->setEnabled(checked);

        ui.label_username->setEnabled(checked);
        ui.edit_username->setEnabled(checked);

        ui.label_password->setEnabled(checked);
        ui.edit_password->setEnabled(checked);
    });

    connect(ui.buttonbox, &QDialogButtonBox::clicked,
            this, &ClientSettingsDialog::onButtonBoxClicked);

    QTimer::singleShot(0, this, &ClientSettingsDialog::adjustSize);
}

//--------------------------------------------------------------------------------------------------
ClientSettingsDialog::~ClientSettingsDialog() = default;

//--------------------------------------------------------------------------------------------------
void ClientSettingsDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        reject();
    }
    else
    {
        base::Address address = base::Address::fromString(
            ui.edit_address->text().toStdU16String(), DEFAULT_ROUTER_TCP_PORT);
        if (!address.isValid())
        {
            showError(tr("An invalid router address was entered."));
            ui.edit_address->setFocus();
            ui.edit_address->selectAll();
            return;
        }

        std::u16string username = ui.edit_username->text().toStdU16String();
        std::u16string password = ui.edit_password->text().toStdU16String();

        if (!base::User::isValidUserName(username))
        {
            showError(tr("The user name can not be empty and can contain only"
                         " alphabet characters, numbers and ""_"", ""-"", ""."" characters."));
            ui.edit_username->setFocus();
            ui.edit_username->selectAll();
            return;
        }

        if (!base::User::isValidPassword(password))
        {
            showError(tr("Password cannot be empty."));
            ui.edit_password->setFocus();
            ui.edit_password->selectAll();
            return;
        }

        RouterConfig config;
        config.address = address.host();
        config.port = address.port();
        config.username = std::move(username);
        config.password = std::move(password);

        RouterConfigStorage config_storage;
        config_storage.setEnabled(ui.checkbox_enable_router->isChecked());
        config_storage.setRouterConfig(config);

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
