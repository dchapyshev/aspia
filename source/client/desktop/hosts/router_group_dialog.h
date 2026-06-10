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

#ifndef CLIENT_DESKTOP_HOSTS_ROUTER_GROUP_DIALOG_H
#define CLIENT_DESKTOP_HOSTS_ROUTER_GROUP_DIALOG_H

#include <QDialog>

#include <memory>

#include "client/router.h"

class QAbstractButton;

namespace Ui {
class RouterGroupDialog;
} // namespace Ui

namespace proto::router {
class GroupResult;
} // namespace proto::router

class RouterGroupDialog final : public QDialog
{
    Q_OBJECT

public:
    // entry_id == 0 means add mode (the new group's parent defaults to default_parent_id);
    // entry_id > 0 means modify mode (the existing group's name/parent/comment populate the
    // form and default_parent_id is ignored).
    RouterGroupDialog(qint64 router_id, qint64 workspace_id, const QString& workspace_name,
                      qint64 entry_id, qint64 default_parent_id, QWidget* parent);
    ~RouterGroupDialog() final;

private slots:
    void onGroupListReceived(const Router::GroupList& list);
    void onGroupResultReceived(const proto::router::GroupResult& result);

private:
    void onButtonBoxClicked(QAbstractButton* button);

    std::unique_ptr<Ui::RouterGroupDialog> ui;
    qint64 router_id_ = 0;
    qint64 workspace_id_ = 0;
    QString workspace_name_;
    qint64 entry_id_ = 0;
    qint64 default_parent_id_ = 0;

    Q_DISABLE_COPY_MOVE(RouterGroupDialog)
};

#endif // CLIENT_DESKTOP_HOSTS_ROUTER_GROUP_DIALOG_H
