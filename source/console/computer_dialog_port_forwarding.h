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

#ifndef CONSOLE_COMPUTER_DIALOG_PORT_FORWARDING_H
#define CONSOLE_COMPUTER_DIALOG_PORT_FORWARDING_H

#include "console/computer_dialog_tab.h"
#include "proto/address_book.h"
#include "ui_computer_dialog_port_forwarding.h"

namespace console {

class ComputerDialogPortForwarding final : public ComputerDialogTab
{
    Q_OBJECT

public:
    ComputerDialogPortForwarding(int type, QWidget* parent);
    ~ComputerDialogPortForwarding() final = default;

    void restoreSettings(const proto::address_book::Computer& computer);
    void saveSettings(proto::address_book::Computer* computer);

private:
    Ui::ComputerDialogPortForwarding ui;
    Q_DISABLE_COPY(ComputerDialogPortForwarding)
};

} // namespace console

#endif // CONSOLE_COMPUTER_DIALOG_PORT_FORWARDING_H
