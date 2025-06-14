//
// Aspia Project
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

#include "console/computer_group_dialog_port_forwarding.h"

#include "base/logging.h"

namespace console {

//--------------------------------------------------------------------------------------------------
ComputerGroupDialogPortForwarding::ComputerGroupDialogPortForwarding(
    int type, bool is_root_group, QWidget* parent)
    : ComputerGroupDialogTab(type, is_root_group, parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);
}

//--------------------------------------------------------------------------------------------------
ComputerGroupDialogPortForwarding::~ComputerGroupDialogPortForwarding()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupDialogPortForwarding::restoreSettings(
    const proto::address_book::ComputerGroupConfig& group_config)
{
    proto::port_forwarding::Config config;

    ui.checkbox_inherit_config->setChecked(group_config.inherit().port_forwarding());
    config = group_config.session_config().port_forwarding();

    ui.spinbox_local_port->setValue(config.local_port());
    ui.edit_command_line->setPlainText(QString::fromStdString(config.command_line()));
    ui.spinbox_remote_port->setValue(config.remote_port());
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupDialogPortForwarding::saveSettings(
    proto::address_book::ComputerGroupConfig* group_config)
{
    proto::port_forwarding::Config* config;

    config = group_config->mutable_session_config()->mutable_port_forwarding();
    group_config->mutable_inherit()->set_desktop_manage(ui.checkbox_inherit_config->isChecked());

    config->set_local_port(ui.spinbox_local_port->value());
    config->set_command_line(ui.edit_command_line->toPlainText().toStdString());
    config->set_remote_port(ui.spinbox_remote_port->value());
}

} // namespace console
