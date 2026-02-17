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

#ifndef CONSOLE_COMPUTER_GROUP_DIALOG_H
#define CONSOLE_COMPUTER_GROUP_DIALOG_H

#include "console/computer_group_item.h"
#include "console/settings.h"
#include "ui_computer_group_dialog.h"

namespace console {

class ComputerGroupDialog final : public QDialog
{
    Q_OBJECT

public:
    enum Mode { CreateComputerGroup, ModifyComputerGroup };

    ComputerGroupDialog(QWidget* parent,
                        Mode mode,
                        const QString& parent_name,
                        proto::address_book::ComputerGroup* computer_group);
    ~ComputerGroupDialog() final;

protected:
    // QDialog implementation.
    void closeEvent(QCloseEvent* event) final;
    bool eventFilter(QObject* watched, QEvent* event) final;
    void keyPressEvent(QKeyEvent* event) final;

private slots:
    void buttonBoxClicked(QAbstractButton* button);
    void onTabChanged(QTreeWidgetItem* current);

private:
    void showError(const QString& message);
    void showTab(int type);
    bool saveChanges();

    Ui::ComputerGroupDialog ui;

    Settings settings_;

    const Mode mode_;
    proto::address_book::ComputerGroup* computer_group_;
    QWidgetList tabs_;

    Q_DISABLE_COPY(ComputerGroupDialog)
};

} // namespace console

#endif // CONSOLE_COMPUTER_GROUP_DIALOG_H
