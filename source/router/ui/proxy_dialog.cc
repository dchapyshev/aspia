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
}

} // namespace router
