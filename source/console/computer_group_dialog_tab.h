//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CONSOLE_COMPUTER_GROUP_DIALOG_TAB_H
#define CONSOLE_COMPUTER_GROUP_DIALOG_TAB_H

#include <QWidget>

namespace console {

class ComputerGroupDialogTab : public QWidget
{
public:
    virtual ~ComputerGroupDialogTab() override = default;

    int type() const { return type_; }

protected:
    ComputerGroupDialogTab(int type, QWidget* parent)
        : QWidget(parent),
          type_(type)
    {
        // Nothing
    }

private:
    const int type_;
};

} // namespace console

#endif // CONSOLE_COMPUTER_GROUP_DIALOG_TAB_H
