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

#include "router/ui/connect_dialog.h"

namespace router {

ConnectDialog::ConnectDialog()
{
    ui.setupUi(this);
}

ConnectDialog::~ConnectDialog()
{

}

QString ConnectDialog::address() const
{
    return ui.combobox_address->currentText();
}

uint16_t ConnectDialog::port() const
{
    return ui.spinbox_port->value();
}

QString ConnectDialog::userName() const
{
    return ui.edit_username->text();
}

QString ConnectDialog::password() const
{
    return ui.edit_password->text();
}

} // namespace router
