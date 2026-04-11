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
#include <QMessageBox>
#include <QToolButton>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/net/address.h"
#include "base/peer/user.h"
#include "build/build_config.h"

namespace client {

//--------------------------------------------------------------------------------------------------
RouterDialog::RouterDialog(QWidget* parent)
    : QDialog(parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    base::GuiApplication::translateButtonBox(ui.buttonbox);

    connect(ui.buttonbox, &QDialogButtonBox::clicked, this, &RouterDialog::onButtonBoxClicked);
    connect(ui.button_show_password, &QToolButton::toggled,
            this, &RouterDialog::onShowPasswordButtonToggled);
}

//--------------------------------------------------------------------------------------------------
RouterDialog::RouterDialog(const RouterConfig& config, QWidget* parent)
    : RouterDialog(parent)
{
    base::Address address(DEFAULT_ROUTER_TCP_PORT);
    address.setHost(config.address);
    address.setPort(config.port);

    ui.edit_address->setText(address.toString());
    ui.edit_username->setText(config.username);
    ui.edit_password->setText(config.password);
}

//--------------------------------------------------------------------------------------------------
RouterDialog::~RouterDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
RouterConfig RouterDialog::routerConfig() const
{
    RouterConfig config;

    base::Address address =
        base::Address::fromString(ui.edit_address->text(), DEFAULT_ROUTER_TCP_PORT);
    config.address = address.host();
    config.port = address.port();
    config.username = ui.edit_username->text();
    config.password = ui.edit_password->text();

    return config;
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
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace client
