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

#ifndef CLIENT_DESKTOP_HOSTS_ROUTER_USERS_WIDGET_H
#define CLIENT_DESKTOP_HOSTS_ROUTER_USERS_WIDGET_H

#include <memory>

#include "client/desktop/hosts/content_widget.h"

namespace Ui {
class RouterUsersWidget;
} // namespace Ui

class QLabel;
class QStatusBar;
class User;

namespace proto::router {
class UserList;
class UserResult;
} // namespace proto::router

class RouterUsersWidget final : public ContentWidget
{
    Q_OBJECT

public:
    explicit RouterUsersWidget(QWidget* parent = nullptr);
    ~RouterUsersWidget() final;

    void showRouter(qint64 router_id);
    qint64 routerId() const { return router_id_; }
    bool hasSelectedUser() const;

    // ContentWidget implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    bool canReload() const final { return true; }
    void reload() final;
    void activate(QStatusBar* statusbar) final;
    void deactivate(QStatusBar* statusbar) final;

public slots:
    void onAddUser();
    void onModifyUser();
    void onDeleteUser();

signals:
    void sig_currentChanged();
    void sig_userContextMenu(const User& user, const QPoint& global_pos);

protected:
    // QObject implementation.
    bool eventFilter(QObject* watched, QEvent* event) final;

private slots:
    void onUserListReceived(const proto::router::UserList& list);
    void onUserResultReceived(const proto::router::UserResult& result);
    void onUserContextMenu(const QPoint& pos);
    void onHeaderContextMenu(const QPoint& pos);

private:
    void fetchUsers();
    void updateStatusLabel();

    std::unique_ptr<Ui::RouterUsersWidget> ui;
    qint64 router_id_ = 0;
    QLabel* status_users_label_ = nullptr;

    Q_DISABLE_COPY_MOVE(RouterUsersWidget)
};

#endif // CLIENT_DESKTOP_HOSTS_ROUTER_USERS_WIDGET_H
