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

#ifndef CLIENT_UI_HOSTS_LOCAL_GROUP_DIALOG_H
#define CLIENT_UI_HOSTS_LOCAL_GROUP_DIALOG_H

#include <QDialog>

#include "ui_local_group_dialog.h"

namespace client {

class LocalGroupDialog : public QDialog
{
    Q_OBJECT

public:
    LocalGroupDialog(qint64 group_id, qint64 parent_id, QWidget* parent = nullptr);
    ~LocalGroupDialog() override;

private slots:
    void onButtonBoxClicked(QAbstractButton* button);

private:
    Ui::LocalGroupDialog ui;
    qint64 group_id_ = -1;
    qint64 parent_id_ = 0;

    Q_DISABLE_COPY_MOVE(LocalGroupDialog)
};

} // namespace client

#endif // CLIENT_UI_HOSTS_LOCAL_GROUP_DIALOG_H
