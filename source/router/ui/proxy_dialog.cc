//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "router/ui/proxy_dialog.h"

#include <QAbstractButton>

namespace router {

ProxyDialog::ProxyDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    connect(ui.button_create_keys, &QPushButton::released, this, &ProxyDialog::onCreateKeys);
    connect(ui.button_check_keys, &QPushButton::released, this, &ProxyDialog::onCheckKeys);
    connect(ui.buttonbox, &QDialogButtonBox::clicked, this, &ProxyDialog::onButtonBoxClicked);
}

ProxyDialog::~ProxyDialog() = default;

void ProxyDialog::setAddress(const QString& address)
{
    ui.edit_address->setText(address);
}

QString ProxyDialog::address() const
{
    return ui.edit_address->text();
}

void ProxyDialog::setPort(uint16_t port)
{
    ui.spinbox_port->setValue(port);
}

uint16_t ProxyDialog::port() const
{
    return ui.spinbox_port->value();
}

void ProxyDialog::setReconnectTimeout(int timeout)
{
    ui.spinbox_reconnect_timeout->setValue(timeout);
}

int ProxyDialog::reconnectTimeout() const
{
    return ui.spinbox_reconnect_timeout->value();
}

void ProxyDialog::setPrivateKey(const QString& private_key)
{
    ui.edit_private_key->setPlainText(private_key);
}

QString ProxyDialog::privateKey() const
{
    return ui.edit_private_key->toPlainText();
}

void ProxyDialog::setPublicKey(const QString& public_key)
{
    ui.edit_public_key->setPlainText(public_key);
}

QString ProxyDialog::publicKey() const
{
    return ui.edit_public_key->toPlainText();
}

void ProxyDialog::setEnabled(bool enable)
{
    ui.checkbox_enable->setChecked(enable);
}

bool ProxyDialog::isEnabled() const
{
    return ui.checkbox_enable->isChecked();
}

void ProxyDialog::onCreateKeys()
{

}

void ProxyDialog::onCheckKeys()
{

}

void ProxyDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
    if (standard_button == QDialogButtonBox::Ok)
    {
        accept();
    }
    else
    {
        reject();
    }

    close();
}

} // namespace router
