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

#ifndef CONSOLE_COMPUTER_GROUP_DIALOG_PORT_FORWARDING_H
#define CONSOLE_COMPUTER_GROUP_DIALOG_PORT_FORWARDING_H

#include "console/computer_group_dialog_tab.h"
#include "proto/address_book.h"
#include "ui_computer_group_dialog_port_forwarding.h"

namespace console {

class ComputerGroupDialogPortForwarding final : public ComputerGroupDialogTab
{
    Q_OBJECT

public:
    ComputerGroupDialogPortForwarding(int type, bool is_root_group, QWidget* parent);
    ~ComputerGroupDialogPortForwarding() final;

    void restoreSettings(const proto::address_book::ComputerGroupConfig& group_config);
    void saveSettings(proto::address_book::ComputerGroupConfig* group_config);

private:
    Ui::ComputerGroupDialogPortForwarding ui;
    Q_DISABLE_COPY(ComputerGroupDialogPortForwarding)
};

} // namespace console

#endif // CONSOLE_COMPUTER_GROUP_DIALOG_PORT_FORWARDING_H
