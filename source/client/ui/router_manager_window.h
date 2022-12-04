//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_UI_ROUTER_MANAGER_WINDOW_H
#define CLIENT_UI_ROUTER_MANAGER_WINDOW_H

#include "client/router_config.h"
#include "client/router_window.h"

#include <QMainWindow>
#include <QPointer>

namespace Ui {
class RouterManagerWindow;
} // namespace Ui

class QTreeWidgetItem;

namespace client {

class RouterProxy;
class RouterWindowProxy;
class RouterStatusDialog;

class RouterManagerWindow
    : public QMainWindow,
      public RouterWindow
{
    Q_OBJECT

public:
    explicit RouterManagerWindow(QWidget* parent = nullptr);
    ~RouterManagerWindow() override;

    void connectToRouter(const RouterConfig& router_config);

    // RouterWindow implementation.
    void onConnected(const base::Version& peer_version) override;
    void onDisconnected(base::NetworkChannel::ErrorCode error_code) override;
    void onAccessDenied(base::ClientAuthenticator::ErrorCode error_code) override;
    void onSessionList(std::shared_ptr<proto::SessionList> session_list) override;
    void onSessionResult(std::shared_ptr<proto::SessionResult> session_result) override;
    void onUserList(std::shared_ptr<proto::UserList> user_list) override;
    void onUserResult(std::shared_ptr<proto::UserResult> user_result) override;

protected:
    // QMainWindow implementation.
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onHostsContextMenu(const QPoint& pos);
    void onRelaysContextMenu(const QPoint& pos);
    void onUsersContextMenu(const QPoint& pos);

private:
    void refreshSessionList();
    void disconnectHost();
    void disconnectAllHosts();
    void refreshUserList();
    void addUser();
    void modifyUser();
    void deleteUser();
    void onCurrentUserChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onCurrentHostChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

    void beforeRequest();
    void afterRequest();

    void saveHostsToFile();
    void saveRelaysToFile();

    std::unique_ptr<Ui::RouterManagerWindow> ui;

    QString peer_address_;
    uint16_t peer_port_ = 0;

    RouterStatusDialog* status_dialog_;

    std::shared_ptr<RouterWindowProxy> window_proxy_;
    std::unique_ptr<RouterProxy> router_proxy_;

    DISALLOW_COPY_AND_ASSIGN(RouterManagerWindow);
};

} // namespace client

#endif // CLIENT_UI_ROUTER_MANAGER_WINDOW_H
