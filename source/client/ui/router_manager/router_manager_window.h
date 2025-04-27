//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_UI_ROUTER_MANAGER_ROUTER_MANAGER_WINDOW_H
#define CLIENT_UI_ROUTER_MANAGER_ROUTER_MANAGER_WINDOW_H

#include "client/router_config.h"
#include "client/router_window.h"

#include <QMainWindow>
#include <QPointer>

namespace Ui {
class RouterManagerWindow;
} // namespace Ui

class QTreeWidget;
class QTreeWidgetItem;

namespace common {
class StatusDialog;
} // namespace common

namespace client {

class RouterProxy;
class RouterWindowProxy;

class RouterManagerWindow final
    : public QMainWindow,
      public RouterWindow
{
    Q_OBJECT

public:
    explicit RouterManagerWindow(QWidget* parent = nullptr);
    ~RouterManagerWindow() final;

    void connectToRouter(const RouterConfig& router_config);

    // RouterWindow implementation.
    void onConnecting() final;
    void onConnected(const base::Version& peer_version) final;
    void onDisconnected(base::TcpChannel::ErrorCode error_code) final;
    void onWaitForRouter() final;
    void onWaitForRouterTimeout() final;
    void onVersionMismatch(const base::Version& router, const base::Version& client) final;
    void onAccessDenied(base::Authenticator::ErrorCode error_code) final;
    void onSessionList(std::shared_ptr<proto::SessionList> session_list) final;
    void onSessionResult(std::shared_ptr<proto::SessionResult> session_result) final;
    void onUserList(std::shared_ptr<proto::UserList> user_list) final;
    void onUserResult(std::shared_ptr<proto::UserResult> user_result) final;

    static QString delayToString(uint64_t delay);
    static QString sizeToString(int64_t size);

protected:
    // QMainWindow implementation.
    void closeEvent(QCloseEvent* event) final;

private slots:
    void onHostsContextMenu(const QPoint& pos);
    void onRelaysContextMenu(const QPoint& pos);
    void onActiveConnContextMenu(const QPoint& pos);
    void onUsersContextMenu(const QPoint& pos);

private:
    void refreshSessionList();
    void disconnectRelay();
    void disconnectAllRelays();
    void disconnectHost();
    void disconnectAllHosts();
    void refreshUserList();
    void addUser();
    void modifyUser();
    void deleteUser();
    void onCurrentUserChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onCurrentHostChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onCurrentRelayChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onContextMenuForTreeHeader(QTreeWidget* tree, const QPoint& pos);
    void copyRowFromTree(QTreeWidgetItem* item);
    void copyColumnFromTree(QTreeWidgetItem* item, int column);
    void updateRelayStatistics();

    void beforeRequest();
    void afterRequest();

    void saveHostsToFile();
    void saveRelaysToFile();

    QByteArray saveState();
    void restoreState(const QByteArray& state);

    std::unique_ptr<Ui::RouterManagerWindow> ui;

    QString peer_address_;
    uint16_t peer_port_ = 0;
    bool is_connected_ = false;

    common::StatusDialog* status_dialog_;

    std::shared_ptr<RouterWindowProxy> window_proxy_;
    std::unique_ptr<RouterProxy> router_proxy_;

    int current_hosts_column_ = 0;
    int current_relays_column_ = 0;
    int current_active_conn_column_ = 0;

    DISALLOW_COPY_AND_ASSIGN(RouterManagerWindow);
};

} // namespace client

#endif // CLIENT_UI_ROUTER_MANAGER_ROUTER_MANAGER_WINDOW_H
