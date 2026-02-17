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

#ifndef CONSOLE_COMPUTER_DIALOG_DESKTOP_H
#define CONSOLE_COMPUTER_DIALOG_DESKTOP_H

#include "console/computer_dialog_tab.h"
#include "proto/address_book.h"
#include "ui_computer_dialog_desktop.h"

namespace console {

class ComputerDialogDesktop final : public ComputerDialogTab
{
    Q_OBJECT

public:
    ComputerDialogDesktop(int type, QWidget* parent);
    ~ComputerDialogDesktop() final = default;

    void restoreSettings(proto::peer::SessionType session_type,
        const proto::address_book::Computer& computer);
    void saveSettings(proto::peer::SessionType session_type, proto::address_book::Computer* computer);

private slots:
    void onCodecChanged(int item_index);
    void onCompressionRatioChanged(int value);

private:
    Ui::ComputerDialogDesktop ui;
    Q_DISABLE_COPY(ComputerDialogDesktop)
};

} // namespace console

#endif // CONSOLE_COMPUTER_DIALOG_DESKTOP_H
