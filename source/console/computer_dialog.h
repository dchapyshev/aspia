//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CONSOLE__COMPUTER_DIALOG_H_
#define ASPIA_CONSOLE__COMPUTER_DIALOG_H_

#include "base/macros_magic.h"
#include "protocol/address_book.pb.h"
#include "ui_computer_dialog.h"

namespace aspia {

class ComputerGroupItem;

class ComputerDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { CreateComputer, ModifyComputer };

    ComputerDialog(QWidget* parent,
                   Mode mode,
                   proto::address_book::Computer* computer,
                   proto::address_book::ComputerGroup* parent_computer_group);
    ~ComputerDialog() = default;

private slots:
    void sessionTypeChanged(int item_index);
    void showPasswordButtonToggled(bool checked);
    void sessionConfigButtonPressed();
    void buttonBoxClicked(QAbstractButton* button);

private:
    void showError(const QString& message);

    Ui::ComputerDialog ui;

    const Mode mode_;
    proto::address_book::Computer* computer_;

    DISALLOW_COPY_AND_ASSIGN(ComputerDialog);
};

} // namespace aspia

#endif // ASPIA_CONSOLE__COMPUTER_DIALOG_H_
