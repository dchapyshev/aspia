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

#ifndef CLIENT_UI_HOSTS_ROUTER_WIDGET_H
#define CLIENT_UI_HOSTS_ROUTER_WIDGET_H

#include <memory>

#include "base/scoped_qpointer.h"
#include "client/config.h"
#include "client/router.h"
#include "client/ui/hosts/content_widget.h"

namespace Ui {
class RouterWidget;
} // namespace Ui

class QLabel;
class QStatusBar;
class StatusDialog;
class User;

namespace proto::router {
class ClientList;
class ClientResult;
class HostResult;
class RelayList;
class RelayResult;
} // namespace proto::router

class RouterWidget final : public ContentWidget
{
    Q_OBJECT

public:
    enum class TabType
    {
        WORKSPACES = 0,
        HOSTS      = 1,
        CLIENTS    = 2,
        RELAYS     = 3,
        USERS      = 4
    };
    Q_ENUM(TabType)

    explicit RouterWidget(const RouterConfig& config, QWidget* parent = nullptr);
    ~RouterWidget() final;

    qint64 routerId() const;
    Router::Status status() const;
    TabType currentTabType() const;
    bool hasSelectedUser() const;
    bool hasSelectedHost() const;
    int hostCount() const;
    bool hasSelectedClient() const;
    int clientCount() const;
    bool hasSelectedRelay() const;
    int relayCount() const;

    void copyCurrentHostRow();
    void copyCurrentHostColumn(int column);
    void copyCurrentClientRow();
    void copyCurrentClientColumn(int column);
    void copyCurrentRelayRow();
    void copyCurrentRelayColumn(int column);

    // ContentWidget implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    bool canReload() const final { return true; }
    void reload() final;
    bool canSave() const final;
    void save() final;
    void activate(QStatusBar* statusbar) final;
    void deactivate(QStatusBar* statusbar) final;

    void connectToRouter();
    void disconnectFromRouter();
    void updateConfig(const RouterConfig& config);

    void showStatusDialog();

public slots:
    void onUpdateRelayList();
    void onUpdateHostList();
    void onUpdateClientList();
    void onUpdateUserList();
    void onAddUser();
    void onModifyUser();
    void onDeleteUser();
    void onDisconnectHost();
    void onDisconnectAllHosts();
    void onRemoveHost();
    void onDisconnectRelay();
    void onDisconnectAllRelays();
    void onDisconnectClient();
    void onDisconnectAllClients();

signals:
    void sig_relayListRequest();
    void sig_hostListRequest();
    void sig_clientListRequest();
    void sig_userListRequest();
    void sig_addUser(const proto::router::User& user);
    void sig_modifyUser(const proto::router::User& user);
    void sig_deleteUser(qint64 entry_id);
    void sig_disconnectHost(qint64 session_id);
    void sig_removeHost(qint64 session_id, bool try_to_uninstall);
    void sig_disconnectRelay(qint64 session_id);
    void sig_disconnectClient(qint64 session_id);
    void sig_disconnectPeer(qint64 relay_entry_id, quint64 peer_session_id);
    void sig_statusChanged(qint64 router_id, Router::Status status);
    void sig_currentTabTypeChanged(qint64 router_id, RouterWidget::TabType tab);
    void sig_currentUserChanged(qint64 router_id);
    void sig_currentHostChanged(qint64 router_id);
    void sig_currentClientChanged(qint64 router_id);
    void sig_currentRelayChanged(qint64 router_id);
    void sig_userContextMenu(qint64 router_id, const User& user, const QPoint& global_pos);
    void sig_hostContextMenu(qint64 router_id, const QPoint& global_pos, int column);
    void sig_clientContextMenu(qint64 router_id, const QPoint& global_pos, int column);
    void sig_relayContextMenu(qint64 router_id, const QPoint& global_pos, int column);
    void sig_updateConfig(const RouterConfig& config);

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private slots:
    void onStatusChanged(qint64 router_id, Router::Status status);
    void onConnectionErrorOccurred(qint64 router_id, TcpChannel::ErrorCode error_code);
    void onTabChanged(int index);
    void onCurrentUserChanged();
    void onCurrentRelayChanged();
    void onCurrentHostChanged();
    void onCurrentClientChanged();
    void onUserContextMenuRequested(const QPoint& pos);
    void onHostContextMenuRequested(const QPoint& pos);
    void onClientContextMenuRequested(const QPoint& pos);
    void onRelayContextMenuRequested(const QPoint& pos);
    void onPeerContextMenuRequested(const QPoint& pos);
    void onRelayListReceived(const proto::router::RelayList& relays);
    void onHostListReceived(const proto::router::HostList& hosts);
    void onClientListReceived(const proto::router::ClientList& clients);
    void onUserListReceived(const proto::router::UserList& list);
    void onUserResultReceived(const proto::router::UserResult& result);
    void onHostResultReceived(const proto::router::HostResult& result);
    void onRelayResultReceived(const proto::router::RelayResult& result);
    void onClientResultReceived(const proto::router::ClientResult& result);

private:
    void updateStatusLabel();
    void updateRelayStatistics();
    void saveHostsToFile();
    void saveClientsToFile();
    void saveRelaysToFile();

    std::unique_ptr<Ui::RouterWidget> ui;
    RouterConfig config_;
    ScopedQPointer<Router> router_;
    Router::Status status_ = Router::Status::OFFLINE;

    StatusDialog* status_dialog_ = nullptr;

    QLabel* status_label_ = nullptr;

    Q_DISABLE_COPY_MOVE(RouterWidget)
};

#endif // CLIENT_UI_HOSTS_ROUTER_WIDGET_H
