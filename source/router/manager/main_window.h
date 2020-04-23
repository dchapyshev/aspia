//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ROUTER__MANAGER__MAIN_WINDOW_H
#define ROUTER__MANAGER__MAIN_WINDOW_H

#include "proto/router.pb.h"
#include "router/manager/router_window.h"
#include "ui_main_window.h"

#include <QMainWindow>
#include <QPointer>

namespace router {

class RouterProxy;
class RouterWindowProxy;
class StatusDialog;

class MainWindow
    : public QMainWindow,
      public RouterWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void connectToRouter(const QString& address,
                         uint16_t port,
                         const QByteArray& public_key,
                         const QString& user_name,
                         const QString& password);

    // RouterWindow implementation.
    void onConnected(const base::Version& peer_version) override;
    void onDisconnected(net::Channel::ErrorCode error_code) override;
    void onAccessDenied(net::ClientAuthenticator::ErrorCode error_code) override;
    void onPeerList(std::shared_ptr<proto::PeerList> peer_list) override;
    void onPeerResult(std::shared_ptr<proto::PeerResult> peer_result) override;
    void onProxyList(std::shared_ptr<proto::ProxyList> proxy_list) override;
    void onUserList(std::shared_ptr<proto::UserList> user_list) override;
    void onUserResult(std::shared_ptr<proto::UserResult> user_result) override;

protected:
    // QMainWindow implementation.
    void closeEvent(QCloseEvent* event) override;

private:
    void refreshPeerList();
    void disconnectPeer();
    void refreshProxyList();
    void refreshUserList();
    void addUser();
    void modifyUser();
    void deleteUser();
    void onCurrentUserChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

    void beforeRequest();
    void afterRequest();

    Ui::MainWindow ui;

    QString peer_address_;
    uint16_t peer_port_ = 0;

    StatusDialog* status_dialog_;

    std::shared_ptr<RouterWindowProxy> window_proxy_;
    std::unique_ptr<RouterProxy> router_proxy_;

    DISALLOW_COPY_AND_ASSIGN(MainWindow);
};

} // namespace router

#endif // ROUTER__MANAGER__MAIN_WINDOW_H
