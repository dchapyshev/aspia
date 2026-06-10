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

#ifndef CLIENT_DESKTOP_HOSTS_LOCAL_GROUP_DIALOG_H
#define CLIENT_DESKTOP_HOSTS_LOCAL_GROUP_DIALOG_H

#include <QDialog>

#include <memory>

class QAbstractButton;

namespace Ui {
class LocalGroupDialog;
} // namespace Ui

class LocalGroupDialog final : public QDialog
{
    Q_OBJECT

public:
    LocalGroupDialog(qint64 group_id, qint64 parent_id, QWidget* parent = nullptr);
    ~LocalGroupDialog() final;

private slots:
    void onButtonBoxClicked(QAbstractButton* button);

private:
    std::unique_ptr<Ui::LocalGroupDialog> ui;
    qint64 group_id_ = -1;
    qint64 parent_id_ = 0;

    Q_DISABLE_COPY_MOVE(LocalGroupDialog)
};

#endif // CLIENT_DESKTOP_HOSTS_LOCAL_GROUP_DIALOG_H
