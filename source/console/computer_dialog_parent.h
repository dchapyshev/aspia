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

#ifndef CONSOLE_COMPUTER_DIALOG_PARENT_H
#define CONSOLE_COMPUTER_DIALOG_PARENT_H

#include "console/computer_dialog_tab.h"
#include "ui_computer_dialog_parent.h"

namespace console {

class ComputerDialogParent final : public ComputerDialogTab
{
    Q_OBJECT

public:
    ComputerDialogParent(int type, QWidget* parent);
    ~ComputerDialogParent() final = default;

private:
    Ui::ComputerDialogParent ui;
    Q_DISABLE_COPY(ComputerDialogParent)
};

} // namespace console

#endif // CONSOLE_COMPUTER_DIALOG_PARENT_H
