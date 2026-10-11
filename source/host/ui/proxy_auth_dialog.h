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

#ifndef HOST_UI_PROXY_AUTH_DIALOG_H
#define HOST_UI_PROXY_AUTH_DIALOG_H

#include <QDialog>

#include <memory>

#include "base/net/proxy_settings.h"

class QAbstractButton;

namespace Ui {
class ProxyAuthDialog;
} // namespace Ui

class ProxyAuthDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ProxyAuthDialog(const ProxySettings& proxy, QWidget* parent = nullptr);
    ~ProxyAuthDialog() final;

    // The proxy of the constructor with the user name and the password entered.
    ProxySettings proxy() const;

private slots:
    void onButtonBoxClicked(QAbstractButton* button);

private:
    std::unique_ptr<Ui::ProxyAuthDialog> ui;
    const ProxySettings proxy_;

    Q_DISABLE_COPY_MOVE(ProxyAuthDialog)
};

#endif // HOST_UI_PROXY_AUTH_DIALOG_H
