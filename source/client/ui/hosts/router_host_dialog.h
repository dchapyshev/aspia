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

#ifndef CLIENT_UI_HOSTS_ROUTER_HOST_DIALOG_H
#define CLIENT_UI_HOSTS_ROUTER_HOST_DIALOG_H

#include <QDialog>

#include <memory>

#include "client/router.h"

class QAbstractButton;

namespace Ui {
class RouterHostDialog;
} // namespace Ui

namespace proto::router {
class HostResult;
} // namespace proto::router

class RouterHostDialog final : public QDialog
{
    Q_OBJECT

public:
    RouterHostDialog(qint64 router_id, const QString& workspace_name, const Router::Host& host,
                     QWidget* parent);
    ~RouterHostDialog() final;

private slots:
    void onGroupListReceived(const Router::GroupList& list);
    void onHostResultReceived(const proto::router::HostResult& result);

private:
    void onButtonBoxClicked(QAbstractButton* button);

    std::unique_ptr<Ui::RouterHostDialog> ui;
    qint64 router_id_ = 0;
    QString workspace_name_;
    Router::Host host_;

    Q_DISABLE_COPY_MOVE(RouterHostDialog)
};

#endif // CLIENT_UI_HOSTS_ROUTER_HOST_DIALOG_H
