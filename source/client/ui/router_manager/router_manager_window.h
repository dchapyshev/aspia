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

#include <QMainWindow>
#include <QPointer>
#include <QVersionNumber>

#include "base/peer/authenticator.h"
#include "base/net/tcp_channel.h"
#include "client/router_config.h"
#include "proto/router_admin.h"

namespace Ui {
class RouterManagerWindow;
} // namespace Ui

class QTreeWidget;
class QTreeWidgetItem;

namespace common {
class StatusDialog;
} // namespace common

namespace client {

class RouterManagerWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit RouterManagerWindow(QWidget* parent = nullptr);
    ~RouterManagerWindow() final;

    void connectToRouter(const RouterConfig& router_config);

public slots:
    void onConnecting();
    void onConnected(const QVersionNumber& peer_version);
    void onDisconnected(base::TcpChannel::ErrorCode error_code);
    void onWaitForRouter();
    void onWaitForRouterTimeout();
    void onVersionMismatch(const QVersionNumber& router, const QVersionNumber& client);
    void onAccessDenied(base::Authenticator::ErrorCode error_code);
    void onSessionList(std::shared_ptr<proto::router::SessionList> session_list);
    void onSessionResult(std::shared_ptr<proto::router::SessionResult> session_result);
    void onUserList(std::shared_ptr<proto::router::UserList> user_list);
    void onUserResult(std::shared_ptr<proto::router::UserResult> user_result);

    static QString delayToString(quint64 delay);
    static QString sizeToString(qint64 size);

signals:
    void sig_connectToRouter(const QString& address, quint16 port);
    void sig_disconnectFromRouter();
    void sig_refreshSessionList();
    void sig_stopSession(qint64 session_id);
    void sig_refreshUserList();
    void sig_addUser(const proto::router::User& user);
    void sig_modifyUser(const proto::router::User& user);
    void sig_deleteUser(qint64 entry_id);
    void sig_disconnectPeerSession(qint64 relay_session_id, quint64 peer_session_id);

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
    quint16 peer_port_ = 0;
    bool is_connected_ = false;

    common::StatusDialog* status_dialog_;

    int current_hosts_column_ = 0;
    int current_relays_column_ = 0;
    int current_active_conn_column_ = 0;

    Q_DISABLE_COPY(RouterManagerWindow)
};

} // namespace client

#endif // CLIENT_UI_ROUTER_MANAGER_ROUTER_MANAGER_WINDOW_H
