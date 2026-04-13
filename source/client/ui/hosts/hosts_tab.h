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

#include "client/config.h"
#include "client/ui/client_tab.h"
#include "ui_hosts_tab.h"

class QTreeWidgetItem;

namespace client {

class ContentWidget;
class LocalGroupWidget;
class RouterConnection;
class RouterWidget;
class RouterGroupWidget;
class SearchWidget;

class HostsTab : public ClientTab
{
    Q_OBJECT

public:
    explicit HostsTab(QWidget* parent = nullptr);
    ~HostsTab() override;

    QByteArray saveState() override;
    void restoreState(const QByteArray& state) override;
    void onActivated(QStatusBar* statusbar) override;
    void onDeactivated(QStatusBar* statusbar) override;
    bool hasSearchField() const override;
    void onSearchTextChanged(const QString& text) override;
    void reloadRouters();

signals:
    void sig_connect(const client::Config& config);
    void sig_destroyRouterConnection(const QUuid& uuid);
    void sig_updateRouterConfig(const QUuid& uuid, const client::RouterConfig& config);

private slots:
    void onAddComputerAction();
    void onEditComputerAction();
    void onCopyComputerAction();
    void onDeleteComputerAction();
    void onAddGroupAction();
    void onEditGroupAction();
    void onDeleteGroupAction();
    void onSwitchContent(client::Sidebar::Item::Type type);
    void onSidebarContextMenu(client::Sidebar::Item::Type type, const QPoint& pos);
    void onCurrentComputerChanged(qint64 computer_id);
    void onConnectAction(QAction* action);
    void onLocalConnect(qint64 computer_id);
    void onLocalComputerContextMenu(qint64 computer_id, const QPoint& pos);

private:
    void switchContent(ContentWidget* new_widget);
    void updateActionsState();
    proto::peer::SessionType defaultSessionType() const;

    void destroyAllRouterConnections();
    void createRouterConnection(const RouterConfig& config);

    Ui::HostsTab ui;

    QAction* action_add_group_ = nullptr;
    QAction* action_delete_group_ = nullptr;
    QAction* action_edit_group_ = nullptr;

    QAction* action_add_computer_ = nullptr;
    QAction* action_delete_computer_ = nullptr;
    QAction* action_edit_computer_ = nullptr;
    QAction* action_copy_computer_ = nullptr;

    QAction* action_desktop_ = nullptr;
    QAction* action_file_transfer_ = nullptr;
    QAction* action_chat_ = nullptr;
    QAction* action_system_info_ = nullptr;

    QAction* action_desktop_connect_ = nullptr;
    QAction* action_file_transfer_connect_ = nullptr;
    QAction* action_chat_connect_ = nullptr;
    QAction* action_system_info_connect_ = nullptr;

    QAction* action_router_users_ = nullptr;

    ContentWidget* current_content_ = nullptr;
    ContentWidget* previous_content_ = nullptr;

    LocalGroupWidget* local_group_widget_ = nullptr;
    RouterWidget* router_widget_ = nullptr;
    RouterGroupWidget* router_group_widget_ = nullptr;
    SearchWidget* search_widget_ = nullptr;

    Q_DISABLE_COPY_MOVE(HostsTab)
};

} // namespace client

#endif // CLIENT_UI_HOSTS_HOSTS_TAB_H
