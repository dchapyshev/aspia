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

#ifndef CONSOLE__COMPUTER_DIALOG_H
#define CONSOLE__COMPUTER_DIALOG_H

#include "base/macros_magic.h"
#include "console/console_settings.h"
#include "proto/address_book.pb.h"
#include "ui_computer_dialog.h"

#include <QDialog>

class QAbstractButton;

namespace console {

class ComputerDialog : public QDialog
{
    Q_OBJECT

public:
    ComputerDialog(QWidget* parent,
                   const QString& parent_name,
                   const proto::address_book::Computer& computer);
    ComputerDialog(QWidget* parent, const QString& parent_name);
    ~ComputerDialog();

    const proto::address_book::Computer& computer() const { return computer_; }

protected:
    // QDialog implementation.
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onTabChanged(QTreeWidgetItem* current);
    void buttonBoxClicked(QAbstractButton* button);

private:
    void init(const QString& parent_name);
    void showTab(int type);

    enum class Mode { CREATE_COMPUTER, MODIFY_COMPUTER };

    Ui::ComputerDialog ui;
    QWidgetList tabs_;
    const Mode mode_;

    Settings settings_;

    proto::address_book::Computer computer_;

    DISALLOW_COPY_AND_ASSIGN(ComputerDialog);
};

} // namespace console

#endif // CONSOLE__COMPUTER_DIALOG_H
