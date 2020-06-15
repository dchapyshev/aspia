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

#ifndef CONSOLE__COMPUTER_GROUP_DIALOG_H
#define CONSOLE__COMPUTER_GROUP_DIALOG_H

#include "console/computer_group_item.h"
#include "console/settings.h"
#include "ui_computer_group_dialog.h"

namespace console {

class ComputerGroupDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { CreateComputerGroup, ModifyComputerGroup };

    ComputerGroupDialog(QWidget* parent,
                        Mode mode,
                        const QString& parent_name,
                        proto::address_book::ComputerGroup* computer_group);
    ~ComputerGroupDialog() = default;

protected:
    // QDialog implementation.
    void closeEvent(QCloseEvent* event) override;

private slots:
    void buttonBoxClicked(QAbstractButton* button);

private:
    void showError(const QString& message);

    Ui::ComputerGroupDialog ui;

    Settings settings_;

    const Mode mode_;
    proto::address_book::ComputerGroup* computer_group_;

    DISALLOW_COPY_AND_ASSIGN(ComputerGroupDialog);
};

} // namespace console

#endif // CONSOLE__COMPUTER_GROUP_DIALOG_H
