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

#ifndef CLIENT_DESKTOP_UPDATE_SERVER_DIALOG_H
#define CLIENT_DESKTOP_UPDATE_SERVER_DIALOG_H

#include <QDialog>

#include <memory>

namespace Ui {
class UpdateServerDialog;
} // namespace Ui

class QAbstractButton;

// Edits the update server of SystemSettings. Saving takes the privileges of an administrator.
class UpdateServerDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateServerDialog(QWidget* parent = nullptr);
    ~UpdateServerDialog() final;

private slots:
    void onButtonBoxClicked(QAbstractButton* button);

private:
    std::unique_ptr<Ui::UpdateServerDialog> ui;

    Q_DISABLE_COPY_MOVE(UpdateServerDialog)
};

#endif // CLIENT_DESKTOP_UPDATE_SERVER_DIALOG_H
