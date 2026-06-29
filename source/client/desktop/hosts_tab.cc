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

#include "client/desktop/hosts_tab.h"

#include <QActionGroup>
#include <QDateTime>
#include <QEvent>
#include <QFileDialog>
#include <QMenu>
#include <QStatusBar>

#include <optional>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "base/peer/user.h"
#include "build/build_config.h"
#include "client/aab_importer.h"
#include "client/database.h"
#include "client/json_backup.h"
#include "client/master_password.h"
#include "client/settings.h"
#include "client/desktop/hosts/content_widget.h"
#include "client/desktop/hosts/local_group_widget.h"
#include "client/desktop/hosts/local_host_dialog.h"
#include "client/desktop/hosts/router_clients_widget.h"
#include "client/desktop/hosts/router_group_dialog.h"
#include "client/desktop/hosts/router_group_widget.h"
#include "client/desktop/hosts/router_hosts_widget.h"
#include "client/desktop/hosts/router_relays_widget.h"
#include "client/desktop/hosts/router_status_widget.h"
#include "client/desktop/hosts/router_users_widget.h"
#include "client/desktop/hosts/router_workspace_dialog.h"
#include "client/desktop/hosts/search_widget.h"
#include "common/desktop/credentials_dialog.h"
#include "common/desktop/msg_box.h"
#include "proto/peer.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_hosts_tab.h"

//--------------------------------------------------------------------------------------------------
HostsTab::HostsTab(QWidget* parent)
    : Tab(Type::HOSTS, "hosts", parent),
      ui(std::make_unique<Ui::HostsTab>())
{
    LOG(INFO) << "Ctor";

    if (!Database::instance().isValid())
        LOG(ERROR) << "Failed to open or create book database";

    ui->setupUi(this);

    // Group session-type actions to make them mutually exclusive.
    QActionGroup* session_type_group = new QActionGroup(this);
    session_type_group->addAction(ui->action_desktop);
    session_type_group->addAction(ui->action_terminal);
    session_type_group->addAction(ui->action_file_transfer);
    session_type_group->addAction(ui->action_chat);
    session_type_group->addAction(ui->action_system_info);

    QActionGroup* session_connect_group = new QActionGroup(this);
    session_connect_group->addAction(ui->action_desktop_connect);
    session_connect_group->addAction(ui->action_terminal_connect);
    session_connect_group->addAction(ui->action_file_transfer_connect);
    session_connect_group->addAction(ui->action_chat_connect);
    session_connect_group->addAction(ui->action_system_info_connect);

    Settings settings;

    switch (settings.sessionType())
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
            ui->action_desktop->setChecked(true);
            break;
        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            ui->action_file_transfer->setChecked(true);
            break;
        case proto::peer::SESSION_TYPE_CHAT:
            ui->action_chat->setChecked(true);
            break;
        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
            ui->action_system_info->setChecked(true);
            break;
        case proto::peer::SESSION_TYPE_TERMINAL:
            ui->action_terminal->setChecked(true);
            break;
        default:
            break;
    }

    ui->action_online_check->setChecked(settings.isOnlineCheckEnabled());

    ui->action_import_old_book->setProperty(Tab::kMenuOnlyProperty, true);
    ui->action_export_book->setProperty(Tab::kMenuOnlyProperty, true);
    ui->action_import_book->setProperty(Tab::kMenuOnlyProperty, true);
    ui->action_online_check->setProperty(Tab::kMenuOnlyProperty, true);
    ui->action_add_router->setProperty(Tab::kMenuOnlyProperty, true);
    ui->action_delete_router->setProperty(Tab::kMenuOnlyProperty, true);

    // Create content widgets.
    local_group_widget_ = new LocalGroupWidget(this);
    router_group_widget_ = new RouterGroupWidget(this);
    router_hosts_widget_ = new RouterHostsWidget(this);
    router_users_widget_ = new RouterUsersWidget(this);
    router_clients_widget_ = new RouterClientsWidget(this);
    router_relays_widget_ = new RouterRelaysWidget(this);
    router_status_widget_ = new RouterStatusWidget(this);
    search_widget_ = new SearchWidget(this);

    ui->content_stack->addWidget(local_group_widget_);
    ui->content_stack->addWidget(router_group_widget_);
    ui->content_stack->addWidget(router_hosts_widget_);
    ui->content_stack->addWidget(router_users_widget_);
    ui->content_stack->addWidget(router_clients_widget_);
    ui->content_stack->addWidget(router_relays_widget_);
    ui->content_stack->addWidget(router_status_widget_);
    ui->content_stack->addWidget(search_widget_);

    connect(ui->sidebar, &Sidebar::sig_routerEvent,
            router_status_widget_, &RouterStatusWidget::onEvent);

    connect(router_hosts_widget_, &RouterHostsWidget::sig_currentChanged,
            this, &HostsTab::updateActionsState);
    connect(router_hosts_widget_, &RouterHostsWidget::sig_contextMenu,
            this, &HostsTab::onHostContextMenu);

    connect(router_users_widget_, &RouterUsersWidget::sig_currentChanged,
            this, &HostsTab::updateActionsState);
    connect(router_users_widget_, &RouterUsersWidget::sig_userContextMenu,
            this, &HostsTab::onUserContextMenu);

    connect(router_clients_widget_, &RouterClientsWidget::sig_currentChanged,
            this, &HostsTab::updateActionsState);
    connect(router_clients_widget_, &RouterClientsWidget::sig_contextMenu,
            this, &HostsTab::onClientContextMenu);

    connect(router_relays_widget_, &RouterRelaysWidget::sig_currentChanged,
            this, &HostsTab::updateActionsState);
    connect(router_relays_widget_, &RouterRelaysWidget::sig_contextMenu,
            this, &HostsTab::onRelayContextMenu);

    // Setup drag-and-drop: pass the host mime types from the source widgets to Sidebar.
    ui->sidebar->setLocalHostMimeType(local_group_widget_->mimeType());
    ui->sidebar->setRouterHostMimeType(router_group_widget_->mimeType());

    // Connect signals.
    connect(ui->sidebar, &Sidebar::sig_switchContent, this, &HostsTab::onSwitchContent);
    connect(ui->sidebar, &Sidebar::sig_contextMenu, this, &HostsTab::onSidebarContextMenu);
    connect(ui->sidebar, &Sidebar::sig_addGroup, this, &HostsTab::onAddGroupAction);
    connect(ui->sidebar, &Sidebar::sig_removeGroup, this, &HostsTab::onDeleteGroupAction);
    connect(ui->sidebar, &Sidebar::sig_editGroup, this, &HostsTab::onEditGroupAction);
    connect(ui->sidebar, &Sidebar::sig_itemDropped, this, [this]()
    {
        local_group_widget_->showGroup(ui->sidebar->currentGroupId());
        updateActionsState();
    });
    connect(local_group_widget_, &LocalGroupWidget::sig_currentChanged, this, &HostsTab::onCurrentHostChanged);
    connect(local_group_widget_, &LocalGroupWidget::sig_activated, this, &HostsTab::onLocalConnect);
    connect(local_group_widget_, &LocalGroupWidget::sig_contextMenu, this, &HostsTab::onLocalHostContextMenu);
    connect(local_group_widget_, &LocalGroupWidget::sig_addHost, this, &HostsTab::onAddHost);
    connect(local_group_widget_, &LocalGroupWidget::sig_deleteHost, this, &HostsTab::onRemoveHost);
    connect(local_group_widget_, &LocalGroupWidget::sig_editHost, this, &HostsTab::onEditHost);
    connect(router_group_widget_, &RouterGroupWidget::sig_currentChanged, this, &HostsTab::updateActionsState);
    connect(router_group_widget_, &RouterGroupWidget::sig_contextMenu, this, &HostsTab::onRouterGroupContextMenu);
    connect(router_group_widget_, &RouterGroupWidget::sig_activated, this, &HostsTab::onRouterGroupConnect);
    connect(this, &HostsTab::sig_connectRequested, local_group_widget_,
            [this](const HostConfig& host, proto::peer::SessionType /* session_type */)
    {
        if (host.id() != -1)
            local_group_widget_->setConnectTime(host.id(), QDateTime::currentSecsSinceEpoch());
    });
    connect(ui->action_add_host, &QAction::triggered, this, &HostsTab::onAddHost);
    connect(ui->action_edit_host, &QAction::triggered, this, &HostsTab::onEditHost);
    connect(ui->action_copy_host, &QAction::triggered, this, &HostsTab::onCopyHost);
    connect(ui->action_delete_host, &QAction::triggered, this, &HostsTab::onRemoveHost);
    connect(search_widget_, &SearchWidget::sig_currentChanged, this, &HostsTab::updateActionsState);
    connect(search_widget_, &SearchWidget::sig_activated, this, &HostsTab::onSearchConnect);
    connect(search_widget_, &SearchWidget::sig_contextMenu, this, &HostsTab::onSearchContextMenu);
    connect(ui->action_add_group, &QAction::triggered, this, &HostsTab::onAddGroupAction);
    connect(ui->action_edit_group, &QAction::triggered, this, &HostsTab::onEditGroupAction);
    connect(ui->action_delete_group, &QAction::triggered, this, &HostsTab::onDeleteGroupAction);
    connect(ui->action_add_router, &QAction::triggered, ui->sidebar, &Sidebar::onAddRouter);
    connect(ui->action_edit_router, &QAction::triggered, ui->sidebar, &Sidebar::onEditRouter);
    connect(ui->action_delete_router, &QAction::triggered, ui->sidebar, &Sidebar::onRemoveRouter);
    connect(ui->action_change_router_password, &QAction::triggered, this, &HostsTab::onChangeRouterPassword);
    connect(ui->action_clear_router_events, &QAction::triggered, this, &HostsTab::onClearRouterEvents);
    connect(ui->sidebar, &Sidebar::sig_routersChanged, this, &HostsTab::reloadRouters);
    connect(ui->sidebar, &Sidebar::sig_routerHostMoved, this, [this](qint64 /* router_id */)
    {
        if (current_content_ == router_group_widget_)
            router_group_widget_->reload();
    });
    connect(ui->sidebar, &Sidebar::sig_routerGroupMoved, ui->sidebar, &Sidebar::onRefreshHostGroups);
    connect(ui->action_add_user, &QAction::triggered, this, &HostsTab::onAddUserAction);
    connect(ui->action_edit_user, &QAction::triggered, this, &HostsTab::onEditUserAction);
    connect(ui->action_delete_user, &QAction::triggered, this, &HostsTab::onDeleteUserAction);
    connect(ui->action_add_workspace, &QAction::triggered, this, &HostsTab::onAddWorkspaceAction);
    connect(ui->action_edit_workspace, &QAction::triggered, this, &HostsTab::onEditWorkspaceAction);
    connect(ui->action_delete_workspace, &QAction::triggered, this, &HostsTab::onDeleteWorkspaceAction);
    connect(ui->action_reload, &QAction::triggered, this, &HostsTab::onReloadAction);
    connect(ui->action_save, &QAction::triggered, this, &HostsTab::onSaveAction);
    connect(ui->action_import_old_book, &QAction::triggered, this, &HostsTab::onImportOldBookAction);
    connect(ui->action_export_book, &QAction::triggered, this, &HostsTab::onExportBookAction);
    connect(ui->action_import_book, &QAction::triggered, this, &HostsTab::onImportBookAction);
    connect(ui->action_disconnect, &QAction::triggered, this, &HostsTab::onDisconnectAction);
    connect(ui->action_disconnect_all, &QAction::triggered, this, &HostsTab::onDisconnectAllAction);
    connect(ui->action_host_remove, &QAction::triggered, this, &HostsTab::onRemoveHostAction);
    connect(ui->action_host_check_updates, &QAction::triggered, this, &HostsTab::onCheckHostUpdatesAction);
    connect(ui->action_online_check, &QAction::toggled, this, &HostsTab::onOnlineCheckToggled);
    connect(session_connect_group, &QActionGroup::triggered, this, &HostsTab::onConnectAction);

    // Register actions for toolbar and menus.
    addActions(ActionRole::FILE,
    {
        ui->action_save, ui->action_import_old_book, ui->action_export_book, ui->action_import_book
    });
    addActions(ActionRole::EDIT, { ui->action_add_user, ui->action_edit_user, ui->action_delete_user });
    addActions(ActionRole::EDIT,
    {
        ui->action_add_workspace, ui->action_edit_workspace, ui->action_delete_workspace
    });
    addActions(ActionRole::EDIT,
    {
        ui->action_add_router, ui->action_edit_router, ui->action_delete_router,
        ui->action_change_router_password, ui->action_clear_router_events
    });
    addActions(ActionRole::EDIT,
    {
        ui->action_add_group, ui->action_edit_group, ui->action_delete_group
    });
    addActions(ActionRole::EDIT,
    {
        ui->action_add_host, ui->action_edit_host, ui->action_copy_host, ui->action_delete_host,
        ui->action_host_check_updates, ui->action_host_remove, ui->action_disconnect,
        ui->action_disconnect_all
    });
    addActions(ActionRole::ACTION,
    {
        ui->action_desktop_connect, ui->action_terminal_connect, ui->action_file_transfer_connect,
        ui->action_chat_connect, ui->action_system_info_connect
    });
    addActions(ActionRole::VIEW, { ui->action_reload, ui->action_online_check });
    addActions(ActionRole::SESSION_TYPE,
    {
        ui->action_desktop, ui->action_terminal, ui->action_file_transfer, ui->action_chat,
        ui->action_system_info
    });

    local_group_widget_->setOnlineCheckEnabled(ui->action_online_check->isChecked());
    local_group_widget_->showGroup(ui->sidebar->currentGroupId());
    switchContent(local_group_widget_);
    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
HostsTab::~HostsTab()
{
    LOG(INFO) << "Dtor";
    Settings settings;
    settings.setSessionType(defaultSessionType());
}

//--------------------------------------------------------------------------------------------------
QByteArray HostsTab::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << local_group_widget_->saveState();
        stream << router_group_widget_->saveState();
        stream << router_hosts_widget_->saveState();
        stream << router_users_widget_->saveState();
        stream << router_clients_widget_->saveState();
        stream << router_relays_widget_->saveState();
        stream << router_status_widget_->saveState();
        stream << search_widget_->saveState();
        stream << ui->splitter->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void HostsTab::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray local_group_state;
    QByteArray router_group_state;
    QByteArray router_hosts_state;
    QByteArray router_users_state;
    QByteArray router_clients_state;
    QByteArray router_relays_state;
    QByteArray router_status_state;
    QByteArray search_state;
    QByteArray splitter_state;

    stream >> local_group_state;
    stream >> router_group_state;
    stream >> router_hosts_state;
    stream >> router_users_state;
    stream >> router_clients_state;
    stream >> router_relays_state;
    stream >> router_status_state;
    stream >> search_state;
    stream >> splitter_state;

    if (!local_group_state.isEmpty())
        local_group_widget_->restoreState(local_group_state);

    if (!router_group_state.isEmpty())
        router_group_widget_->restoreState(router_group_state);

    if (!router_hosts_state.isEmpty())
        router_hosts_widget_->restoreState(router_hosts_state);

    if (!router_users_state.isEmpty())
        router_users_widget_->restoreState(router_users_state);

    if (!router_clients_state.isEmpty())
        router_clients_widget_->restoreState(router_clients_state);

    if (!router_relays_state.isEmpty())
        router_relays_widget_->restoreState(router_relays_state);

    if (!router_status_state.isEmpty())
        router_status_widget_->restoreState(router_status_state);

    if (!search_state.isEmpty())
        search_widget_->restoreState(search_state);

    if (!splitter_state.isEmpty())
    {
        ui->splitter->restoreState(splitter_state);
    }
    else
    {
        QList<int> sizes;
        sizes.emplace_back(200);
        sizes.emplace_back(width() - 200);
        ui->splitter->setSizes(sizes);
    }
}

//--------------------------------------------------------------------------------------------------
void HostsTab::activate(QStatusBar* statusbar)
{
    statusbar_ = statusbar;
    if (current_content_ && statusbar_)
        current_content_->activate(statusbar_);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::deactivate(QStatusBar* /* statusbar */)
{
    if (current_content_ && statusbar_)
        current_content_->deactivate(statusbar_);
    statusbar_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
QString HostsTab::searchText() const
{
    return search_widget_->currentQuery();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::searchTextChanged(const QString& text)
{
    if (text.isEmpty())
    {
        // Return to the previous content widget.
        if (previous_content_)
        {
            switchContent(previous_content_);
            previous_content_ = nullptr;
        }

        search_widget_->clear();
    }
    else
    {
        // Save the current content widget before switching to search.
        if (current_content_ != search_widget_)
            previous_content_ = current_content_;

        switchContent(search_widget_);
        search_widget_->search(text);
    }

    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::reloadRouters()
{
    ui->sidebar->reloadRouters();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        emit sig_titleChanged(tr("Hosts"));
    }
    Tab::changeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onSwitchContent(SidebarItem::Type type)
{
    switch (type)
    {
        case SidebarItem::LOCAL_GROUP:
        {
            switchContent(local_group_widget_);
            local_group_widget_->showGroup(ui->sidebar->currentGroupId());
        }
        break;

        case SidebarItem::ROUTER:
        {
            SidebarItem* sidebar_item = ui->sidebar->currentItem();
            if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER)
                break;

            const qint64 router_id = static_cast<SidebarRouter*>(sidebar_item)->routerId();
            switchContent(router_status_widget_);
            router_status_widget_->showRouter(router_id, ui->sidebar->routerEvents(router_id));
        }
        break;

        case SidebarItem::ROUTER_WORKSPACE:
        {
            switchContent(router_group_widget_);

            auto* item = static_cast<SidebarRouterWorkspace*>(ui->sidebar->currentItem());
            router_group_widget_->showGroup(item->routerId(), item->workspaceId(),
                                            item->workspaceName(), /*group_id=*/0);
        }
        break;

        case SidebarItem::ROUTER_GROUP:
        {
            switchContent(router_group_widget_);

            auto* item = static_cast<SidebarRouterGroup*>(ui->sidebar->currentItem());
            router_group_widget_->showGroup(item->routerId(), item->workspaceId(),
                                            item->workspaceName(), item->groupId());
        }
        break;

        case SidebarItem::ROUTER_USERS:
        {
            SidebarItem* sidebar_item = ui->sidebar->currentItem();
            if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_USERS)
                break;

            auto* users_item = static_cast<SidebarRouterUsers*>(sidebar_item);
            switchContent(router_users_widget_);
            router_users_widget_->showRouter(users_item->routerId());
        }
        break;

        case SidebarItem::ROUTER_CLIENTS:
        {
            SidebarItem* sidebar_item = ui->sidebar->currentItem();
            if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_CLIENTS)
                break;

            auto* clients_item = static_cast<SidebarRouterClients*>(sidebar_item);
            switchContent(router_clients_widget_);
            router_clients_widget_->showRouter(clients_item->routerId());
        }
        break;

        case SidebarItem::ROUTER_RELAYS:
        {
            SidebarItem* sidebar_item = ui->sidebar->currentItem();
            if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_RELAYS)
                break;

            auto* relays_item = static_cast<SidebarRouterRelays*>(sidebar_item);
            switchContent(router_relays_widget_);
            router_relays_widget_->showRouter(relays_item->routerId());
        }
        break;

        case SidebarItem::ROUTER_HOSTS:
        {
            SidebarItem* sidebar_item = ui->sidebar->currentItem();
            if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_HOSTS)
                break;

            auto* hosts_item = static_cast<SidebarRouterHosts*>(sidebar_item);
            switchContent(router_hosts_widget_);
            router_hosts_widget_->showRouter(hosts_item->routerId());
        }
        break;

    }

    if (current_content_ != search_widget_)
    {
        previous_content_ = nullptr;
        emit sig_clearSearch();
    }

    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onSidebarContextMenu(SidebarItem::Type type, const QPoint& pos)
{
    QMenu menu;

    if (type == SidebarItem::LOCAL_GROUP)
    {
        menu.addAction(ui->action_add_group);
        menu.addAction(ui->action_edit_group);
        menu.addAction(ui->action_delete_group);
        menu.addSeparator();
        menu.addAction(ui->action_add_host);
    }
    else if (type == SidebarItem::ROUTER_WORKSPACE)
    {
        SidebarItem* item = ui->sidebar->currentItem();
        if (!item || item->itemType() != SidebarItem::ROUTER_WORKSPACE)
            return;

        auto* workspace_item = static_cast<SidebarRouterWorkspace*>(item);

        proto::router::SessionType session_type = proto::router::SESSION_TYPE_CLIENT;
        if (Router* router = Router::instance(workspace_item->routerId()))
            session_type = router->config().sessionType();

        // Clients are read-only and cannot manage host groups or workspaces.
        if (session_type != proto::router::SESSION_TYPE_CLIENT)
        {
            menu.addAction(ui->action_add_group);
            menu.addSeparator();
            menu.addAction(ui->action_edit_workspace);
            menu.addAction(ui->action_delete_workspace);
        }
    }
    else if (type == SidebarItem::ROUTER_GROUP)
    {
        SidebarItem* item = ui->sidebar->currentItem();
        if (!item || item->itemType() != SidebarItem::ROUTER_GROUP)
            return;

        auto* group_item = static_cast<SidebarRouterGroup*>(item);

        proto::router::SessionType session_type = proto::router::SESSION_TYPE_CLIENT;
        if (Router* router = Router::instance(group_item->routerId()))
            session_type = router->config().sessionType();

        // Clients are read-only and cannot manage host groups.
        if (session_type != proto::router::SESSION_TYPE_CLIENT)
        {
            menu.addAction(ui->action_add_group);
            menu.addAction(ui->action_edit_group);
            menu.addAction(ui->action_delete_group);
        }
    }
    else if (type == SidebarItem::ROUTER)
    {
        SidebarItem* item = ui->sidebar->currentItem();
        if (!item || item->itemType() != SidebarItem::ROUTER)
            return;

        menu.addAction(ui->action_edit_router);
        menu.addAction(ui->action_delete_router);
        menu.addAction(ui->action_clear_router_events);

        auto* router_item = static_cast<SidebarRouter*>(item);
        Router* router = Router::instance(router_item->routerId());
        if (router && router->status() == Router::Status::ONLINE)
        {
            menu.addSeparator();
            if (router->config().sessionType() == proto::router::SESSION_TYPE_ADMIN)
                menu.addAction(ui->action_add_workspace);
            menu.addAction(ui->action_change_router_password);
        }

        menu.exec(pos);
        return;
    }
    else if (type == SidebarItem::ROUTER_USERS)
    {
        menu.addAction(ui->action_add_user);
    }
    else
    {
        return;
    }

    if (menu.isEmpty())
        return;

    menu.exec(pos);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onCurrentHostChanged(qint64 /* entry_id */)
{
    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onConnectAction(QAction* action)
{
    proto::peer::SessionType session_type;

    if (action == ui->action_desktop_connect)
        session_type = proto::peer::SESSION_TYPE_DESKTOP;
    else if (action == ui->action_file_transfer_connect)
        session_type = proto::peer::SESSION_TYPE_FILE_TRANSFER;
    else if (action == ui->action_chat_connect)
        session_type = proto::peer::SESSION_TYPE_CHAT;
    else if (action == ui->action_system_info_connect)
        session_type = proto::peer::SESSION_TYPE_SYSTEM_INFO;
    else if (action == ui->action_terminal_connect)
        session_type = proto::peer::SESSION_TYPE_TERMINAL;
    else
        return;

    HostConfig host;

    if (current_content_ == local_group_widget_)
    {
        LocalGroupWidget::Item* item = local_group_widget_->currentItem();
        if (!item)
            return;

        std::optional<HostConfig> found = Database::instance().findHost(item->entryId());
        if (!found.has_value())
        {
            MsgBox::warning(this,
                tr("Failed to retrieve host information from the local database."));
            return;
        }

        host = *found;

        if (!validateHostForConnect(host))
            return;

        Database::instance().setConnectTime(host.id(), QDateTime::currentSecsSinceEpoch());
    }
    else if (current_content_ == search_widget_)
    {
        SearchWidget::Item* item = search_widget_->currentItem();
        if (!item)
            return;

        if (item->type() == SearchWidget::Item::Type::ROUTER)
        {
            host = item->hostConfig();
            if (!validateHostForConnect(host))
                return;
        }
        else
        {
            std::optional<HostConfig> found = Database::instance().findHost(item->entryId());
            if (!found.has_value())
            {
                MsgBox::warning(this,
                    tr("Failed to retrieve host information from the local database."));
                return;
            }

            host = *found;

            if (!validateHostForConnect(host))
                return;

            Database::instance().setConnectTime(host.id(), QDateTime::currentSecsSinceEpoch());
        }
    }
    else if (current_content_ == router_group_widget_)
    {
        if (!router_group_widget_->hasSelectedHost())
            return;
        host = router_group_widget_->selectedHostConfig();
        if (!validateHostForConnect(host))
            return;
    }
    else if (current_content_ == router_hosts_widget_)
    {
        if (!router_hosts_widget_->hasSelectedHost())
            return;
        host = router_hosts_widget_->selectedHostConfig();
        if (!validateHostForConnect(host))
            return;
    }
    else
    {
        return;
    }

    emit sig_connectRequested(host, session_type);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onLocalConnect(qint64 entry_id)
{
    std::optional<HostConfig> host = Database::instance().findHost(entry_id);
    if (!host.has_value())
    {
        MsgBox::warning(this, tr("Failed to retrieve host information from the local database."));
        return;
    }

    if (!validateHostForConnect(*host))
        return;

    Database::instance().setConnectTime(entry_id, QDateTime::currentSecsSinceEpoch());
    emit sig_connectRequested(*host, defaultSessionType());
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onRouterGroupConnect()
{
    if (!router_group_widget_->hasSelectedHost())
        return;
    HostConfig host = router_group_widget_->selectedHostConfig();
    if (!validateHostForConnect(host))
        return;
    emit sig_connectRequested(host, defaultSessionType());
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onSearchConnect()
{
    SearchWidget::Item* item = search_widget_->currentItem();
    if (!item)
        return;

    if (item->type() == SearchWidget::Item::Type::ROUTER)
    {
        HostConfig host = item->hostConfig();
        if (!validateHostForConnect(host))
            return;
        emit sig_connectRequested(host, defaultSessionType());
    }
    else
    {
        onLocalConnect(item->entryId());
    }
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onLocalHostContextMenu(qint64 entry_id, const QPoint& pos)
{
    QMenu menu;

    if (entry_id)
    {
        auto addProxy = [&menu](QAction* action)
        {
            menu.addAction(action->icon(), action->text(), action, &QAction::triggered);
        };

        addProxy(ui->action_desktop_connect);
        addProxy(ui->action_terminal_connect);
        addProxy(ui->action_file_transfer_connect);
        addProxy(ui->action_chat_connect);
        addProxy(ui->action_system_info_connect);
        menu.addSeparator();
        menu.addAction(ui->action_edit_host);
        menu.addAction(ui->action_copy_host);
        menu.addAction(ui->action_delete_host);
    }
    else
    {
        menu.addAction(ui->action_add_host);
    }

    menu.exec(pos);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onSearchContextMenu(const QPoint& pos)
{
    SearchWidget::Item* item = search_widget_->currentItem();
    if (!item)
        return;

    QMenu menu;
    menu.addAction(ui->action_desktop_connect);
    menu.addAction(ui->action_terminal_connect);
    menu.addAction(ui->action_file_transfer_connect);
    menu.addAction(ui->action_chat_connect);
    menu.addAction(ui->action_system_info_connect);

    // Router hosts have no address-book record to edit, copy or delete.
    if (item->type() == SearchWidget::Item::Type::LOCAL)
    {
        menu.addSeparator();
        menu.addAction(ui->action_edit_host);
        menu.addAction(ui->action_copy_host);
        menu.addAction(ui->action_delete_host);
    }

    menu.exec(pos);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onAddHost()
{
    LOG(INFO) << "[ACTION] Add host";

    qint64 group_id = ui->sidebar->currentGroupId();
    if (group_id < 0)
    {
        LOG(INFO) << "No current group";
        return;
    }

    LocalHostDialog dialog(-1, group_id, this);
    if (dialog.exec() == LocalHostDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    qint64 new_id = dialog.entryId();
    local_group_widget_->showGroup(group_id);
    local_group_widget_->setCurrentHost(new_id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onEditHost()
{
    LOG(INFO) << "[ACTION] Edit host";

    if (current_content_ == router_group_widget_)
    {
        router_group_widget_->onEditHost();
        return;
    }

    if (current_content_ == router_hosts_widget_)
    {
        router_hosts_widget_->onModifyHost();
        return;
    }

    qint64 entry_id = currentHostEntryId();
    if (entry_id <= 0)
    {
        LOG(INFO) << "No current host";
        return;
    }

    std::optional<HostConfig> host = Database::instance().findHost(entry_id);
    if (!host.has_value())
    {
        MsgBox::warning(this, tr("Failed to retrieve host information from the local database."));
        return;
    }

    LocalHostDialog dialog(entry_id, host->groupId(), this);
    if (dialog.exec() == LocalHostDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    refreshItem(entry_id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onCopyHost()
{
    LOG(INFO) << "[ACTION] Copy host";

    qint64 entry_id = currentHostEntryId();
    if (entry_id <= 0)
    {
        LOG(INFO) << "No current host";
        return;
    }

    Database& db = Database::instance();

    std::optional<HostConfig> host = db.findHost(entry_id);
    if (!host.has_value())
    {
        MsgBox::warning(this, tr("Failed to retrieve host information from the local database."));
        return;
    }

    host->setName(host->name() + " " + tr("(copy)"));

    if (!db.addHost(*host))
    {
        MsgBox::warning(this, tr("Failed to add the host to the local database."));
        return;
    }

    qint64 new_id = host->id();

    LocalHostDialog(new_id, host->groupId(), this).exec();

    if (current_content_ == search_widget_)
    {
        search_widget_->search(search_widget_->currentQuery());
        search_widget_->setCurrentHost(new_id);
    }
    else
    {
        local_group_widget_->showGroup(local_group_widget_->currentGroupId());
        local_group_widget_->setCurrentHost(new_id);
    }
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onRemoveHost()
{
    LOG(INFO) << "[ACTION] Delete host";

    qint64 entry_id = currentHostEntryId();
    if (entry_id <= 0)
    {
        LOG(INFO) << "No current host";
        return;
    }

    std::optional<HostConfig> host = Database::instance().findHost(entry_id);
    if (!host.has_value())
    {
        MsgBox::warning(this, tr("Failed to retrieve host information from the local database."));
        return;
    }

    QString message = tr("Are you sure you want to delete host \"%1\"?").arg(host->name());

    if (MsgBox::question(this, message) == MsgBox::No)
    {
        LOG(INFO) << "Action is rejected by user";
        return;
    }

    if (!Database::instance().removeHost(entry_id))
    {
        MsgBox::warning(this, tr("Unable to remove host"));
        LOG(INFO) << "Unable to remove host with id" << entry_id;
        return;
    }

    removeItem(entry_id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onUserContextMenu(const User& user, const QPoint& pos)
{
    QMenu menu;
    if (user.isValid())
    {
        menu.addAction(ui->action_edit_user);
        menu.addAction(ui->action_delete_user);
    }
    else
    {
        menu.addAction(ui->action_add_user);
    }
    menu.exec(pos);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onHostContextMenu(const QPoint& pos, int column)
{
    if (!router_hosts_widget_->hasSelectedHost())
        return;

    bool is_online = router_hosts_widget_->isSelectedHostOnline();

    QMenu menu;
    menu.addAction(ui->action_edit_host);

    if (is_online)
    {
        menu.addAction(ui->action_host_check_updates);
        menu.addAction(ui->action_disconnect);
    }

    menu.addAction(ui->action_host_remove);

    if (is_online)
    {
        menu.addSeparator();
        menu.addAction(ui->action_desktop_connect);
        menu.addAction(ui->action_terminal_connect);
        menu.addAction(ui->action_file_transfer_connect);
        menu.addAction(ui->action_chat_connect);
        menu.addAction(ui->action_system_info_connect);
    }
    menu.addSeparator();

    const QIcon copy_icon(":/img/copy.svg");
    QAction* copy_row = menu.addAction(copy_icon, tr("Copy Row"));
    QAction* copy_col = menu.addAction(copy_icon, tr("Copy Value"));

    QAction* action = menu.exec(pos);
    if (!action)
        return;

    if (action == copy_row)
        router_hosts_widget_->copyCurrentHostRow();
    else if (action == copy_col)
        router_hosts_widget_->copyCurrentHostColumn(column);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onClientContextMenu(const QPoint& pos, int column)
{
    if (!router_clients_widget_->hasSelectedClient())
        return;

    QMenu menu;
    menu.addAction(ui->action_disconnect);
    menu.addAction(ui->action_disconnect_all);
    menu.addSeparator();

    const QIcon copy_icon(":/img/copy.svg");
    QAction* copy_row = menu.addAction(copy_icon, tr("Copy Row"));
    QAction* copy_col = menu.addAction(copy_icon, tr("Copy Value"));

    QAction* action = menu.exec(pos);
    if (!action)
        return;

    if (action == copy_row)
        router_clients_widget_->copyCurrentClientRow();
    else if (action == copy_col)
        router_clients_widget_->copyCurrentClientColumn(column);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onRelayContextMenu(const QPoint& pos, int column)
{
    if (!router_relays_widget_->hasSelectedRelay())
        return;

    QMenu menu;
    menu.addAction(ui->action_disconnect);
    menu.addAction(ui->action_disconnect_all);
    menu.addSeparator();

    const QIcon copy_icon(":/img/copy.svg");
    QAction* copy_row = menu.addAction(copy_icon, tr("Copy Row"));
    QAction* copy_col = menu.addAction(copy_icon, tr("Copy Value"));

    QAction* action = menu.exec(pos);
    if (!action)
        return;

    if (action == copy_row)
        router_relays_widget_->copyCurrentRelayRow();
    else if (action == copy_col)
        router_relays_widget_->copyCurrentRelayColumn(column);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onRouterGroupContextMenu(const QPoint& pos)
{
    if (!router_group_widget_->hasSelectedHost())
        return;

    // Proxy the global action via a fresh menu entry: the local copy is always visible (so the
    // item shows up here) while the original ui->action_*_connect can stay hidden globally.
    QMenu menu;
    auto addProxy = [&menu](QAction* action)
    {
        menu.addAction(action->icon(), action->text(), action, &QAction::triggered);
    };

    addProxy(ui->action_desktop_connect);
    addProxy(ui->action_terminal_connect);
    addProxy(ui->action_file_transfer_connect);
    addProxy(ui->action_chat_connect);
    addProxy(ui->action_system_info_connect);
    if (ui->action_edit_host->isVisible())
    {
        menu.addSeparator();
        menu.addAction(ui->action_edit_host);
    }
    menu.exec(pos);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onAddUserAction()
{
    if (current_content_ == router_users_widget_)
        router_users_widget_->onAddUser();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onEditUserAction()
{
    if (current_content_ == router_users_widget_)
        router_users_widget_->onModifyUser();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onDeleteUserAction()
{
    if (current_content_ == router_users_widget_)
        router_users_widget_->onDeleteUser();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onAddWorkspaceAction()
{
    SidebarItem* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER)
        return;

    const qint64 router_id = static_cast<SidebarRouter*>(sidebar_item)->routerId();

    RouterWorkspaceDialog dialog(router_id, 0, this);
    if (dialog.exec() == QDialog::Accepted)
        ui->sidebar->onRefreshWorkspaces(router_id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onEditWorkspaceAction()
{
    SidebarItem* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_WORKSPACE)
        return;

    auto* workspace_item = static_cast<SidebarRouterWorkspace*>(sidebar_item);
    const qint64 router_id = workspace_item->routerId();

    RouterWorkspaceDialog dialog(router_id, workspace_item->workspaceId(), this);
    if (dialog.exec() == QDialog::Accepted)
        ui->sidebar->onRefreshWorkspaces(router_id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onDeleteWorkspaceAction()
{
    SidebarItem* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_WORKSPACE)
        return;

    auto* workspace_item = static_cast<SidebarRouterWorkspace*>(sidebar_item);
    const qint64 router_id = workspace_item->routerId();
    const qint64 workspace_id = workspace_item->workspaceId();

    if (MsgBox::question(this, tr("Are you sure you want to delete workspace \"%1\"?")
            .arg(workspace_item->workspaceName())) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Delete workspace rejected by user";
        return;
    }

    Router* router = Router::instance(router_id);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Delete workspace accepted by user";
    router->deleteWorkspace(workspace_id, this,
        [this, router_id](const proto::router::WorkspaceResult& result)
    {
        if (result.error_code() != proto::router::kErrorOk)
            LOG(ERROR) << "Workspace delete failed:" << result.error_code();
        ui->sidebar->onRefreshWorkspaces(router_id);
    });
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onAddGroupAction()
{
    SidebarItem* item = ui->sidebar->currentItem();
    if (!item)
        return;

    if (item->itemType() == SidebarItem::LOCAL_GROUP)
    {
        ui->sidebar->onAddGroup();
        return;
    }

    // For a workspace item the new group goes to the workspace root (parent 0); for a host
    // group it becomes its subgroup.
    qint64 router_id = 0;
    qint64 workspace_id = 0;
    qint64 default_parent_id = 0;
    QString workspace_name;

    if (item->itemType() == SidebarItem::ROUTER_WORKSPACE)
    {
        auto* workspace_item = static_cast<SidebarRouterWorkspace*>(item);
        router_id = workspace_item->routerId();
        workspace_id = workspace_item->workspaceId();
        workspace_name = workspace_item->workspaceName();
        default_parent_id = 0;
    }
    else if (item->itemType() == SidebarItem::ROUTER_GROUP)
    {
        auto* group_item = static_cast<SidebarRouterGroup*>(item);
        router_id = group_item->routerId();
        workspace_id = group_item->workspaceId();
        workspace_name = group_item->workspaceName();
        default_parent_id = group_item->groupId();
    }
    else
    {
        return;
    }

    RouterGroupDialog dialog(router_id, workspace_id, workspace_name, 0, default_parent_id, this);
    if (dialog.exec() == QDialog::Accepted)
        ui->sidebar->onRefreshHostGroups(router_id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onEditGroupAction()
{
    SidebarItem* item = ui->sidebar->currentItem();
    if (!item)
        return;

    if (item->itemType() == SidebarItem::LOCAL_GROUP)
    {
        ui->sidebar->onEditGroup();
        return;
    }

    if (item->itemType() != SidebarItem::ROUTER_GROUP)
        return;

    auto* group_item = static_cast<SidebarRouterGroup*>(item);

    const qint64 router_id = group_item->routerId();
    RouterGroupDialog dialog(router_id, group_item->workspaceId(), group_item->workspaceName(),
        group_item->groupId(), 0, this);
    if (dialog.exec() == QDialog::Accepted)
        ui->sidebar->onRefreshHostGroups(router_id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onDeleteGroupAction()
{
    SidebarItem* item = ui->sidebar->currentItem();
    if (!item)
        return;

    if (item->itemType() == SidebarItem::LOCAL_GROUP)
    {
        ui->sidebar->onRemoveGroup();
        return;
    }

    if (item->itemType() != SidebarItem::ROUTER_GROUP)
        return;

    auto* group_item = static_cast<SidebarRouterGroup*>(item);

    const QString question = tr("Are you sure you want to delete the group \"%1\"? "
                                "Hosts assigned to this group or its subgroups will be moved "
                                "to the workspace root.").arg(group_item->text(0));
    if (MsgBox::question(this, question) == MsgBox::No)
        return;

    const qint64 router_id = group_item->routerId();
    Router* router = Router::instance(router_id);
    if (!router)
        return;

    router->deleteGroup(group_item->workspaceId(), group_item->groupId(), this,
        [this, router_id](const proto::router::GroupResult& result)
    {
        if (result.error_code() != proto::router::kErrorOk)
        {
            LOG(ERROR) << "Group delete failed:" << result.error_code();
            return;
        }
        ui->sidebar->onRefreshHostGroups(router_id);
    });
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onChangeRouterPassword()
{
    SidebarItem* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER)
        return;

    SidebarRouter* router = static_cast<SidebarRouter*>(sidebar_item);
    Router* instance = Router::instance(router->routerId());
    if (instance && instance->status() == Router::Status::ONLINE)
        ui->sidebar->changeRouterPassword(router->routerId());
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onClearRouterEvents()
{
    SidebarItem* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER)
        return;

    const qint64 router_id = static_cast<SidebarRouter*>(sidebar_item)->routerId();
    ui->sidebar->clearRouterEvents(router_id);
    router_status_widget_->showRouter(router_id, ui->sidebar->routerEvents(router_id));
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onImportOldBookAction()
{
    LOG(INFO) << "[ACTION] Import address book";

    SidebarItem* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != SidebarItem::LOCAL_GROUP)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    QString file_path = QFileDialog::getOpenFileName(
        this,
        tr("Import Old Address Book"),
        QString(),
        tr("Address Book (*.aab);;All files (*)"));

    if (file_path.isEmpty())
    {
        LOG(INFO) << "[ACTION] Cancelled by user";
        return;
    }

    if (!AabImporter::import(this, file_path))
        return;

    reloadRouters();
    ui->sidebar->reloadGroups();
    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onExportBookAction()
{
    LOG(INFO) << "[ACTION] Export address book";

    QString file_path = QFileDialog::getSaveFileName(
        this,
        tr("Export Address Book"),
        QString(),
        tr("Address Book (*.json);;All files (*)"));

    if (file_path.isEmpty())
    {
        LOG(INFO) << "[ACTION] Cancelled by user";
        return;
    }

    if (!Database::instance().isValid())
    {
        MsgBox::warning(this, tr("Address book database is not available."));
        return;
    }

    CredentialsDialog dialog(CredentialsDialog::Type::SET_PASSWORD, this);
    dialog.setWindowTitle(tr("Export Address Book"));
    dialog.setHeaderIcon(":/img/lock.svg");
    dialog.setHeaderText(tr("Enter a password to encrypt the address book."));
    dialog.setValidator([](CredentialsDialog* d) -> bool
    {
        if (!MasterPassword::isSafePassword(d->password()))
        {
            QString unsafe = tr("Password you entered does not meet the security requirements!");
            QString safe = tr("The password must contain lowercase and uppercase characters, "
                              "numbers and should not be shorter than %n characters.",
                              "", MasterPassword::kSafePasswordLength);
            QString question = tr("Do you want to enter a different password?");

            if (MsgBox::warning(d, QString("<b>%1</b><br/>%2<br/>%3").arg(unsafe, safe, question),
                                MsgBox::Yes | MsgBox::No) == MsgBox::Yes)
                return false;
        }
        return true;
    });

    if (dialog.exec() != QDialog::Accepted)
        return;

    JsonBackup::ExportCounts counts;
    const JsonBackup::Result result = JsonBackup::exportToFile(file_path, dialog.password(), &counts);

    if (result != JsonBackup::Result::SUCCESS)
    {
        MsgBox::warning(this, tr("Failed to export the address book."));
        return;
    }

    MsgBox::information(this,
        tr("Export completed successfully.\n"
           "Routers exported: %1\n"
           "Groups exported: %2\n"
           "Hosts exported: %3")
            .arg(counts.routers).arg(counts.groups).arg(counts.hosts));
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onImportBookAction()
{
    LOG(INFO) << "[ACTION] Import address book (json)";

    SidebarItem* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != SidebarItem::LOCAL_GROUP)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    QString file_path = QFileDialog::getOpenFileName(
        this,
        tr("Import Address Book"),
        QString(),
        tr("Address Book (*.json);;All files (*)"));

    if (file_path.isEmpty())
    {
        LOG(INFO) << "[ACTION] Cancelled by user";
        return;
    }

    CredentialsDialog dialog(CredentialsDialog::Type::ENTER_PASSWORD, this);
    dialog.setWindowTitle(tr("Unlock"));
    dialog.setHeaderIcon(":/img/lock.svg");
    dialog.setHeaderText(tr("Address book is encrypted. To open, you must enter a password."));
    dialog.setShowPasswordButtonVisible(true);

    if (dialog.exec() != QDialog::Accepted)
        return;

    JsonBackup::ImportCounts counts;
    const JsonBackup::Result result = JsonBackup::importFromFile(file_path, dialog.password(), &counts);

    switch (result)
    {
        case JsonBackup::Result::SUCCESS:
            break;

        case JsonBackup::Result::WRONG_PASSWORD:
            MsgBox::warning(this, tr("Unable to decrypt the file with the specified password."));
            return;

        case JsonBackup::Result::UNSUPPORTED_VERSION:
            MsgBox::warning(this, tr("Unsupported file format version."));
            return;

        case JsonBackup::Result::NOTHING_IMPORTED:
            MsgBox::information(this, tr("Nothing was imported."));
            return;

        default:
            MsgBox::warning(this, tr("The file is not a valid address book."));
            return;
    }

    MsgBox::information(this,
        tr("Import completed successfully.\n"
           "Routers added: %1\n"
           "Routers skipped: %2\n"
           "Groups added: %3\n"
           "Groups skipped: %4\n"
           "Hosts added: %5\n"
           "Hosts skipped: %6")
            .arg(counts.routers).arg(counts.routers_skipped)
            .arg(counts.groups).arg(counts.groups_skipped)
            .arg(counts.hosts).arg(counts.hosts_skipped));

    reloadRouters();
    ui->sidebar->reloadGroups();
    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onReloadAction()
{
    if (current_content_ && current_content_->canReload())
        current_content_->reload();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onSaveAction()
{
    if (current_content_ && current_content_->canSave())
        current_content_->save();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onDisconnectAction()
{
    if (current_content_ == router_clients_widget_)
    {
        router_clients_widget_->onDisconnectClient();
        return;
    }

    if (current_content_ == router_relays_widget_)
    {
        router_relays_widget_->onDisconnectRelay();
        return;
    }

    if (current_content_ == router_hosts_widget_)
        router_hosts_widget_->onDisconnectHost();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onDisconnectAllAction()
{
    if (current_content_ == router_clients_widget_)
    {
        router_clients_widget_->onDisconnectAllClients();
        return;
    }

    if (current_content_ == router_relays_widget_)
    {
        router_relays_widget_->onDisconnectAllRelays();
        return;
    }

    if (current_content_ == router_hosts_widget_)
        router_hosts_widget_->onDisconnectAllHosts();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onRemoveHostAction()
{
    if (current_content_ == router_hosts_widget_)
        router_hosts_widget_->onRemoveHost();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onCheckHostUpdatesAction()
{
    if (current_content_ == router_hosts_widget_)
        router_hosts_widget_->onCheckHostUpdates();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onOnlineCheckToggled(bool checked)
{
    Settings settings;
    settings.setOnlineCheckEnabled(checked);
    local_group_widget_->setOnlineCheckEnabled(checked);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::switchContent(ContentWidget* new_widget)
{
    if (!new_widget || new_widget == current_content_)
        return;

    if (current_content_ && statusbar_)
        current_content_->deactivate(statusbar_);

    current_content_ = new_widget;
    ui->content_stack->setCurrentWidget(new_widget);

    if (statusbar_)
        current_content_->activate(statusbar_);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::updateActionsState()
{
    ui->action_add_group->setVisible(false);
    ui->action_delete_group->setVisible(false);
    ui->action_edit_group->setVisible(false);

    ui->action_add_router->setVisible(true);
    ui->action_edit_router->setVisible(false);
    ui->action_delete_router->setVisible(false);
    ui->action_change_router_password->setVisible(false);
    ui->action_clear_router_events->setVisible(false);

    ui->action_add_host->setVisible(false);
    ui->action_delete_host->setVisible(false);
    ui->action_edit_host->setVisible(false);
    ui->action_copy_host->setVisible(false);

    ui->action_desktop->setVisible(false);
    ui->action_file_transfer->setVisible(false);
    ui->action_chat->setVisible(false);
    ui->action_system_info->setVisible(false);
    ui->action_terminal->setVisible(false);

    ui->action_add_user->setVisible(false);
    ui->action_edit_user->setVisible(false);
    ui->action_delete_user->setVisible(false);

    ui->action_add_workspace->setVisible(false);
    ui->action_edit_workspace->setVisible(false);
    ui->action_delete_workspace->setVisible(false);

    ui->action_reload->setVisible(false);
    ui->action_save->setVisible(false);
    ui->action_import_old_book->setVisible(false);
    ui->action_export_book->setVisible(false);
    ui->action_import_book->setVisible(false);
    ui->action_disconnect->setVisible(false);
    ui->action_disconnect_all->setVisible(false);
    ui->action_host_remove->setVisible(false);
    ui->action_host_check_updates->setVisible(false);
    ui->action_online_check->setVisible(false);
    ui->action_desktop_connect->setVisible(false);
    ui->action_file_transfer_connect->setVisible(false);
    ui->action_chat_connect->setVisible(false);
    ui->action_system_info_connect->setVisible(false);
    ui->action_terminal_connect->setVisible(false);

    SidebarItem* sidebar_item = ui->sidebar->currentItem();

    if (current_content_ == search_widget_)
    {
        SearchWidget::Item* host_item = search_widget_->currentItem();
        const bool has_item = host_item != nullptr;
        const bool is_local_host = has_item && host_item->type() == SearchWidget::Item::Type::LOCAL;

        // Address-book operations apply to local hosts only; router hosts can only be connected.
        ui->action_delete_host->setVisible(is_local_host);
        ui->action_edit_host->setVisible(is_local_host);
        ui->action_copy_host->setVisible(is_local_host);

        ui->action_desktop_connect->setVisible(has_item);
        ui->action_file_transfer_connect->setVisible(has_item);
        ui->action_chat_connect->setVisible(has_item);
        ui->action_system_info_connect->setVisible(has_item);
        ui->action_terminal_connect->setVisible(has_item);
    }
    else if (sidebar_item && sidebar_item->itemType() == SidebarItem::LOCAL_GROUP)
    {
        ui->action_online_check->setVisible(true);
        ui->action_import_old_book->setVisible(true);
        ui->action_export_book->setVisible(true);
        ui->action_import_book->setVisible(true);

        ui->action_add_group->setVisible(true);
        ui->action_delete_group->setVisible(sidebar_item->groupId() != 0);
        ui->action_edit_group->setVisible(sidebar_item->groupId() != 0);

        LocalGroupWidget::Item* host_item = local_group_widget_->currentItem();

        ui->action_add_host->setVisible(true);
        ui->action_delete_host->setVisible(host_item != nullptr);
        ui->action_edit_host->setVisible(host_item != nullptr);
        ui->action_copy_host->setVisible(host_item != nullptr);
    }
    else if (sidebar_item && sidebar_item->itemType() == SidebarItem::ROUTER_WORKSPACE)
    {
        auto* workspace_item = static_cast<SidebarRouterWorkspace*>(sidebar_item);

        proto::router::SessionType session_type = proto::router::SESSION_TYPE_CLIENT;
        if (Router* router = Router::instance(workspace_item->routerId()))
            session_type = router->config().sessionType();

        // Clients are read-only and cannot manage host groups or workspaces.
        const bool can_manage = session_type != proto::router::SESSION_TYPE_CLIENT;
        ui->action_add_group->setVisible(can_manage);
        ui->action_edit_workspace->setVisible(can_manage);
        ui->action_delete_workspace->setVisible(can_manage);
    }
    else if (sidebar_item && sidebar_item->itemType() == SidebarItem::ROUTER_GROUP)
    {
        auto* group_item = static_cast<SidebarRouterGroup*>(sidebar_item);

        proto::router::SessionType session_type = proto::router::SESSION_TYPE_CLIENT;
        if (Router* router = Router::instance(group_item->routerId()))
            session_type = router->config().sessionType();

        // Clients are read-only and cannot manage host groups.
        const bool can_manage_groups = session_type != proto::router::SESSION_TYPE_CLIENT;
        ui->action_add_group->setVisible(can_manage_groups);
        ui->action_edit_group->setVisible(can_manage_groups);
        ui->action_delete_group->setVisible(can_manage_groups);
    }
    else if (sidebar_item && sidebar_item->itemType() == SidebarItem::ROUTER_USERS)
    {
        const bool has_user = router_users_widget_->hasSelectedUser();
        ui->action_add_user->setVisible(true);
        ui->action_edit_user->setVisible(has_user);
        ui->action_delete_user->setVisible(has_user);
    }
    else if (sidebar_item && sidebar_item->itemType() == SidebarItem::ROUTER_CLIENTS)
    {
        ui->action_disconnect->setVisible(router_clients_widget_->hasSelectedClient());
        ui->action_disconnect_all->setVisible(router_clients_widget_->clientCount() > 0);
    }
    else if (sidebar_item && sidebar_item->itemType() == SidebarItem::ROUTER_RELAYS)
    {
        ui->action_disconnect->setVisible(router_relays_widget_->hasSelectedRelay());
        ui->action_disconnect_all->setVisible(router_relays_widget_->relayCount() > 0);
    }
    else if (sidebar_item && sidebar_item->itemType() == SidebarItem::ROUTER_HOSTS)
    {
        const bool has_host = router_hosts_widget_->hasSelectedHost();
        const bool can_connect = router_hosts_widget_->isSelectedHostOnline();

        ui->action_disconnect->setVisible(can_connect);
        ui->action_disconnect_all->setVisible(router_hosts_widget_->hostCount() > 0);
        ui->action_host_remove->setVisible(has_host);
        ui->action_edit_host->setVisible(has_host);

        ui->action_host_check_updates->setVisible(can_connect);
        ui->action_desktop_connect->setVisible(can_connect);
        ui->action_file_transfer_connect->setVisible(can_connect);
        ui->action_chat_connect->setVisible(can_connect);
        ui->action_system_info_connect->setVisible(can_connect);
        ui->action_terminal_connect->setVisible(can_connect);
    }

    if (sidebar_item && sidebar_item->itemType() == SidebarItem::ROUTER)
    {
        ui->action_edit_router->setVisible(true);
        ui->action_delete_router->setVisible(true);
        ui->action_clear_router_events->setVisible(true);

        // Workspaces are managed from the sidebar tree. Adding one targets the whole router, so
        // its action lives on the router node when connected as an administrator.
        SidebarRouter* router = static_cast<SidebarRouter*>(sidebar_item);
        Router* instance = Router::instance(router->routerId());
        const bool is_online = instance && instance->status() == Router::Status::ONLINE;
        const bool is_admin_online = is_online &&
            instance->config().sessionType() == proto::router::SESSION_TYPE_ADMIN;
        ui->action_add_workspace->setVisible(is_admin_online);
        ui->action_change_router_password->setVisible(is_online);
    }
    else
    {
        ui->action_desktop->setVisible(true);
        ui->action_file_transfer->setVisible(true);
        ui->action_chat->setVisible(true);
        ui->action_system_info->setVisible(true);
        ui->action_terminal->setVisible(true);
    }

    if (current_content_ != search_widget_ && sidebar_item &&
        (sidebar_item->itemType() == SidebarItem::ROUTER_GROUP ||
         sidebar_item->itemType() == SidebarItem::ROUTER_WORKSPACE))
    {
        proto::router::SessionType session_type = proto::router::SESSION_TYPE_CLIENT;
        Router* router = Router::instance(router_group_widget_->routerId());
        if (router)
            session_type = router->config().sessionType();

        const bool has_selection = router_group_widget_->hasSelectedHost();
        const bool can_edit = has_selection && session_type != proto::router::SESSION_TYPE_CLIENT;
        ui->action_edit_host->setVisible(can_edit);
    }

    ui->action_reload->setVisible(current_content_ && current_content_->canReload());
    ui->action_save->setVisible(current_content_ && current_content_->canSave());
}

//--------------------------------------------------------------------------------------------------
proto::peer::SessionType HostsTab::defaultSessionType() const
{
    if (ui->action_desktop->isChecked())
        return proto::peer::SESSION_TYPE_DESKTOP;
    else if (ui->action_file_transfer->isChecked())
        return proto::peer::SESSION_TYPE_FILE_TRANSFER;
    else if (ui->action_chat->isChecked())
        return proto::peer::SESSION_TYPE_CHAT;
    else if (ui->action_system_info->isChecked())
        return proto::peer::SESSION_TYPE_SYSTEM_INFO;
    else if (ui->action_terminal->isChecked())
        return proto::peer::SESSION_TYPE_TERMINAL;
    else
        return proto::peer::SESSION_TYPE_UNKNOWN;
}

//--------------------------------------------------------------------------------------------------
bool HostsTab::validateHostForConnect(const HostConfig& host)
{
    if (host.routerId() != 0)
    {
        std::optional<RouterConfig> router = Database::instance().findRouter(host.routerId());
        if (!router.has_value())
        {
            MsgBox::warning(this, tr("The router associated with this host has been deleted. "
                "Edit the host to select another router or switch to direct connection."));
            return false;
        }

        if (!isHostId(host.address()))
        {
            MsgBox::warning(this, tr("The host has an invalid host ID."));
            return false;
        }
    }
    else
    {
        Address address = Address::fromString(host.address(), DEFAULT_HOST_TCP_PORT);
        if (!address.isValid())
        {
            MsgBox::warning(this, tr("The host has an incorrect address."));
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
qint64 HostsTab::currentHostEntryId() const
{
    if (current_content_ == local_group_widget_)
    {
        LocalGroupWidget::Item* item = local_group_widget_->currentItem();
        return item ? item->entryId() : -1;
    }

    if (current_content_ == search_widget_)
    {
        SearchWidget::Item* item = search_widget_->currentItem();
        return item ? item->entryId() : -1;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
void HostsTab::refreshItem(qint64 entry_id)
{
    if (current_content_ == local_group_widget_)
        local_group_widget_->refreshItem(entry_id);
    else if (current_content_ == search_widget_)
        search_widget_->refreshItem(entry_id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::removeItem(qint64 entry_id)
{
    if (current_content_ == local_group_widget_)
        local_group_widget_->removeItem(entry_id);
    else if (current_content_ == search_widget_)
        search_widget_->removeItem(entry_id);
}
