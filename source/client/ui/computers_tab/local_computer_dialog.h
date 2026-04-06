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

#ifndef CLIENT_UI_COMPUTERS_TAB_LOCAL_COMPUTER_DIALOG_H
#define CLIENT_UI_COMPUTERS_TAB_LOCAL_COMPUTER_DIALOG_H

#include <QDialog>

#include "client/local_data.h"
#include "ui_local_computer_dialog.h"

namespace client {

class LocalComputerDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { ADD, EDIT };

    LocalComputerDialog(Mode mode, QWidget* parent = nullptr);
    ~LocalComputerDialog() override;

    void setComputer(const ComputerData& computer);
    ComputerData computer() const;

private slots:
    void onShowPasswordButtonToggled(bool checked);
    void onButtonBoxClicked(QAbstractButton* button);

private:
    Ui::LocalComputerDialog ui;
    Mode mode_;
    qint64 computer_id_ = -1;
    qint64 group_id_ = 0;

    Q_DISABLE_COPY_MOVE(LocalComputerDialog)
};

} // namespace client

#endif // CLIENT_UI_COMPUTERS_TAB_LOCAL_COMPUTER_DIALOG_H
