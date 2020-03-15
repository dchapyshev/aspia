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

#include "crypto/key_pair.h"

#include <QAbstractButton>
#include <QMessageBox>

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
    crypto::KeyPair key_pair = crypto::KeyPair::create(crypto::KeyPair::Type::X25519);
    if (!key_pair.isValid())
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Error generating keys."),
                             QMessageBox::Ok);
        return;
    }

    ui.edit_private_key->setPlainText(
        QString::fromStdString(base::toHex(key_pair.privateKey())));
    ui.edit_public_key->setPlainText(
        QString::fromStdString(base::toHex(key_pair.publicKey())));
}

void ProxyDialog::onCheckKeys()
{
    base::ByteArray private_key = base::fromHex(ui.edit_private_key->toPlainText().toStdString());
    if (private_key.empty())
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Private key not entered."),
                             QMessageBox::Ok);
        return;
    }

    base::ByteArray public_key = base::fromHex(ui.edit_public_key->toPlainText().toStdString());
    if (public_key.empty())
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Public key not entered."),
                             QMessageBox::Ok);
        return;
    }

    crypto::KeyPair key_pair = crypto::KeyPair::fromPrivateKey(private_key);
    if (!key_pair.isValid())
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Invalid private key entered."),
                             QMessageBox::Ok);
        return;
    }

    if (base::compare(public_key, key_pair.publicKey()) != 0)
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("An invalid key pair has been entered."),
                             QMessageBox::Ok);
        return;
    }

    QMessageBox::information(this,
                             tr("Information"),
                             tr("The correct key pair has been entered."),
                             QMessageBox::Ok);
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
