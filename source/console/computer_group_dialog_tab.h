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

#ifndef CONSOLE_COMPUTER_GROUP_DIALOG_TAB_H
#define CONSOLE_COMPUTER_GROUP_DIALOG_TAB_H

#include <QWidget>

namespace console {

class ComputerGroupDialogTab : public QWidget
{
public:
    virtual ~ComputerGroupDialogTab() override = default;

    bool isRootGroup() const { return is_root_group_; }
    int type() const { return type_; }

protected:
    ComputerGroupDialogTab(int type, bool is_root_group, QWidget* parent)
        : QWidget(parent),
          is_root_group_(is_root_group),
          type_(type)
    {
        // Nothing
    }

private:
    const bool is_root_group_;
    const int type_;
};

} // namespace console

#endif // CONSOLE_COMPUTER_GROUP_DIALOG_TAB_H
