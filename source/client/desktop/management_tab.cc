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

#include "client/desktop/management_tab.h"

#include <QActionGroup>
#include <QDateTime>
#include <QEvent>
#include <QMenu>
#include <QPointer>
#include <QTimer>

#include "base/auto_qpointer.h"
#include "base/build_config.h"
#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "base/peer/user.h"
#include "client/desktop/file_dialog.h"
#include "client/desktop/ui_management_tab.h"
#include "client/desktop/management/content_widget.h"
#include "client/desktop/management/local_group_widget.h"
#include "client/desktop/management/local_host_dialog.h"
#include "client/desktop/management/router_access_log_widget.h"
#include "client/desktop/management/router_clients_widget.h"
#include "client/desktop/management/router_group_dialog.h"
#include "client/desktop/management/router_group_widget.h"
#include "client/desktop/management/router_host_dialog.h"
#include "client/desktop/management/router_hosts_widget.h"
#include "client/desktop/management/router_relays_widget.h"
#include "client/desktop/management/router_status_widget.h"
#include "client/desktop/management/router_temp_hosts_widget.h"
#include "client/desktop/management/router_users_widget.h"
#include "client/desktop/management/router_workspace_dialog.h"
#include "client/desktop/management/search_widget.h"
#include "client/router/router_controller.h"
#include "client/storage/backup.h"
#include "client/storage/local_host_store.h"
#include "client/storage/router_store.h"
#include "client/storage/user_settings.h"
#include "common/desktop/credentials_dialog.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/session_type.h"
#include "common/desktop/two_factor_code_dialog.h"
#include "common/desktop/two_factor_enroll_dialog.h"
#include "proto/peer.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"

namespace {

const char kLocalGroupKey[] = "local_group";
const char kRouterGroupKey[] = "router_group";
const char kRouterHostsKey[] = "router_hosts";
const char kRouterTempHostsKey[] = "router_temp_hosts";
const char kRouterQsHostsKey[] = "router_qs_hosts";
const char kRouterUsersKey[] = "router_users";
const char kRouterClientsKey[] = "router_clients";
const char kRouterRelaysKey[] = "router_relays";
const char kRouterAccessLogKey[] = "router_access_log";
const char kRouterStatusKey[] = "router_status";
const char kSearchKey[] = "search";
const char kSplitterKey[] = "splitter";
const char kSidebarKey[] = "sidebar";

//--------------------------------------------------------------------------------------------------
HostConfig hostForRouterRow(const SearchResultModel::Row& row)
{
    return HostConfig::forRouterHost(stringToHostId(row.host.address()), row.host.name());
}

} // namespace

//--------------------------------------------------------------------------------------------------
ManagementTab::ManagementTab(QWidget* parent)
    : Tab(Type::HOSTS, "hosts", parent),
      ui(std::make_unique<Ui::ManagementTab>())
{
    LOG(INFO) << "Ctor";

    if (!Database::instance().isValid())
        LOG(ERROR) << "Failed to open or create book database";

    // The sidebar built by setupUi() below expects the session owner to be up already.
    new RouterController(this);

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

    UserSettings settings;

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

    ui->action_export_book->setProperty(Tab::kMenuOnlyProperty, true);
    ui->action_import_book->setProperty(Tab::kMenuOnlyProperty, true);
    ui->action_online_check->setProperty(Tab::kMenuOnlyProperty, true);

    // Create content widgets.
    local_group_widget_ = new LocalGroupWidget(this);
    router_group_widget_ = new RouterGroupWidget(this);
    router_hosts_widget_ = new RouterHostsWidget(this);
    router_temp_hosts_widget_ = new RouterTempHostsWidget(/* portable */ false, this);
    router_qs_hosts_widget_ = new RouterTempHostsWidget(/* portable */ true, this);
    router_users_widget_ = new RouterUsersWidget(this);
    router_clients_widget_ = new RouterClientsWidget(this);
    router_relays_widget_ = new RouterRelaysWidget(this);
    router_access_log_widget_ = new RouterAccessLogWidget(this);
    router_status_widget_ = new RouterStatusWidget(this);
    search_widget_ = new SearchWidget(this);

    ui->content_stack->addWidget(local_group_widget_);
    ui->content_stack->addWidget(router_group_widget_);
    ui->content_stack->addWidget(router_hosts_widget_);
    ui->content_stack->addWidget(router_temp_hosts_widget_);
    ui->content_stack->addWidget(router_qs_hosts_widget_);
    ui->content_stack->addWidget(router_users_widget_);
    ui->content_stack->addWidget(router_clients_widget_);
    ui->content_stack->addWidget(router_relays_widget_);
    ui->content_stack->addWidget(router_access_log_widget_);
    ui->content_stack->addWidget(router_status_widget_);
    ui->content_stack->addWidget(search_widget_);

    RouterController& controller = RouterController::instance();
    connect(&controller, &RouterController::sig_event,
            router_status_widget_, &RouterStatusWidget::onEvent);
    connect(&controller, &RouterController::sig_statusChanged,
            router_status_widget_, &RouterStatusWidget::onStatusChanged);
    connect(&controller, &RouterController::sig_twoFactorRequired,
            this, &ManagementTab::onTwoFactorRequired);
    connect(&controller, &RouterController::sig_statusChanged,
            this, &ManagementTab::updateActionsState);

    // The button of the status widget asks the question of the router again.
    connect(router_status_widget_, &RouterStatusWidget::sig_twoFactorClicked,
            this, &ManagementTab::onTwoFactorRequired);

    connect(router_hosts_widget_, &RouterHostsWidget::sig_currentChanged,
            this, &ManagementTab::updateActionsState);
    connect(router_hosts_widget_, &RouterHostsWidget::sig_contextMenu,
            this, &ManagementTab::onHostContextMenu);
    connect(router_hosts_widget_, &RouterHostsWidget::sig_activated,
            this, &ManagementTab::onRouterHostConnect);

    connect(router_temp_hosts_widget_, &RouterTempHostsWidget::sig_currentChanged,
            this, &ManagementTab::updateActionsState);
    connect(router_temp_hosts_widget_, &RouterTempHostsWidget::sig_contextMenu,
            this, &ManagementTab::onTempHostContextMenu);
    connect(router_temp_hosts_widget_, &RouterTempHostsWidget::sig_activated,
            this, &ManagementTab::onTempHostConnect);

    connect(router_qs_hosts_widget_, &RouterTempHostsWidget::sig_currentChanged,
            this, &ManagementTab::updateActionsState);
    connect(router_qs_hosts_widget_, &RouterTempHostsWidget::sig_contextMenu,
            this, &ManagementTab::onTempHostContextMenu);
    connect(router_qs_hosts_widget_, &RouterTempHostsWidget::sig_activated,
            this, &ManagementTab::onTempHostConnect);

    connect(router_users_widget_, &RouterUsersWidget::sig_currentChanged,
            this, &ManagementTab::updateActionsState);
    connect(router_users_widget_, &RouterUsersWidget::sig_contextMenu,
            this, &ManagementTab::onUserContextMenu);

    connect(router_clients_widget_, &RouterClientsWidget::sig_currentChanged,
            this, &ManagementTab::updateActionsState);
    connect(router_clients_widget_, &RouterClientsWidget::sig_contextMenu,
            this, &ManagementTab::onClientContextMenu);

    connect(router_relays_widget_, &RouterRelaysWidget::sig_currentChanged,
            this, &ManagementTab::updateActionsState);
    connect(router_relays_widget_, &RouterRelaysWidget::sig_contextMenu,
            this, &ManagementTab::onRelayContextMenu);

    connect(router_access_log_widget_, &RouterAccessLogWidget::sig_contextMenu,
            this, &ManagementTab::onAccessLogContextMenu);

    // Setup drag-and-drop: pass the host mime types from the source widgets to Sidebar. The list
    // of the approved hosts drags the same records as the group view, so it shares its type.
    ui->sidebar->setLocalHostMimeType(local_group_widget_->mimeType());
    ui->sidebar->setRouterHostMimeType(router_group_widget_->mimeType());
    router_hosts_widget_->setMimeType(router_group_widget_->mimeType());

    // Connect signals.
    connect(ui->sidebar, &Sidebar::sig_switchContent, this, &ManagementTab::onSwitchContent);
    connect(ui->sidebar, &Sidebar::sig_contextMenu, this, &ManagementTab::onSidebarContextMenu);
    connect(ui->sidebar, &Sidebar::sig_addGroup, this, &ManagementTab::onAddGroupAction);
    connect(ui->sidebar, &Sidebar::sig_removeGroup, this, &ManagementTab::onDeleteGroupAction);
    connect(ui->sidebar, &Sidebar::sig_editGroup, this, &ManagementTab::onEditGroupAction);
    connect(ui->sidebar, &Sidebar::sig_itemDropped, this, [this]()
    {
        local_group_widget_->showGroup(ui->sidebar->currentGroupId());
        updateActionsState();
    });
    connect(local_group_widget_, &LocalGroupWidget::sig_currentChanged, this, &ManagementTab::onCurrentHostChanged);
    connect(local_group_widget_, &LocalGroupWidget::sig_activated, this, &ManagementTab::onLocalConnect);
    connect(local_group_widget_, &LocalGroupWidget::sig_contextMenu, this, &ManagementTab::onLocalHostContextMenu);
    connect(local_group_widget_, &LocalGroupWidget::sig_addHost, this, &ManagementTab::onAddHost);
    connect(local_group_widget_, &LocalGroupWidget::sig_deleteHost, this, &ManagementTab::onRemoveHost);
    connect(local_group_widget_, &LocalGroupWidget::sig_editHost, this, &ManagementTab::onEditHost);
    connect(router_group_widget_, &RouterGroupWidget::sig_currentChanged, this, &ManagementTab::updateActionsState);
    connect(router_group_widget_, &RouterGroupWidget::sig_contextMenu, this, &ManagementTab::onRouterGroupContextMenu);
    connect(router_group_widget_, &RouterGroupWidget::sig_activated, this, &ManagementTab::onRouterGroupConnect);
    connect(ui->action_add_host, &QAction::triggered, this, &ManagementTab::onAddHost);
    connect(ui->action_edit_host, &QAction::triggered, this, &ManagementTab::onEditHost);
    connect(ui->action_copy_host, &QAction::triggered, this, &ManagementTab::onCopyHost);
    connect(ui->action_delete_host, &QAction::triggered, this, &ManagementTab::onRemoveHost);
    connect(search_widget_, &SearchWidget::sig_currentChanged, this, &ManagementTab::updateActionsState);
    connect(search_widget_, &SearchWidget::sig_activated, this, &ManagementTab::onSearchConnect);
    connect(search_widget_, &SearchWidget::sig_contextMenu, this, &ManagementTab::onSearchContextMenu);
    connect(ui->action_add_group, &QAction::triggered, this, &ManagementTab::onAddGroupAction);
    connect(ui->action_edit_group, &QAction::triggered, this, &ManagementTab::onEditGroupAction);
    connect(ui->action_delete_group, &QAction::triggered, this, &ManagementTab::onDeleteGroupAction);
    connect(ui->action_change_router_password, &QAction::triggered, this, &ManagementTab::onChangeRouterPassword);
    connect(ui->action_clear_router_events, &QAction::triggered, this, &ManagementTab::onClearRouterEvents);
    connect(ui->sidebar, &Sidebar::sig_routerHostMoved, this, [this]()
    {
        if (current_content_ == router_group_widget_)
            router_group_widget_->reload();
        else if (current_content_ == router_hosts_widget_)
            router_hosts_widget_->reload();
    });
    connect(ui->sidebar, &Sidebar::sig_routerGroupMoved, ui->sidebar, &Sidebar::onRefreshHostGroups);
    connect(ui->action_add_user, &QAction::triggered, this, &ManagementTab::onAddUserAction);
    connect(ui->action_edit_user, &QAction::triggered, this, &ManagementTab::onEditUserAction);
    connect(ui->action_delete_user, &QAction::triggered, this, &ManagementTab::onDeleteUserAction);
    connect(ui->action_add_workspace, &QAction::triggered, this, &ManagementTab::onAddWorkspaceAction);
    connect(ui->action_edit_workspace, &QAction::triggered, this, &ManagementTab::onEditWorkspaceAction);
    connect(ui->action_delete_workspace, &QAction::triggered, this, &ManagementTab::onDeleteWorkspaceAction);
    connect(ui->action_reload, &QAction::triggered, this, &ManagementTab::onReloadAction);
    connect(ui->action_save, &QAction::triggered, this, &ManagementTab::onSaveAction);
    connect(ui->action_export_book, &QAction::triggered, this, &ManagementTab::onExportBookAction);
    connect(ui->action_import_book, &QAction::triggered, this, &ManagementTab::onImportBookAction);
    connect(ui->action_disconnect, &QAction::triggered, this, &ManagementTab::onDisconnectAction);
    connect(ui->action_host_remove, &QAction::triggered, this, &ManagementTab::onRemoveHostAction);
    connect(ui->action_host_approve, &QAction::triggered, this, &ManagementTab::onApproveHostAction);
    connect(ui->action_host_check_updates, &QAction::triggered, this, &ManagementTab::onCheckHostUpdatesAction);
    connect(ui->action_host_telemetry, &QAction::triggered, this, &ManagementTab::onHostTelemetryAction);
    connect(ui->action_online_check, &QAction::toggled, this, &ManagementTab::onOnlineCheckToggled);
    connect(session_connect_group, &QActionGroup::triggered, this, &ManagementTab::onConnectAction);

    // Register actions for toolbar and menus.
    addActions(ActionRole::FILE,
    {
        ui->action_save, ui->action_export_book, ui->action_import_book
    });
    addActions(ActionRole::EDIT, { ui->action_add_user, ui->action_edit_user, ui->action_delete_user });
    addActions(ActionRole::EDIT,
    {
        ui->action_add_workspace, ui->action_edit_workspace, ui->action_delete_workspace
    });
    addActions(ActionRole::EDIT,
    {
        ui->action_change_router_password, ui->action_clear_router_events
    });
    addActions(ActionRole::EDIT,
    {
        ui->action_add_group, ui->action_edit_group, ui->action_delete_group
    });
    addActions(ActionRole::EDIT,
    {
        ui->action_add_host, ui->action_edit_host, ui->action_copy_host, ui->action_delete_host,
        ui->action_host_check_updates, ui->action_host_telemetry, ui->action_host_approve,
        ui->action_host_remove, ui->action_disconnect
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

    // A question asked while the application was locked had no window to show it.
    QTimer::singleShot(0, this, [this]() { showNextTwoFactorPrompt(); });
}

//--------------------------------------------------------------------------------------------------
ManagementTab::~ManagementTab()
{
    LOG(INFO) << "Dtor";

    delete two_factor_dialog_;

    UserSettings().setSessionType(defaultSessionType());
}

//--------------------------------------------------------------------------------------------------
QVariantMap ManagementTab::saveState()
{
    QVariantMap state;
    state.insert(kLocalGroupKey, local_group_widget_->saveState());
    state.insert(kRouterGroupKey, router_group_widget_->saveState());
    state.insert(kRouterHostsKey, router_hosts_widget_->saveState());
    state.insert(kRouterTempHostsKey, router_temp_hosts_widget_->saveState());
    state.insert(kRouterQsHostsKey, router_qs_hosts_widget_->saveState());
    state.insert(kRouterUsersKey, router_users_widget_->saveState());
    state.insert(kRouterClientsKey, router_clients_widget_->saveState());
    state.insert(kRouterRelaysKey, router_relays_widget_->saveState());
    state.insert(kRouterAccessLogKey, router_access_log_widget_->saveState());
    state.insert(kRouterStatusKey, router_status_widget_->saveState());
    state.insert(kSearchKey, search_widget_->saveState());
    state.insert(kSplitterKey, ui->splitter->saveState());
    state.insert(kSidebarKey, ui->sidebar->saveState());
    return state;
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::restoreState(const QVariantMap& state)
{
    local_group_widget_->restoreState(state.value(kLocalGroupKey).toMap());
    router_group_widget_->restoreState(state.value(kRouterGroupKey).toMap());
    router_hosts_widget_->restoreState(state.value(kRouterHostsKey).toMap());
    router_temp_hosts_widget_->restoreState(state.value(kRouterTempHostsKey).toMap());
    router_qs_hosts_widget_->restoreState(state.value(kRouterQsHostsKey).toMap());
    router_users_widget_->restoreState(state.value(kRouterUsersKey).toMap());
    router_clients_widget_->restoreState(state.value(kRouterClientsKey).toMap());

    const QVariantMap router_relays_state = state.value(kRouterRelaysKey).toMap();
    if (!router_relays_state.isEmpty())
        router_relays_widget_->restoreState(router_relays_state);

    router_access_log_widget_->restoreState(state.value(kRouterAccessLogKey).toMap());
    router_status_widget_->restoreState(state.value(kRouterStatusKey).toMap());
    search_widget_->restoreState(state.value(kSearchKey).toMap());

    const QByteArray splitter_state = state.value(kSplitterKey).toByteArray();
    if (!splitter_state.isEmpty())
    {
        ui->splitter->restoreState(splitter_state);
    }
    else
    {
        QList<int> sizes;
        sizes.emplace_back(260);
        sizes.emplace_back(width() - 260);
        ui->splitter->setSizes(sizes);
    }

    ui->sidebar->restoreState(state.value(kSidebarKey).toMap());
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::activate()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::deactivate()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QString ManagementTab::searchText() const
{
    return search_widget_->currentQuery();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::searchTextChanged(const QString& text)
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

        // An item selected later would take the place of the search results.
        ui->sidebar->cancelPendingItem();

        switchContent(search_widget_);
        search_widget_->search(text);
    }

    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::reloadRouter()
{
    ui->sidebar->reloadRouter();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::connectLocalHost(qint64 entry_id, proto::peer::SessionType session_type)
{
    LocalHostConfig entry;
    if (!readLocalHost(entry_id, &entry))
        return;

    HostConfig host = HostConfig::forLocalHost(entry);
    if (!validateHostForConnect(host))
        return;

    setHostConnectTime(entry_id);
    emit sig_connectRequested(host, session_type);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::connectRouterHost(const HostConfig& host, proto::peer::SessionType session_type)
{
    if (validateHostForConnect(host))
        emit sig_connectRequested(host, session_type);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onSwitchContent(SidebarItem::Type type)
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

            switchContent(router_status_widget_);
            router_status_widget_->showRouter(RouterController::instance().events());
        }
        break;

        case SidebarItem::ROUTER_WORKSPACE:
        {
            switchContent(router_group_widget_);

            auto* item = static_cast<SidebarRouterWorkspace*>(ui->sidebar->currentItem());
            router_group_widget_->showGroup(item->workspaceId(), /*group_id=*/0);
        }
        break;

        case SidebarItem::ROUTER_GROUP:
        {
            switchContent(router_group_widget_);

            auto* item = static_cast<SidebarRouterGroup*>(ui->sidebar->currentItem());
            router_group_widget_->showGroup(item->workspaceId(), item->groupId());
        }
        break;

        case SidebarItem::ROUTER_USERS:
        {
            SidebarItem* sidebar_item = ui->sidebar->currentItem();
            if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_USERS)
                break;

            switchContent(router_users_widget_);
            router_users_widget_->showRouter();
        }
        break;

        case SidebarItem::ROUTER_CLIENTS:
        {
            SidebarItem* sidebar_item = ui->sidebar->currentItem();
            if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_CLIENTS)
                break;

            switchContent(router_clients_widget_);
            router_clients_widget_->showRouter();
        }
        break;

        case SidebarItem::ROUTER_RELAYS:
        {
            SidebarItem* sidebar_item = ui->sidebar->currentItem();
            if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_RELAYS)
                break;

            switchContent(router_relays_widget_);
            router_relays_widget_->showRouter();
        }
        break;

        case SidebarItem::ROUTER_ACCESS_LOG:
        {
            SidebarItem* sidebar_item = ui->sidebar->currentItem();
            if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_ACCESS_LOG)
                break;

            switchContent(router_access_log_widget_);
            router_access_log_widget_->showRouter();
        }
        break;

        case SidebarItem::ROUTER_HOSTS:
        {
            SidebarItem* sidebar_item = ui->sidebar->currentItem();
            if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_HOSTS)
                break;

            switchContent(router_hosts_widget_);
            router_hosts_widget_->showRouter();
        }
        break;

        case SidebarItem::ROUTER_TEMP_HOSTS:
        {
            SidebarItem* sidebar_item = ui->sidebar->currentItem();
            if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_TEMP_HOSTS)
                break;

            switchContent(router_temp_hosts_widget_);
            router_temp_hosts_widget_->showRouter();
        }
        break;

        case SidebarItem::ROUTER_QS_HOSTS:
        {
            SidebarItem* sidebar_item = ui->sidebar->currentItem();
            if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_QS_HOSTS)
                break;

            switchContent(router_qs_hosts_widget_);
            router_qs_hosts_widget_->showRouter();
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
void ManagementTab::onSidebarContextMenu(SidebarItem::Type type, const QPoint& pos)
{
    AutoQPointer<QMenu> menu(new QMenu(this));

    if (type == SidebarItem::LOCAL_GROUP)
    {
        menu->addAction(ui->action_add_group);
        menu->addAction(ui->action_edit_group);
        menu->addAction(ui->action_delete_group);
        menu->addSeparator();
        menu->addAction(ui->action_add_host);
    }
    else if (type == SidebarItem::ROUTER_WORKSPACE)
    {
        SidebarItem* item = ui->sidebar->currentItem();
        if (!item || item->itemType() != SidebarItem::ROUTER_WORKSPACE)
            return;

        auto* workspace_item = static_cast<SidebarRouterWorkspace*>(item);

        proto::router::SessionType session_type = proto::router::SESSION_TYPE_OPERATOR;
        if (RouterSession* session = RouterController::session())
            session_type = session->config().sessionType();

        // Operators are read-only; only an administrator manages the workspaces themselves.
        if (session_type != proto::router::SESSION_TYPE_OPERATOR)
            menu->addAction(ui->action_add_group);

        if (session_type == proto::router::SESSION_TYPE_ADMIN)
        {
            menu->addSeparator();
            menu->addAction(ui->action_edit_workspace);
            menu->addAction(ui->action_delete_workspace);
        }
    }
    else if (type == SidebarItem::ROUTER_GROUP)
    {
        SidebarItem* item = ui->sidebar->currentItem();
        if (!item || item->itemType() != SidebarItem::ROUTER_GROUP)
            return;

        auto* group_item = static_cast<SidebarRouterGroup*>(item);

        proto::router::SessionType session_type = proto::router::SESSION_TYPE_OPERATOR;
        if (RouterSession* session = RouterController::session())
            session_type = session->config().sessionType();

        // Clients are read-only and cannot manage host groups.
        if (session_type != proto::router::SESSION_TYPE_OPERATOR)
        {
            menu->addAction(ui->action_add_group);
            menu->addAction(ui->action_edit_group);
            menu->addAction(ui->action_delete_group);
        }
    }
    else if (type == SidebarItem::ROUTER)
    {
        SidebarItem* item = ui->sidebar->currentItem();
        if (!item || item->itemType() != SidebarItem::ROUTER)
            return;

        menu->addAction(ui->action_clear_router_events);

        RouterSession* session = RouterController::session();
        if (session)
        {
            menu->addSeparator();
            if (session->config().sessionType() == proto::router::SESSION_TYPE_ADMIN)
                menu->addAction(ui->action_add_workspace);
            menu->addAction(ui->action_change_router_password);
        }

        menu->exec(pos);
        return;
    }
    else if (type == SidebarItem::ROUTER_USERS)
    {
        menu->addAction(ui->action_add_user);
    }
    else
    {
        return;
    }

    if (menu->isEmpty())
        return;

    menu->exec(pos);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onCurrentHostChanged(qint64 /* entry_id */)
{
    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onConnectAction(QAction* action)
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
        const QList<LocalHostConfig> hosts = local_group_widget_->selectedHosts();
        if (hosts.size() != 1)
            return;

        LocalHostConfig found;
        if (!readLocalHost(hosts.front().id(), &found))
            return;

        host = HostConfig::forLocalHost(found);

        if (!validateHostForConnect(host))
            return;

        setHostConnectTime(found.id());
    }
    else if (current_content_ == search_widget_)
    {
        const SearchResultModel::Row* row = search_widget_->currentRow();
        if (!row)
            return;

        if (row->type == SearchResultModel::Type::ROUTER)
        {
            host = hostForRouterRow(*row);
            if (!validateHostForConnect(host))
                return;
        }
        else
        {
            LocalHostConfig found;
            if (!readLocalHost(row->host.id(), &found))
                return;

            host = HostConfig::forLocalHost(found);

            if (!validateHostForConnect(host))
                return;

            setHostConnectTime(found.id());
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
    else if (RouterTempHostsWidget* temp_hosts_widget = currentTempHostsWidget())
    {
        if (!temp_hosts_widget->hasSelectedHost())
            return;
        host = temp_hosts_widget->selectedHostConfig();
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
void ManagementTab::onLocalConnect(qint64 entry_id)
{
    connectLocalHost(entry_id, defaultSessionType());
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onRouterGroupConnect()
{
    if (!router_group_widget_->hasSelectedHost())
        return;
    HostConfig host = router_group_widget_->selectedHostConfig();
    if (!validateHostForConnect(host))
        return;
    emit sig_connectRequested(host, defaultSessionType());
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onRouterHostConnect()
{
    if (!router_hosts_widget_->hasSelectedHost())
        return;
    HostConfig host = router_hosts_widget_->selectedHostConfig();
    if (!validateHostForConnect(host))
        return;
    emit sig_connectRequested(host, defaultSessionType());
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onTempHostConnect()
{
    RouterTempHostsWidget* temp_hosts_widget = currentTempHostsWidget();
    if (!temp_hosts_widget)
        return;

    const QList<RouterTempHost> hosts = temp_hosts_widget->selectedHosts();
    if (hosts.size() != 1)
        return;

    const proto::peer::SessionType session_type = defaultSessionType();

    if (hosts.front().portable && session_type != proto::peer::SESSION_TYPE_DESKTOP &&
        session_type != proto::peer::SESSION_TYPE_FILE_TRANSFER)
    {
        LOG(INFO) << "Session type" << session_type << "is not available for a portable host";
        MsgBox::warning(this, tr("The \"%1\" session type is not available for Quick Support.")
            .arg(sessionName(session_type)));
        return;
    }

    HostConfig host = temp_hosts_widget->selectedHostConfig();
    if (!validateHostForConnect(host))
        return;
    emit sig_connectRequested(host, session_type);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onSearchConnect()
{
    const SearchResultModel::Row* row = search_widget_->currentRow();
    if (!row)
        return;

    if (row->type == SearchResultModel::Type::ROUTER)
    {
        HostConfig host = hostForRouterRow(*row);
        if (!validateHostForConnect(host))
            return;
        emit sig_connectRequested(host, defaultSessionType());
    }
    else
    {
        onLocalConnect(row->host.id());
    }
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onLocalHostContextMenu(qint64 entry_id, const QPoint& pos)
{
    AutoQPointer<QMenu> menu(new QMenu(this));

    if (entry_id && local_group_widget_->selectedHosts().size() > 1)
    {
        menu->addAction(ui->action_delete_host);
    }
    else if (entry_id)
    {
        auto addProxy = [&menu](QAction* action)
        {
            menu->addAction(action->icon(), action->text(), action, &QAction::triggered);
        };

        addProxy(ui->action_desktop_connect);
        addProxy(ui->action_terminal_connect);
        addProxy(ui->action_file_transfer_connect);
        addProxy(ui->action_chat_connect);
        addProxy(ui->action_system_info_connect);
        menu->addSeparator();
        menu->addAction(ui->action_edit_host);
        menu->addAction(ui->action_copy_host);
        menu->addAction(ui->action_delete_host);
    }
    else
    {
        menu->addAction(ui->action_add_host);
    }

    menu->exec(pos);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onSearchContextMenu(const QPoint& pos)
{
    const SearchResultModel::Row* row = search_widget_->currentRow();
    if (!row)
        return;

    AutoQPointer<QMenu> menu(new QMenu(this));

    auto addProxy = [&menu](QAction* action)
    {
        menu->addAction(action->icon(), action->text(), action, &QAction::triggered);
    };

    addProxy(ui->action_desktop_connect);
    addProxy(ui->action_terminal_connect);
    addProxy(ui->action_file_transfer_connect);
    addProxy(ui->action_chat_connect);
    addProxy(ui->action_system_info_connect);

    // Router hosts have no address-book record to copy or delete.
    if (row->type == SearchResultModel::Type::LOCAL)
    {
        menu->addSeparator();
        menu->addAction(ui->action_edit_host);
        menu->addAction(ui->action_copy_host);
        menu->addAction(ui->action_delete_host);
    }
    else if (ui->action_edit_host->isVisible())
    {
        menu->addSeparator();
        menu->addAction(ui->action_edit_host);
    }

    menu->exec(pos);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onAddHost()
{
    LOG(INFO) << "[ACTION] Add host";

    qint64 group_id = ui->sidebar->currentGroupId();
    if (group_id < 0)
    {
        LOG(INFO) << "No current group";
        return;
    }

    AutoQPointer<LocalHostDialog> dialog(new LocalHostDialog(-1, group_id, this));
    if (dialog->exec() == LocalHostDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    qint64 new_id = dialog->entryId();
    local_group_widget_->showGroup(group_id);
    local_group_widget_->setCurrentHost(new_id);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onEditHost()
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

    if (current_content_ == search_widget_)
    {
        const SearchResultModel::Row* row = search_widget_->currentRow();
        if (row && row->type == SearchResultModel::Type::ROUTER)
        {
            const RouterHost& host = row->router_host;

            AutoQPointer<RouterHostDialog> dialog(new RouterHostDialog(host, this));
            if (dialog->exec() == QDialog::Accepted)
                search_widget_->search(search_widget_->currentQuery());
            return;
        }
    }

    qint64 entry_id = currentHostEntryId();
    if (entry_id <= 0)
    {
        LOG(INFO) << "No current host";
        return;
    }

    LocalHostConfig host;
    const Database::FindResult host_found = LocalHostStore().find(entry_id, &host);
    if (host_found == Database::FindResult::NOT_FOUND || host_found == Database::FindResult::FAILED)
    {
        MsgBox::warning(this, tr("Failed to retrieve host information from the local database."));
        return;
    }

    AutoQPointer<LocalHostDialog> dialog(new LocalHostDialog(entry_id, host.groupId(), this));
    if (dialog->exec() == LocalHostDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    refreshItem(entry_id);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onCopyHost()
{
    LOG(INFO) << "[ACTION] Copy host";

    qint64 entry_id = currentHostEntryId();
    if (entry_id <= 0)
    {
        LOG(INFO) << "No current host";
        return;
    }

    LocalHostConfig host;
    if (!readLocalHost(entry_id, &host))
        return;

    host.setName(host.name() + " " + tr("(copy)"));
    host.setGuid(QString());

    if (!LocalHostStore().add(host))
    {
        MsgBox::warning(this, tr("Failed to add the host to the local database."));
        return;
    }

    qint64 new_id = host.id();

    AutoQPointer<LocalHostDialog> dialog(new LocalHostDialog(new_id, host.groupId(), this));
    dialog->exec();

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
void ManagementTab::onRemoveHost()
{
    LOG(INFO) << "[ACTION] Delete host";

    LocalHostStore store;

    QList<LocalHostConfig> hosts;
    if (current_content_ == local_group_widget_)
    {
        hosts = local_group_widget_->selectedHosts();
    }
    else
    {
        const qint64 entry_id = currentHostEntryId();
        if (entry_id > 0)
        {
            LocalHostConfig host;
            const Database::FindResult host_found = store.find(entry_id, &host);
            if (host_found == Database::FindResult::NOT_FOUND || host_found == Database::FindResult::FAILED)
            {
                MsgBox::warning(this, tr("Failed to retrieve host information from the local database."));
                return;
            }

            hosts.append(host);
        }
    }

    if (hosts.isEmpty())
    {
        LOG(INFO) << "No selected host";
        return;
    }

    const int total = static_cast<int>(hosts.size());

    QString message;
    if (total == 1)
        message = tr("Are you sure you want to delete host \"%1\"?").arg(hosts.front().name());
    else
        message = tr("Are you sure you want to delete %n hosts?", "", total);

    if (MsgBox::question(this, message) == MsgBox::No)
    {
        LOG(INFO) << "Action is rejected by user";
        return;
    }

    int failed = 0;
    for (const LocalHostConfig& host : std::as_const(hosts))
    {
        if (store.remove(host.id()))
        {
            removeItem(host.id());
        }
        else
        {
            LOG(INFO) << "Unable to remove host with id" << host.id();
            ++failed;
        }
    }

    if (failed == 0)
        return;

    if (total == 1)
        MsgBox::warning(this, tr("Unable to remove host"));
    else
        MsgBox::warning(this, tr("Failed to delete %n of the selected hosts.", "", failed));
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onUserContextMenu(const QPoint& pos)
{
    AutoQPointer<QMenu> menu(new QMenu(this));
    if (router_users_widget_->hasSelectedUser())
    {
        menu->addAction(ui->action_edit_user);
        menu->addAction(ui->action_delete_user);
    }
    else
    {
        menu->addAction(ui->action_add_user);
    }
    menu->exec(pos);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onHostContextMenu(const QPoint& pos, int column)
{
    const qsizetype count = router_hosts_widget_->selectedHosts().size();
    if (count == 0)
        return;

    const bool is_single = count == 1;
    const bool has_online = !router_hosts_widget_->selectedOnlineHosts().isEmpty();

    AutoQPointer<QMenu> menu(new QMenu(this));
    auto addProxy = [&menu](QAction* action)
    {
        menu->addAction(action->icon(), action->text(), action, &QAction::triggered);
    };

    if (is_single)
        menu->addAction(ui->action_edit_host);

    if (has_online)
    {
        menu->addAction(ui->action_host_check_updates);
        menu->addAction(ui->action_disconnect);
    }

    if (is_single)
        menu->addAction(ui->action_host_telemetry);
    menu->addAction(ui->action_host_remove);

    if (!is_single)
    {
        menu->exec(pos);
        return;
    }

    if (has_online)
    {
        menu->addSeparator();
        addProxy(ui->action_desktop_connect);
        addProxy(ui->action_terminal_connect);
        addProxy(ui->action_file_transfer_connect);
        addProxy(ui->action_chat_connect);
        addProxy(ui->action_system_info_connect);
    }
    menu->addSeparator();

    const QIcon copy_icon(":/img/copy.svg");
    QAction* copy_row = menu->addAction(copy_icon, tr("Copy Row"));
    QAction* copy_col = menu->addAction(copy_icon, tr("Copy Value"));

    QPointer<ManagementTab> self(this);
    QAction* action = menu->exec(pos);
    if (!action || !self)
        return;

    if (action == copy_row)
        router_hosts_widget_->copyCurrentHostRow();
    else if (action == copy_col)
        router_hosts_widget_->copyCurrentHostColumn(column);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onTempHostContextMenu(const QPoint& pos)
{
    RouterTempHostsWidget* temp_hosts_widget = currentTempHostsWidget();
    if (!temp_hosts_widget)
        return;

    const QList<RouterTempHost> hosts = temp_hosts_widget->selectedHosts();
    if (hosts.isEmpty())
        return;

    const bool can_approve = temp_hosts_widget == router_temp_hosts_widget_;
    if (!can_approve && hosts.size() != 1)
        return;

    AutoQPointer<QMenu> menu(new QMenu(this));
    auto addProxy = [&menu](QAction* action)
    {
        menu->addAction(action->icon(), action->text(), action, &QAction::triggered);
    };

    if (can_approve)
        menu->addAction(ui->action_host_approve);

    if (hosts.size() == 1)
    {
        const bool is_portable = hosts.front().portable;

        if (can_approve)
            menu->addSeparator();
        addProxy(ui->action_desktop_connect);
        if (!is_portable)
            addProxy(ui->action_terminal_connect);
        addProxy(ui->action_file_transfer_connect);
        if (!is_portable)
        {
            addProxy(ui->action_chat_connect);
            addProxy(ui->action_system_info_connect);
        }
    }

    menu->exec(pos);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onClientContextMenu(const QPoint& pos, int column)
{
    const int count = router_clients_widget_->selectedClientCount();
    if (count == 0)
        return;

    AutoQPointer<QMenu> menu(new QMenu(this));
    menu->addAction(ui->action_disconnect);

    if (count != 1)
    {
        menu->exec(pos);
        return;
    }

    menu->addSeparator();

    const QIcon copy_icon(":/img/copy.svg");
    QAction* copy_row = menu->addAction(copy_icon, tr("Copy Row"));
    QAction* copy_col = menu->addAction(copy_icon, tr("Copy Value"));

    QPointer<ManagementTab> self(this);
    QAction* action = menu->exec(pos);
    if (!action || !self)
        return;

    if (action == copy_row)
        router_clients_widget_->copyCurrentClientRow();
    else if (action == copy_col)
        router_clients_widget_->copyCurrentClientColumn(column);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onRelayContextMenu(const QPoint& pos, int column)
{
    const int count = router_relays_widget_->selectedRelayCount();
    if (count == 0)
        return;

    AutoQPointer<QMenu> menu(new QMenu(this));
    menu->addAction(ui->action_disconnect);

    if (count != 1)
    {
        menu->exec(pos);
        return;
    }

    menu->addSeparator();

    const QIcon copy_icon(":/img/copy.svg");
    QAction* copy_row = menu->addAction(copy_icon, tr("Copy Row"));
    QAction* copy_col = menu->addAction(copy_icon, tr("Copy Value"));

    QPointer<ManagementTab> self(this);
    QAction* action = menu->exec(pos);
    if (!action || !self)
        return;

    if (action == copy_row)
        router_relays_widget_->copyCurrentRelayRow();
    else if (action == copy_col)
        router_relays_widget_->copyCurrentRelayColumn(column);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onAccessLogContextMenu(const QPoint& pos, int column)
{
    if (!router_access_log_widget_->hasCurrentEntry())
        return;

    AutoQPointer<QMenu> menu(new QMenu(this));

    const QIcon copy_icon(":/img/copy.svg");
    QAction* copy_row = menu->addAction(copy_icon, tr("Copy Row"));
    QAction* copy_col = menu->addAction(copy_icon, tr("Copy Value"));

    QPointer<ManagementTab> self(this);
    QAction* action = menu->exec(pos);
    if (!action || !self)
        return;

    if (action == copy_row)
        router_access_log_widget_->copyCurrentRow();
    else if (action == copy_col)
        router_access_log_widget_->copyCurrentColumn(column);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onRouterGroupContextMenu(const QPoint& pos)
{
    if (!router_group_widget_->hasSelectedHost())
        return;

    // Proxy the global action via a fresh menu entry: the local copy is always visible (so the
    // item shows up here) while the original ui->action_*_connect can stay hidden globally.
    AutoQPointer<QMenu> menu(new QMenu(this));
    auto addProxy = [&menu](QAction* action)
    {
        menu->addAction(action->icon(), action->text(), action, &QAction::triggered);
    };

    addProxy(ui->action_desktop_connect);
    addProxy(ui->action_terminal_connect);
    addProxy(ui->action_file_transfer_connect);
    addProxy(ui->action_chat_connect);
    addProxy(ui->action_system_info_connect);

    if (ui->action_edit_host->isVisible())
    {
        menu->addSeparator();
        menu->addAction(ui->action_edit_host);
    }

    menu->exec(pos);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onAddUserAction()
{
    if (current_content_ == router_users_widget_)
        router_users_widget_->onAddUser();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onEditUserAction()
{
    if (current_content_ == router_users_widget_)
        router_users_widget_->onModifyUser();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onDeleteUserAction()
{
    if (current_content_ == router_users_widget_)
        router_users_widget_->onDeleteUser();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onAddWorkspaceAction()
{
    if (!ui->sidebar->isRouterItemCurrent() || !RouterController::session())
        return;

    AutoQPointer<RouterWorkspaceDialog> dialog(new RouterWorkspaceDialog(0, this));
    if (dialog->exec() == QDialog::Accepted)
        ui->sidebar->onRefreshWorkspaces();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onEditWorkspaceAction()
{
    SidebarItem* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_WORKSPACE)
        return;

    auto* workspace_item = static_cast<SidebarRouterWorkspace*>(sidebar_item);

    AutoQPointer<RouterWorkspaceDialog> dialog(
        new RouterWorkspaceDialog(workspace_item->workspaceId(), this));
    if (dialog->exec() == QDialog::Accepted)
        ui->sidebar->onRefreshWorkspaces();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onDeleteWorkspaceAction()
{
    SidebarItem* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER_WORKSPACE)
        return;

    auto* workspace_item = static_cast<SidebarRouterWorkspace*>(sidebar_item);
    const qint64 workspace_id = workspace_item->workspaceId();

    if (MsgBox::question(this, tr("Are you sure you want to delete space \"%1\"?")
            .arg(workspace_item->workspaceName())) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Delete workspace rejected by user";
        return;
    }

    RouterSession* session = RouterController::session();
    if (!session)
        return;

    LOG(INFO) << "[ACTION] Delete workspace accepted by user";
    session->deleteWorkspace(workspace_id, { this,
        [this](const proto::router::WorkspaceResult& result)
    {
        if (result.error_code() != proto::router::kErrorOk)
        {
            // The operator explicitly confirmed the delete - a silent no-op would read as a UI
            // bug, so the failure is reported like every other mutation.
            LOG(ERROR) << "Workspace delete failed:" << result.error_code();
            MsgBox::warning(this, tr("Failed to delete the space."));
        }
        ui->sidebar->onRefreshWorkspaces();
    } });
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onAddGroupAction()
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
    qint64 workspace_id = 0;
    qint64 default_parent_id = 0;
    QString workspace_name;

    if (item->itemType() == SidebarItem::ROUTER_WORKSPACE)
    {
        auto* workspace_item = static_cast<SidebarRouterWorkspace*>(item);
        workspace_id = workspace_item->workspaceId();
        workspace_name = workspace_item->workspaceName();
        default_parent_id = 0;
    }
    else if (item->itemType() == SidebarItem::ROUTER_GROUP)
    {
        auto* group_item = static_cast<SidebarRouterGroup*>(item);
        workspace_id = group_item->workspaceId();
        workspace_name = group_item->workspaceName();
        default_parent_id = group_item->groupId();
    }
    else
    {
        return;
    }

    AutoQPointer<RouterGroupDialog> dialog(
        new RouterGroupDialog(workspace_id, workspace_name, 0, default_parent_id, this));
    if (dialog->exec() == QDialog::Accepted)
        ui->sidebar->onRefreshHostGroups();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onEditGroupAction()
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

    AutoQPointer<RouterGroupDialog> dialog(new RouterGroupDialog(group_item->workspaceId(),
        group_item->workspaceName(), group_item->groupId(), 0, this));
    if (dialog->exec() == QDialog::Accepted)
        ui->sidebar->onRefreshHostGroups();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onDeleteGroupAction()
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

    // A refresh or a disconnect of the router while the question is shown deletes the item, so the
    // ids are taken before asking.
    const qint64 workspace_id = group_item->workspaceId();
    const qint64 group_id = group_item->groupId();

    const QString question = tr("Are you sure you want to delete the group \"%1\"? "
                                "Hosts assigned to this group or its subgroups will be moved "
                                "to the space root.").arg(group_item->text(0));
    if (MsgBox::question(this, question) == MsgBox::No)
        return;

    RouterSession* session = RouterController::session();
    if (!session)
        return;

    session->deleteGroup(workspace_id, group_id, { this,
        [this](const proto::router::GroupResult& result)
    {
        if (result.error_code() != proto::router::kErrorOk)
        {
            // Same as the workspace delete: the operator explicitly confirmed the action, so a
            // silent no-op would read as a UI bug.
            LOG(ERROR) << "Group delete failed:" << result.error_code();
            MsgBox::warning(this, tr("Failed to delete the group."));
            return;
        }
        ui->sidebar->onRefreshHostGroups();
    } });
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onChangeRouterPassword()
{
    SidebarItem* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER)
        return;

    SidebarRouter* router = static_cast<SidebarRouter*>(sidebar_item);
    if (RouterController::session())
        ui->sidebar->changeRouterPassword();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onClearRouterEvents()
{
    SidebarItem* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != SidebarItem::ROUTER)
        return;

    RouterController& controller = RouterController::instance();
    controller.clearEvents();
    router_status_widget_->showRouter(controller.events());
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onExportBookAction()
{
    LOG(INFO) << "[ACTION] Create backup";

    if (!Database::instance().isValid())
    {
        MsgBox::warning(this, tr("The database is not available."));
        return;
    }

    const QString file_path = FileDialog::getSaveFileName(
        this,
        tr("Create Backup"),
        tr("Aspia Backup (*.aspia-backup);;All files (*)"));

    if (file_path.isEmpty())
    {
        LOG(INFO) << "[ACTION] Cancelled by user";
        return;
    }

    Backup::Report report;

    switch (Backup::exportToFile(Database::instance(), file_path, &report))
    {
        case Backup::Result::SUCCESS:
            break;

        case Backup::Result::FILE_ERROR:
            MsgBox::warning(this, tr("Unable to write the file."));
            return;

        case Backup::Result::UNREADABLE_RECORD:
            MsgBox::warning(this, tr("Some records of the database are damaged. Fix or delete them and try again."));
            return;

        default:
            MsgBox::warning(this, tr("Failed to create the backup."));
            return;
    }

    QStringList lines = { tr("Export completed successfully.") };

    if (report.router)
        lines.append(tr("Router exported"));

    lines << tr("Router hosts exported: %1").arg(report.router_hosts)
          << tr("Local groups exported: %1").arg(report.local_groups)
          << tr("Local hosts exported: %1").arg(report.local_hosts)
          << tr("Credentials exported: %1").arg(report.credentials)
          << tr("Profiles exported: %1").arg(report.profiles);

    MsgBox::information(this, lines.join('\n'));
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onImportBookAction()
{
    LOG(INFO) << "[ACTION] Restore from backup";

    if (!Database::instance().isValid())
    {
        MsgBox::warning(this, tr("The database is not available."));
        return;
    }

    const QString file_path = FileDialog::getOpenFileName(
        this,
        tr("Restore from Backup"),
        tr("Aspia Backup (*.aspia-backup);;All files (*)"));

    if (file_path.isEmpty())
    {
        LOG(INFO) << "[ACTION] Cancelled by user";
        return;
    }

    RouterConfig router;
    const bool sync_enabled =
        RouterStore().find(&router) == Database::FindResult::FOUND && router.isSyncEnabled();

    const QString question = sync_enabled ?
        tr("Everything stored now is deleted and replaced with what the backup holds. The synchronized data "
           "on the router and on the other devices of the account saved in the backup will be replaced as "
           "well. Continue?") :
        tr("Everything stored now is deleted and replaced with what the backup holds. Continue?");

    if (MsgBox::question(this, question) == MsgBox::No)
    {
        LOG(INFO) << "[ACTION] Cancelled by user";
        return;
    }

    // A backup made here opens with the key the data is already open with. One made elsewhere
    // takes the master password of that installation.
    SecureString password;
    Backup::Report report;
    Backup::Result result =
        Backup::importFromFile(Database::instance(), file_path, password, &report);

    if (result == Backup::Result::WRONG_PASSWORD)
    {
        AutoQPointer<CredentialsDialog> dialog(new CredentialsDialog(CredentialsDialog::Type::ENTER_PASSWORD, this));
        dialog->setWindowTitle(tr("Restore from Backup"));
        dialog->setHeaderIcon(":/img/lock.svg");
        dialog->setHeaderText(tr("The backup was made on another installation. Enter the master "
                                "password used there."));
        dialog->setShowPasswordButtonVisible(true);

        if (dialog->exec() != QDialog::Accepted)
            return;

        password = dialog->password();
        result = Backup::importFromFile(Database::instance(), file_path, password, &report);
    }

    switch (result)
    {
        case Backup::Result::SUCCESS:
            break;

        case Backup::Result::WRONG_PASSWORD:
            MsgBox::warning(this, tr("Unable to decrypt the file with the specified password."));
            return;

        case Backup::Result::UNSUPPORTED_VERSION:
            MsgBox::warning(this, tr("Unsupported file format version."));
            return;

        case Backup::Result::NOTHING_IMPORTED:
            MsgBox::information(this, tr("The backup carries no data, so nothing was changed."));
            return;

        case Backup::Result::FILE_ERROR:
            MsgBox::warning(this, tr("Unable to read the file."));
            return;

        case Backup::Result::INVALID_FORMAT:
            MsgBox::warning(this, tr("The file is not a valid backup."));
            return;

        default:
            MsgBox::warning(this, tr("Failed to restore from the backup."));
            return;
    }

    QStringList lines = { tr("Import completed successfully.") };

    if (report.router)
        lines.append(tr("Router imported"));

    lines << tr("Router hosts imported: %1").arg(report.router_hosts)
          << tr("Local groups imported: %1").arg(report.local_groups)
          << tr("Local hosts imported: %1").arg(report.local_hosts)
          << tr("Credentials imported: %1").arg(report.credentials)
          << tr("Profiles imported: %1").arg(report.profiles);

    // The router is reloaded before the message, whose event loop lets a session of the replaced
    // router write its old record over the imported one.
    reloadRouter();

    MsgBox::information(this, lines.join('\n'));

    ui->sidebar->reloadGroups();
    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onReloadAction()
{
    if (current_content_ && current_content_->canReload())
        current_content_->reload();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onSaveAction()
{
    if (current_content_ && current_content_->canSave())
        current_content_->save();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onDisconnectAction()
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
void ManagementTab::onRemoveHostAction()
{
    if (current_content_ == router_hosts_widget_)
        router_hosts_widget_->onRemoveHost();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onApproveHostAction()
{
    if (current_content_ == router_temp_hosts_widget_)
        router_temp_hosts_widget_->onApproveHost();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onCheckHostUpdatesAction()
{
    if (current_content_ == router_hosts_widget_)
        router_hosts_widget_->onCheckHostUpdates();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onHostTelemetryAction()
{
    if (current_content_ == router_hosts_widget_)
        router_hosts_widget_->onHostTelemetry();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onOnlineCheckToggled(bool checked)
{
    UserSettings().setOnlineCheckEnabled(checked);
    local_group_widget_->setOnlineCheckEnabled(checked);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::onTwoFactorRequired()
{
    // Being asked again, by a fresh question or by the button of the router, brings back a question
    // the user dismissed.
    two_factor_dismissed_ = false;

    if (two_factor_dialog_)
        return;

    // The question of the router. Gone or blocked means nothing to put on screen.
    QPointer<TwoFactorPrompt> prompt(RouterController::twoFactorPrompt());
    if (!prompt || prompt->blockedSeconds() > 0)
        return;

    // An account with no secret yet scans what the router handed out before it can answer, so the
    // two are asked in different dialogs.
    const QString otpauth_uri = prompt->otpauthUri();
    const bool code_refused = prompt->codeRefused();

    QDialog* dialog = nullptr;
    if (otpauth_uri.isEmpty())
    {
        TwoFactorCodeDialog* code_dialog = new TwoFactorCodeDialog(code_refused, this);
        connect(code_dialog, &QDialog::finished, this, [this, prompt, code_dialog](int result)
        {
            two_factor_dialog_.clear();

            if (result == QDialog::Accepted && prompt)
                prompt->submitCode(code_dialog->code());
            else if (prompt)
                two_factor_dismissed_ = true;

            // A question that replaced the answered one takes the screen. A dismissed question
            // steps aside until it is asked again.
            showNextTwoFactorPrompt();
        });
        dialog = code_dialog;
    }
    else
    {
        TwoFactorEnrollDialog* enroll_dialog =
            new TwoFactorEnrollDialog(otpauth_uri, code_refused, this);
        connect(enroll_dialog, &QDialog::finished, this,
                [this, prompt, enroll_dialog](int result)
        {
            two_factor_dialog_.clear();

            if (result == QDialog::Accepted && prompt)
                prompt->submitCode(enroll_dialog->code());
            else if (prompt)
                two_factor_dismissed_ = true;

            showNextTwoFactorPrompt();
        });
        dialog = enroll_dialog;
    }

    // The question can leave the screen before the operator does: its death (the router left,
    // the challenge changed) closes the dialog.
    connect(prompt, &QObject::destroyed, dialog, &QWidget::close);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    two_factor_dialog_ = dialog;

    // open() and not exec(): the prompt is modal, so it cannot be missed, but it does not spin a
    // nested event loop inside the delivery of the challenge that opened it.
    dialog->open();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::showNextTwoFactorPrompt()
{
    if (two_factor_dismissed_)
        return;

    TwoFactorPrompt* prompt = RouterController::twoFactorPrompt();
    if (prompt && prompt->blockedSeconds() == 0)
        onTwoFactorRequired();
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::switchContent(ContentWidget* new_widget)
{
    if (!new_widget || new_widget == current_content_)
        return;

    current_content_ = new_widget;
    ui->content_stack->setCurrentWidget(new_widget);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::updateActionsState()
{
    ui->action_add_group->setVisible(false);
    ui->action_delete_group->setVisible(false);
    ui->action_edit_group->setVisible(false);

    ui->action_change_router_password->setVisible(false);
    ui->action_clear_router_events->setVisible(false);

    ui->action_add_host->setVisible(false);
    ui->action_delete_host->setVisible(false);
    ui->action_delete_host->setText(tr("Delete Host"));
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

    ui->action_edit_workspace->setVisible(false);
    ui->action_delete_workspace->setVisible(false);

    ui->action_reload->setVisible(false);
    ui->action_save->setVisible(false);
    ui->action_disconnect->setVisible(false);
    ui->action_host_remove->setVisible(false);
    ui->action_host_approve->setVisible(false);
    ui->action_host_check_updates->setVisible(false);
    ui->action_host_telemetry->setVisible(false);
    ui->action_online_check->setVisible(false);
    ui->action_desktop_connect->setVisible(false);
    ui->action_file_transfer_connect->setVisible(false);
    ui->action_chat_connect->setVisible(false);
    ui->action_system_info_connect->setVisible(false);
    ui->action_terminal_connect->setVisible(false);

    SidebarItem* sidebar_item = ui->sidebar->currentItem();

    if (current_content_ == search_widget_)
    {
        const SearchResultModel::Row* row = search_widget_->currentRow();
        const bool is_local_host = row != nullptr && row->type == SearchResultModel::Type::LOCAL;
        const bool is_router_host = row != nullptr && row->type == SearchResultModel::Type::ROUTER &&
            RouterController::session() != nullptr;

        // Router hosts have no address-book record to copy or delete.
        ui->action_delete_host->setVisible(is_local_host);
        ui->action_edit_host->setVisible(is_local_host || is_router_host);
        ui->action_copy_host->setVisible(is_local_host);
    }
    else if (sidebar_item && sidebar_item->itemType() == SidebarItem::LOCAL_GROUP)
    {
        ui->action_online_check->setVisible(true);

        ui->action_add_group->setVisible(true);
        ui->action_delete_group->setVisible(sidebar_item->groupId() != 0);
        ui->action_edit_group->setVisible(sidebar_item->groupId() != 0);

        const qsizetype count = local_group_widget_->selectedHosts().size();
        const bool has_host = count == 1;

        ui->action_add_host->setVisible(true);
        ui->action_delete_host->setVisible(count > 0);
        if (count > 1)
            ui->action_delete_host->setText(tr("Delete Hosts"));
        ui->action_edit_host->setVisible(has_host);
        ui->action_copy_host->setVisible(has_host);
    }
    else if (sidebar_item && sidebar_item->itemType() == SidebarItem::ROUTER_WORKSPACE)
    {
        auto* workspace_item = static_cast<SidebarRouterWorkspace*>(sidebar_item);

        proto::router::SessionType session_type = proto::router::SESSION_TYPE_OPERATOR;
        if (RouterSession* session = RouterController::session())
            session_type = session->config().sessionType();

        // Operators are read-only; only an administrator manages the workspaces themselves.
        const bool is_admin = session_type == proto::router::SESSION_TYPE_ADMIN;
        ui->action_add_group->setVisible(session_type != proto::router::SESSION_TYPE_OPERATOR);
        ui->action_edit_workspace->setVisible(is_admin);
        ui->action_delete_workspace->setVisible(is_admin);
    }
    else if (sidebar_item && sidebar_item->itemType() == SidebarItem::ROUTER_GROUP)
    {
        auto* group_item = static_cast<SidebarRouterGroup*>(sidebar_item);

        proto::router::SessionType session_type = proto::router::SESSION_TYPE_OPERATOR;
        if (RouterSession* session = RouterController::session())
            session_type = session->config().sessionType();

        // Clients are read-only and cannot manage host groups.
        const bool can_manage_groups = session_type != proto::router::SESSION_TYPE_OPERATOR;
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
        ui->action_disconnect->setVisible(router_clients_widget_->selectedClientCount() > 0);
    }
    else if (sidebar_item && sidebar_item->itemType() == SidebarItem::ROUTER_RELAYS)
    {
        ui->action_disconnect->setVisible(router_relays_widget_->selectedRelayCount() > 0);
    }
    else if (sidebar_item && sidebar_item->itemType() == SidebarItem::ROUTER_HOSTS)
    {
        const bool has_host = router_hosts_widget_->hasSelectedHost();
        const bool has_hosts = !router_hosts_widget_->selectedHosts().isEmpty();
        const bool has_online = !router_hosts_widget_->selectedOnlineHosts().isEmpty();

        ui->action_disconnect->setVisible(has_online);
        ui->action_host_remove->setVisible(has_hosts);
        ui->action_edit_host->setVisible(has_host);

        ui->action_host_check_updates->setVisible(has_online);
        ui->action_host_telemetry->setVisible(has_host);
    }
    else if (sidebar_item && sidebar_item->itemType() == SidebarItem::ROUTER_TEMP_HOSTS)
    {
        ui->action_host_approve->setVisible(!router_temp_hosts_widget_->selectedHosts().isEmpty());
    }

    // The address book is saved and restored as a whole, the router included, so its actions go with
    // every item of the sidebar.
    const bool book_actions = sidebar_item && current_content_ != search_widget_;
    ui->action_export_book->setVisible(book_actions);
    ui->action_import_book->setVisible(book_actions);

    if (sidebar_item && sidebar_item->itemType() == SidebarItem::ROUTER)
    {
        ui->action_clear_router_events->setVisible(true);

        ui->action_change_router_password->setVisible(RouterController::session() != nullptr);
    }

    // Adding a workspace targets the whole router, so the action goes with every item of a router
    // when connected as an administrator.
    RouterSession* router_session = ui->sidebar->isRouterItemCurrent() ? RouterController::session() : nullptr;
    ui->action_add_workspace->setVisible(router_session &&
        router_session->config().sessionType() == proto::router::SESSION_TYPE_ADMIN);

    bool show_session_types = (current_content_ == search_widget_);
    if (sidebar_item)
    {
        show_session_types = show_session_types ||
            sidebar_item->itemType() == SidebarItem::LOCAL_GROUP ||
            sidebar_item->itemType() == SidebarItem::ROUTER_GROUP ||
            sidebar_item->itemType() == SidebarItem::ROUTER_WORKSPACE ||
            sidebar_item->itemType() == SidebarItem::ROUTER_HOSTS ||
            sidebar_item->itemType() == SidebarItem::ROUTER_TEMP_HOSTS ||
            sidebar_item->itemType() == SidebarItem::ROUTER_QS_HOSTS;
    }

    ui->action_desktop->setVisible(show_session_types);
    ui->action_terminal->setVisible(show_session_types);
    ui->action_file_transfer->setVisible(show_session_types);
    ui->action_chat->setVisible(show_session_types);
    ui->action_system_info->setVisible(show_session_types);

    if (current_content_ != search_widget_ && sidebar_item &&
        (sidebar_item->itemType() == SidebarItem::ROUTER_GROUP ||
         sidebar_item->itemType() == SidebarItem::ROUTER_WORKSPACE))
    {
        RouterSession* session = RouterController::session();
        ui->action_edit_host->setVisible(session && router_group_widget_->hasSelectedHost());
    }

    ui->action_reload->setVisible(current_content_ && current_content_->canReload());
    ui->action_save->setVisible(current_content_ && current_content_->canSave());
}

//--------------------------------------------------------------------------------------------------
RouterTempHostsWidget* ManagementTab::currentTempHostsWidget() const
{
    if (current_content_ == router_temp_hosts_widget_)
        return router_temp_hosts_widget_;
    else if (current_content_ == router_qs_hosts_widget_)
        return router_qs_hosts_widget_;

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
proto::peer::SessionType ManagementTab::defaultSessionType() const
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
bool ManagementTab::readLocalHost(qint64 entry_id, LocalHostConfig* host)
{
    const Database::FindResult result = LocalHostStore().find(entry_id, host);
    if (result == Database::FindResult::FOUND)
        return true;

    if (result == Database::FindResult::UNREADABLE)
    {
        LOG(ERROR) << "Data of host" << entry_id << "could not be read";
        MsgBox::warning(this, tr("The data of the host is damaged. Edit the host and enter it again."));
    }
    else
    {
        LOG(ERROR) << "Unable to find host with id" << entry_id;
        MsgBox::warning(this, tr("Failed to retrieve host information from the local database."));
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool ManagementTab::validateHostForConnect(const HostConfig& host)
{
    if (isHostId(host.address()))
    {
        RouterConfig router;
        const Database::FindResult found = RouterStore().find(&router);
        if (found == Database::FindResult::UNREADABLE)
        {
            MsgBox::warning(this, tr("The data of the router is damaged. Edit the router and enter it again."));
            return false;
        }

        if (found == Database::FindResult::NOT_FOUND)
        {
            MsgBox::warning(this, tr("The host is reached through a router, but no router is set. "
                                     "Add a router or enter the address of the host instead of its ID."));
            return false;
        }

        if (found != Database::FindResult::FOUND)
        {
            MsgBox::warning(this, tr("Failed to read the router."));
            return false;
        }

        if (!router.isEnabled())
        {
            MsgBox::warning(this, tr("The router is turned off. Turn it on in the router settings to "
                                     "connect to this host."));
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
        Address address = Address::fromString(host.address(), kDefaultHostTcpPort);
        if (!address.isValid())
        {
            MsgBox::warning(this, tr("The host has an incorrect address."));
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::setHostConnectTime(qint64 entry_id)
{
    const qint64 connect_time = QDateTime::currentSecsSinceEpoch();
    LocalHostStore().setConnectTime(entry_id, connect_time);
    local_group_widget_->setConnectTime(entry_id, connect_time);
}

//--------------------------------------------------------------------------------------------------
qint64 ManagementTab::currentHostEntryId() const
{
    if (current_content_ == local_group_widget_)
    {
        const QList<LocalHostConfig> hosts = local_group_widget_->selectedHosts();
        return hosts.size() == 1 ? hosts.front().id() : -1;
    }

    if (current_content_ == search_widget_)
    {
        const SearchResultModel::Row* row = search_widget_->currentRow();
        return row ? row->host.id() : -1;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::refreshItem(qint64 entry_id)
{
    if (current_content_ == local_group_widget_)
        local_group_widget_->refreshItem(entry_id);
    else if (current_content_ == search_widget_)
        search_widget_->refreshItem(entry_id);
}

//--------------------------------------------------------------------------------------------------
void ManagementTab::removeItem(qint64 entry_id)
{
    if (current_content_ == local_group_widget_)
        local_group_widget_->removeItem(entry_id);
    else if (current_content_ == search_widget_)
        search_widget_->removeItem(entry_id);
}
