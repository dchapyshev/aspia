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

#ifndef CLIENT_UI_HOSTS_HOSTS_TAB_H
#define CLIENT_UI_HOSTS_HOSTS_TAB_H

#include <QHash>

#include <memory>

#include "client/config.h"
#include "client/router.h"
#include "client/ui/hosts/sidebar.h"
#include "client/ui/tab.h"

namespace Ui {
class HostsTab;
} // namespace Ui

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

class ContentWidget;
class LocalGroupWidget;
class RouterClientsWidget;
class RouterGroupWidget;
class RouterHostsWidget;
class RouterRelaysWidget;
class RouterStatusWidget;
class RouterUsersWidget;
class SearchWidget;
class User;

class HostsTab final : public Tab
{
    Q_OBJECT

public:
    explicit HostsTab(QWidget* parent = nullptr);
    ~HostsTab() final;

    // Tab implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    void activate(QStatusBar* statusbar) final;
    void deactivate(QStatusBar* statusbar) final;
    QString searchText() const final;
    void searchTextChanged(const QString& text) final;

    void reloadRouters();

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private slots:
    void onSwitchContent(SidebarItem::Type type);
    void onSidebarContextMenu(SidebarItem::Type type, const QPoint& pos);
    void onCurrentHostChanged(qint64 entry_id);
    void onConnectAction(QAction* action);
    void onLocalConnect(qint64 entry_id);
    void onRouterGroupConnect();
    void onSearchConnect();
    void onLocalHostContextMenu(qint64 entry_id, const QPoint& pos);
    void onSearchContextMenu(const QPoint& pos);
    void onAddHost();
    void onEditHost();
    void onCopyHost();
    void onRemoveHost();
    void onUserContextMenu(const User& user, const QPoint& pos);
    void onHostContextMenu(const QPoint& pos, int column);
    void onClientContextMenu(const QPoint& pos, int column);
    void onRelayContextMenu(const QPoint& pos, int column);
    void onRouterGroupContextMenu(const QPoint& pos);
    void onAddUserAction();
    void onEditUserAction();
    void onDeleteUserAction();
    void onAddWorkspaceAction();
    void onEditWorkspaceAction();
    void onDeleteWorkspaceAction();
    void onAddGroupAction();
    void onEditGroupAction();
    void onDeleteGroupAction();
    void onChangeRouterPassword();
    void onImportOldBookAction();
    void onExportBookAction();
    void onImportBookAction();
    void onReloadAction();
    void onSaveAction();
    void onDisconnectAction();
    void onDisconnectAllAction();
    void onRemoveHostAction();
    void onCheckHostUpdatesAction();
    void onOnlineCheckToggled(bool checked);

private:
    void switchContent(ContentWidget* new_widget);
    void updateActionsState();
    proto::peer::SessionType defaultSessionType() const;

    bool validateHostForConnect(const HostConfig& host);
    qint64 currentHostEntryId() const;
    void refreshItem(qint64 entry_id);
    void removeItem(qint64 entry_id);

    std::unique_ptr<Ui::HostsTab> ui;
    ContentWidget* current_content_ = nullptr;
    ContentWidget* previous_content_ = nullptr;

    QStatusBar* statusbar_ = nullptr;

    LocalGroupWidget* local_group_widget_ = nullptr;
    RouterGroupWidget* router_group_widget_ = nullptr;
    RouterHostsWidget* router_hosts_widget_ = nullptr;
    RouterUsersWidget* router_users_widget_ = nullptr;
    RouterClientsWidget* router_clients_widget_ = nullptr;
    RouterRelaysWidget* router_relays_widget_ = nullptr;
    RouterStatusWidget* router_status_widget_ = nullptr;
    SearchWidget* search_widget_ = nullptr;

    Q_DISABLE_COPY_MOVE(HostsTab)
};

#endif // CLIENT_UI_HOSTS_HOSTS_TAB_H
