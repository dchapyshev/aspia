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

#include "base/peer/user.h"
#include "client/config.h"
#include "client/router_connection.h"
#include "client/ui/hosts/content_widget.h"
#include "ui_router_widget.h"

class QLabel;
class QStatusBar;

namespace common {
class StatusDialog;
} // namespace common

namespace proto::router {
class HostResult;
class RelayList;
class RelayResult;
} // namespace proto::router

namespace client {

class RouterWidget : public ContentWidget
{
    Q_OBJECT

public:
    enum class TabType
    {
        WORKSPACES = 0,
        HOSTS = 1,
        RELAYS = 2,
        USERS  = 3
    };
    Q_ENUM(TabType)

    explicit RouterWidget(const RouterConfig& config, QWidget* parent = nullptr);
    ~RouterWidget() override;

    qint64 routerId() const;
    RouterConnection::Status status() const;
    TabType currentTabType() const;
    bool hasSelectedUser() const;
    bool hasSelectedHost() const;
    int hostCount() const;
    bool hasSelectedRelay() const;
    int relayCount() const;

    void copyCurrentHostRow();
    void copyCurrentHostColumn(int column);
    void copyCurrentRelayRow();
    void copyCurrentRelayColumn(int column);

    QByteArray saveState() override;
    void restoreState(const QByteArray& state) override;
    bool canReload() const override { return true; }
    void reload() override;
    bool canSave() const override;
    void save() override;
    void attachStatusBar(QStatusBar* statusbar) override;
    void detachStatusBar(QStatusBar* statusbar) override;

    void connectToRouter();
    void disconnectFromRouter();
    void updateConfig(const RouterConfig& config);

    void showStatusDialog();

    static QString delayToString(quint64 delay);
    static QString sizeToString(qint64 size);

public slots:
    void onUpdateRelayList();
    void onUpdateHostList();
    void onUpdateUserList();
    void onAddUser();
    void onModifyUser();
    void onDeleteUser();
    void onDisconnectHost();
    void onDisconnectAllHosts();
    void onRemoveHost();
    void onDisconnectRelay();
    void onDisconnectAllRelays();

signals:
    void sig_relayListRequest();
    void sig_hostListRequest();
    void sig_userListRequest();
    void sig_addUser(const proto::router::User& user);
    void sig_modifyUser(const proto::router::User& user);
    void sig_deleteUser(qint64 entry_id);
    void sig_disconnectHost(qint64 session_id);
    void sig_removeHost(qint64 session_id, bool try_to_uninstall);
    void sig_disconnectRelay(qint64 session_id);
    void sig_disconnectPeer(qint64 relay_entry_id, quint64 peer_session_id);
    void sig_statusChanged(qint64 router_id, client::RouterConnection::Status status);
    void sig_currentTabTypeChanged(qint64 router_id, client::RouterWidget::TabType tab);
    void sig_currentUserChanged(qint64 router_id);
    void sig_currentHostChanged(qint64 router_id);
    void sig_currentRelayChanged(qint64 router_id);
    void sig_userContextMenu(qint64 router_id, const base::User& user, const QPoint& global_pos);
    void sig_hostContextMenu(qint64 router_id, const QPoint& global_pos, int column);
    void sig_relayContextMenu(qint64 router_id, const QPoint& global_pos, int column);
    void sig_updateConfig(const client::RouterConfig& config);

private slots:
    void onStatusChanged(qint64 router_id, client::RouterConnection::Status status);
    void onConnectionErrorOccurred(qint64 router_id, base::TcpChannel::ErrorCode error_code);
    void onTabChanged(int index);
    void onCurrentUserChanged();
    void onCurrentRelayChanged();
    void onCurrentHostChanged();
    void onUserContextMenuRequested(const QPoint& pos);
    void onHostContextMenuRequested(const QPoint& pos);
    void onRelayContextMenuRequested(const QPoint& pos);
    void onPeerContextMenuRequested(const QPoint& pos);
    void onRelayListReceived(const proto::router::RelayList& relays);
    void onHostListReceived(const proto::router::HostList& hosts);
    void onUserListReceived(const proto::router::UserList& list);
    void onUserResultReceived(const proto::router::UserResult& result);
    void onHostResultReceived(const proto::router::HostResult& result);
    void onRelayResultReceived(const proto::router::RelayResult& result);

private:
    void updateStatusLabel();
    void updateRelayStatistics();
    void saveHostsToFile();
    void saveRelaysToFile();

    Ui::RouterWidget ui;

    RouterConfig config_;
    RouterConnection* connection_ = nullptr;
    RouterConnection::Status status_ = RouterConnection::Status::OFFLINE;

    common::StatusDialog* status_dialog_ = nullptr;

    QLabel* status_label_ = nullptr;

    Q_DISABLE_COPY_MOVE(RouterWidget)
};

} // namespace client

#endif // CLIENT_UI_HOSTS_ROUTER_WIDGET_H
