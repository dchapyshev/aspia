//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "console/computer_dialog_port_forwarding.h"

namespace console {

//--------------------------------------------------------------------------------------------------
ComputerDialogPortForwarding::ComputerDialogPortForwarding(int type, QWidget* parent)
    : ComputerDialogTab(type, parent)
{
    ui.setupUi(this);
}

//--------------------------------------------------------------------------------------------------
void ComputerDialogPortForwarding::restoreSettings(const proto::address_book::Computer& computer)
{
    proto::port_forwarding::Config config;

    ui.checkbox_inherit_config->setChecked(computer.inherit().port_forwarding());
    config = computer.session_config().port_forwarding();

    ui.spinbox_local_port->setValue(config.local_port());
    ui.edit_command_line->setPlainText(QString::fromStdString(config.command_line()));
    ui.spinbox_remote_port->setValue(config.remote_port());
}

//--------------------------------------------------------------------------------------------------
void ComputerDialogPortForwarding::saveSettings(proto::address_book::Computer* computer)
{
    proto::port_forwarding::Config* config;

    computer->mutable_inherit()->set_port_forwarding(ui.checkbox_inherit_config->isChecked());
    config = computer->mutable_session_config()->mutable_port_forwarding();

    config->set_local_port(ui.spinbox_local_port->value());
    config->set_command_line(ui.edit_command_line->toPlainText().toStdString());
    config->set_remote_port(ui.spinbox_remote_port->value());
}

} // namespace console
