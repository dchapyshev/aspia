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

#ifndef ROUTER__UI__PROXY_DIALOG_H
#define ROUTER__UI__PROXY_DIALOG_H

#include "base/macros_magic.h"
#include "ui_proxy_dialog.h"

namespace router {

class ProxyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProxyDialog(QWidget* parent = nullptr);
    ~ProxyDialog();

    void setAddress(const QString& address);
    QString address() const;
    void setPort(uint16_t port);
    uint16_t port() const;
    void setReconnectTimeout(int timeout);
    int reconnectTimeout() const;
    void setPrivateKey(const QString& private_key);
    QString privateKey() const;
    void setPublicKey(const QString& public_key);
    QString publicKey() const;
    void setEnabled(bool enable);
    bool isEnabled() const;

private:
    void onCreateKeys();
    void onCheckKeys();
    void onButtonBoxClicked(QAbstractButton* button);

    Ui::ProxyDialog ui;

    DISALLOW_COPY_AND_ASSIGN(ProxyDialog);
};

} // namespace router

#endif // ROUTER__UI__PROXY_DIALOG_H
