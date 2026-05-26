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
class RouterGroupWidget;
class RouterWidget;
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
    bool hasSearchField() const final;
    QString searchText() const final;
    void searchTextChanged(const QString& text) final;

    void reloadRouters();

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private slots:
    void onRouterStatusChanged(qint64 router_id, Router::Status status);
    void onSwitchContent(Sidebar::Item::Type type);
    void onSidebarContextMenu(Sidebar::Item::Type type, const QPoint& pos);
    void onCurrentComputerChanged(qint64 entry_id);
    void onConnectAction(QAction* action);
    void onLocalConnect(qint64 entry_id);
    void onLocalComputerContextMenu(qint64 entry_id, const QPoint& pos);
    void onAddComputer();
    void onEditComputer();
    void onCopyComputer();
    void onRemoveComputer();
    void onUserContextMenu(qint64 router_id, const User& user, const QPoint& pos);
    void onHostContextMenu(qint64 router_id, const QPoint& pos, int column);
    void onClientContextMenu(qint64 router_id, const QPoint& pos, int column);
    void onRelayContextMenu(qint64 router_id, const QPoint& pos, int column);
    void onWorkspaceContextMenu(qint64 router_id, const QPoint& pos);
    void onRouterGroupContextMenu(const QPoint& pos);
    void onAddUserAction();
    void onEditUserAction();
    void onDeleteUserAction();
    void onAddWorkspaceAction();
    void onEditWorkspaceAction();
    void onDeleteWorkspaceAction();
    void onRouterStatus();
    void onImportOldBookAction();
    void onExportBookAction();
    void onImportBookAction();
    void onReloadAction();
    void onSaveAction();
    void onDisconnectAction();
    void onDisconnectAllAction();
    void onRemoveHostAction();
    void onOnlineCheckToggled(bool checked);

private:
    void switchContent(ContentWidget* new_widget);
    void updateActionsState();
    proto::peer::SessionType defaultSessionType() const;

    void destroyAllRouterWidgets();
    void destroyRouterWidget(qint64 router_id);
    RouterWidget* createRouterWidget(const RouterConfig& config);

    bool validateComputerForConnect(const HostConfig& host);
    qint64 currentComputerId() const;
    void refreshItem(qint64 entry_id);
    void removeItem(qint64 entry_id);

    std::unique_ptr<Ui::HostsTab> ui;
    ContentWidget* current_content_ = nullptr;
    ContentWidget* previous_content_ = nullptr;

    QStatusBar* statusbar_ = nullptr;

    LocalGroupWidget* local_group_widget_ = nullptr;
    RouterGroupWidget* router_group_widget_ = nullptr;
    SearchWidget* search_widget_ = nullptr;

    QHash<qint64, RouterWidget*> router_widgets_;

    Q_DISABLE_COPY_MOVE(HostsTab)
};

#endif // CLIENT_UI_HOSTS_HOSTS_TAB_H
