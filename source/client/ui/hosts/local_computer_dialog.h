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

#ifndef CLIENT_UI_HOSTS_LOCAL_COMPUTER_DIALOG_H
#define CLIENT_UI_HOSTS_LOCAL_COMPUTER_DIALOG_H

#include <QDialog>

#include <memory>

class QAbstractButton;

namespace Ui {
class LocalComputerDialog;
} // namespace Ui

class LocalComputerDialog final : public QDialog
{
    Q_OBJECT

public:
    LocalComputerDialog(qint64 computer_id, qint64 group_id, QWidget* parent = nullptr);
    ~LocalComputerDialog() final;

    qint64 computerId() const { return computer_id_; }

private slots:
    void onShowPasswordButtonToggled(bool checked);
    void onRouterChanged(int index);
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void updateAddressLabel();

    std::unique_ptr<Ui::LocalComputerDialog> ui;
    qint64 computer_id_ = -1;
    qint64 group_id_ = 0;

    Q_DISABLE_COPY_MOVE(LocalComputerDialog)
};

#endif // CLIENT_UI_HOSTS_LOCAL_COMPUTER_DIALOG_H
