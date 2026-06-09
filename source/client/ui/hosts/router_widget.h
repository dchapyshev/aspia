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

#include <QPointer>

#include <memory>

#include "base/peer/host_id.h"
#include "client/config.h"
#include "client/router.h"
#include "client/ui/hosts/content_widget.h"

namespace Ui {
class RouterWidget;
} // namespace Ui

class QLabel;
class QStatusBar;
class QTreeWidget;
class StatusDialog;
class User;

namespace proto::router {
class ClientList;
class ClientResult;
class HostResult;
class PeerResult;
class RelayList;
class RelayResult;
class User;
class UserList;
class UserResult;
class Workspace;
class WorkspaceResult;
} // namespace proto::router

class RouterWidget final : public ContentWidget
{
    Q_OBJECT

public:
    // NONE is returned by currentTabType() for non-admin sessions, where the tab widget is
    // hidden and replaced with a minimal info panel. The other values follow the QTabWidget
    // index order so they can be obtained via static_cast<TabType>(ui->tab->currentIndex()).
    enum class TabType
    {
        NONE       = -1,
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
    bool isSelectedHostOnline() const;
    HostConfig selectedHostConfig() const;
    int hostCount() const;
    bool hasSelectedClient() const;
    int clientCount() const;
    bool hasSelectedRelay() const;
    int relayCount() const;
    bool hasSelectedWorkspace() const;
    int workspaceCount() const;

    void copyCurrentHostRow();
    void copyCurrentHostColumn(int column);
    void copyCurrentClientRow();
    void copyCurrentClientColumn(int column);
    void copyCurrentRelayRow();
    void copyCurrentRelayColumn(int column);

    // ContentWidget implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    bool canReload() const final;
    void reload() final;
    bool canSave() const final;
    void save() final;
    void activate(QStatusBar* statusbar) final;
    void deactivate(QStatusBar* statusbar) final;

    void connectToRouter();
    void disconnectFromRouter();
    void updateConfig(const RouterConfig& config);

    void showStatusDialog();
    void changePassword();

    // Factory for issuing correlated router requests. Pass it to dialogs/other widgets that
    // need to talk to the router without going through RouterWidget. QPointer guards against
    // dangling access if the widget (and its router_) goes away.
    QPointer<Router> router() const { return router_; }

public slots:
    void onUpdateRelayList();
    void onUpdateHostList();
    void onUpdateClientList();
    void onUpdateUserList();
    void onAddUser();
    void onModifyUser();
    void onDeleteUser();
    void onModifyHost();
    void onDisconnectHost();
    void onDisconnectAllHosts();
    void onRemoveHost();
    void onCheckHostUpdates();
    void onDisconnectRelay();
    void onDisconnectAllRelays();
    void onDisconnectClient();
    void onDisconnectAllClients();
    void onUpdateWorkspaceList();
    void onAddWorkspace();
    void onModifyWorkspace();
    void onDeleteWorkspace();

signals:
    void sig_statusChanged(qint64 router_id, Router::Status status);
    void sig_currentTabTypeChanged(qint64 router_id, RouterWidget::TabType tab);
    void sig_currentUserChanged(qint64 router_id);
    void sig_currentHostChanged(qint64 router_id);
    void sig_currentClientChanged(qint64 router_id);
    void sig_currentRelayChanged(qint64 router_id);
    void sig_currentWorkspaceChanged(qint64 router_id);
    void sig_userContextMenu(qint64 router_id, const User& user, const QPoint& global_pos);
    void sig_hostContextMenu(qint64 router_id, const QPoint& global_pos, int column);
    void sig_clientContextMenu(qint64 router_id, const QPoint& global_pos, int column);
    void sig_relayContextMenu(qint64 router_id, const QPoint& global_pos, int column);
    void sig_workspaceContextMenu(qint64 router_id, const QPoint& global_pos);

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

    // QObject implementation.
    bool eventFilter(QObject* watched, QEvent* event) final;

private slots:
    void onStatusChanged(qint64 router_id, Router::Status status);
    void onConnectionErrorOccurred(qint64 router_id, TcpChannel::ErrorCode error_code);
    void onPasswordChangeRequired();
    void onTwoFactorCodeRequired();
    void onTwoFactorEnrollment(qint64 router_id, const QString& otpauth_uri);
    void onTabChanged(int index);
    void onCurrentUserChanged();
    void onCurrentRelayChanged();
    void onCurrentHostChanged();
    void onCurrentClientChanged();
    void onCurrentWorkspaceChanged();
    void onUserContextMenuRequested(const QPoint& pos);
    void onHostContextMenuRequested(const QPoint& pos);
    void onClientContextMenuRequested(const QPoint& pos);
    void onRelayContextMenuRequested(const QPoint& pos);
    void onPeerContextMenuRequested(const QPoint& pos);
    void onWorkspaceContextMenuRequested(const QPoint& pos);
    void onRelayListReceived(const proto::router::RelayList& relays);
    void onHostListReceived(const Router::HostList& list);
    void onClientListReceived(const proto::router::ClientList& clients);
    void onUserListReceived(const proto::router::UserList& list);
    void onUserResultReceived(const proto::router::UserResult& result);
    void onHostResultReceived(const proto::router::HostResult& result);
    void onRelayResultReceived(const proto::router::RelayResult& result);
    void onClientResultReceived(const proto::router::ClientResult& result);
    void onPeerResultReceived(const proto::router::PeerResult& result);
    void onWorkspaceListReceived(const Router::WorkspaceList& list);
    void onWorkspaceResultReceived(const proto::router::WorkspaceResult& result);
    void onHostsPageSizeChanged(int index);
    void onHostsPageChanged(int index);
    void onHostsPrevClicked();
    void onHostsNextClicked();

private:
    void syncAdminVisibility();
    void showColumnsContextMenu(QTreeWidget* tree, const QPoint& pos);
    void updateStatusLabel();
    void updateRelayStatistics();
    void updateHostsPagination();
    void saveHostsToFile();
    QString workspaceNameById(qint64 workspace_id) const;
    void refreshHostsWorkspaceColumn();
    void saveClientsToFile();
    void saveRelaysToFile();

    std::unique_ptr<Ui::RouterWidget> ui;
    Router* router_ = nullptr;

    StatusDialog* status_dialog_ = nullptr;
    QLabel* status_label_ = nullptr;

    qint64 hosts_page_size_ = 100;
    qint64 hosts_current_page_ = 1;
    qint64 hosts_total_count_ = 0;

    Q_DISABLE_COPY_MOVE(RouterWidget)
};

#endif // CLIENT_UI_HOSTS_ROUTER_WIDGET_H
