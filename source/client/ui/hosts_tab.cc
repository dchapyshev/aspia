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

#include "client/ui/hosts_tab.h"

#include <optional>

#include <QActionGroup>
#include <QDateTime>
#include <QEvent>
#include <QFileDialog>
#include <QMenu>
#include <QStatusBar>

#include "common/ui/msg_box.h"
#include "base/logging.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "base/peer/user.h"
#include "build/build_config.h"
#include "client/aab_importer.h"
#include "client/database.h"
#include "client/json_backup.h"
#include "client/settings.h"
#include "client/ui/hosts/content_widget.h"
#include "client/ui/hosts/local_host_dialog.h"
#include "client/ui/hosts/local_group_widget.h"
#include "client/ui/hosts/router_group_dialog.h"
#include "client/ui/hosts/router_widget.h"
#include "client/ui/hosts/router_group_widget.h"
#include "client/ui/hosts/search_widget.h"
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
    session_type_group->addAction(ui->action_file_transfer);
    session_type_group->addAction(ui->action_chat);
    session_type_group->addAction(ui->action_system_info);

    QActionGroup* session_connect_group = new QActionGroup(this);
    session_connect_group->addAction(ui->action_desktop_connect);
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
    search_widget_ = new SearchWidget(this);

    ui->content_stack->addWidget(local_group_widget_);
    ui->content_stack->addWidget(router_group_widget_);
    ui->content_stack->addWidget(search_widget_);

    // Setup drag-and-drop: pass the host mime types from the source widgets to Sidebar.
    ui->sidebar->setLocalHostMimeType(local_group_widget_->mimeType());
    ui->sidebar->setRouterHostMimeType(router_group_widget_->mimeType());

    // Connect signals.
    connect(ui->sidebar, &Sidebar::sig_switchContent, this, &HostsTab::onSwitchContent);
    connect(ui->sidebar, &Sidebar::sig_contextMenu, this, &HostsTab::onSidebarContextMenu);
    connect(ui->sidebar, &Sidebar::sig_itemDropped, this, [this]()
    {
        local_group_widget_->showGroup(ui->sidebar->currentGroupId());
        updateActionsState();
    });
    connect(local_group_widget_, &LocalGroupWidget::sig_currentChanged, this, &HostsTab::onCurrentHostChanged);
    connect(local_group_widget_, &LocalGroupWidget::sig_doubleClicked, this, &HostsTab::onLocalConnect);
    connect(local_group_widget_, &LocalGroupWidget::sig_contextMenu, this, &HostsTab::onLocalHostContextMenu);
    connect(router_group_widget_, &RouterGroupWidget::sig_currentChanged, this, &HostsTab::updateActionsState);
    connect(router_group_widget_, &RouterGroupWidget::sig_contextMenu, this, &HostsTab::onRouterGroupContextMenu);
    connect(router_group_widget_, &RouterGroupWidget::sig_doubleClicked, this, &HostsTab::onRouterGroupConnect);
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
    connect(search_widget_, &SearchWidget::sig_doubleClicked, this, &HostsTab::onSearchConnect);
    connect(search_widget_, &SearchWidget::sig_contextMenu, this, &HostsTab::onSearchContextMenu);
    connect(ui->action_add_group, &QAction::triggered, this, &HostsTab::onAddGroupAction);
    connect(ui->action_edit_group, &QAction::triggered, this, &HostsTab::onEditGroupAction);
    connect(ui->action_delete_group, &QAction::triggered, this, &HostsTab::onDeleteGroupAction);
    connect(ui->action_add_router, &QAction::triggered, ui->sidebar, &Sidebar::onAddRouter);
    connect(ui->action_edit_router, &QAction::triggered, ui->sidebar, &Sidebar::onEditRouter);
    connect(ui->action_delete_router, &QAction::triggered, ui->sidebar, &Sidebar::onRemoveRouter);
    connect(ui->action_router_status, &QAction::triggered, this, &HostsTab::onRouterStatus);
    connect(ui->sidebar, &Sidebar::sig_routersChanged, this, &HostsTab::reloadRouters);
    connect(ui->sidebar, &Sidebar::sig_routerHostMoved, this, [this](qint64 /* router_id */)
    {
        if (current_content_ == router_group_widget_)
            router_group_widget_->reload();
    });
    connect(ui->sidebar, &Sidebar::sig_routerGroupMoved, this, &HostsTab::refreshSidebarHostGroups);
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
    connect(ui->action_online_check, &QAction::toggled, this, &HostsTab::onOnlineCheckToggled);
    connect(session_connect_group, &QActionGroup::triggered, this, &HostsTab::onConnectAction);

    // Register actions for toolbar and menus.
    addActions(ActionRole::FILE, { ui->action_save, ui->action_import_old_book, ui->action_export_book, ui->action_import_book });
    addActions(ActionRole::EDIT, { ui->action_add_user, ui->action_edit_user, ui->action_delete_user });
    addActions(ActionRole::EDIT, { ui->action_add_workspace, ui->action_edit_workspace, ui->action_delete_workspace });
    addActions(ActionRole::EDIT, { ui->action_add_router, ui->action_edit_router, ui->action_delete_router, ui->action_router_status });
    addActions(ActionRole::EDIT, { ui->action_add_group, ui->action_edit_group, ui->action_delete_group });
    addActions(ActionRole::EDIT, { ui->action_add_host, ui->action_edit_host, ui->action_copy_host, ui->action_delete_host });
    addActions(ActionRole::EDIT, { ui->action_host_remove, ui->action_disconnect, ui->action_disconnect_all });
    addActions(ActionRole::ACTION, { ui->action_desktop_connect, ui->action_file_transfer_connect,
                                     ui->action_chat_connect, ui->action_system_info_connect });
    addActions(ActionRole::VIEW, { ui->action_reload, ui->action_online_check });
    addActions(ActionRole::SESSION_TYPE, { ui->action_desktop, ui->action_file_transfer, ui->action_chat, ui->action_system_info });

    local_group_widget_->setOnlineCheckEnabled(ui->action_online_check->isChecked());
    local_group_widget_->showGroup(ui->sidebar->currentGroupId());
    switchContent(local_group_widget_);
    updateActionsState();

    const QList<RouterConfig> routers = Database::instance().routerList();
    for (const RouterConfig& router : std::as_const(routers))
    {
        if (router.isValid())
            createRouterWidget(router);
    }
}

//--------------------------------------------------------------------------------------------------
HostsTab::~HostsTab()
{
    LOG(INFO) << "Dtor";
    destroyAllRouterWidgets();
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
        stream << search_widget_->saveState();
        stream << ui->splitter->saveState();

        QByteArray routers_buffer;
        {
            QDataStream routers_stream(&routers_buffer, QIODevice::WriteOnly);
            routers_stream.setVersion(QDataStream::Qt_6_10);

            routers_stream << quint32(router_widgets_.size());
            for (auto it = router_widgets_.begin(); it != router_widgets_.end(); ++it)
            {
                routers_stream << it.key();
                routers_stream << it.value()->saveState();
            }
        }
        stream << routers_buffer;
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
    QByteArray search_state;
    QByteArray splitter_state;
    QByteArray routers_buffer;

    stream >> local_group_state;
    stream >> router_group_state;
    stream >> search_state;
    stream >> splitter_state;
    stream >> routers_buffer;

    if (!local_group_state.isEmpty())
        local_group_widget_->restoreState(local_group_state);

    if (!router_group_state.isEmpty())
        router_group_widget_->restoreState(router_group_state);

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

    if (!routers_buffer.isEmpty())
    {
        QDataStream routers_stream(routers_buffer);
        routers_stream.setVersion(QDataStream::Qt_6_10);

        quint32 count = 0;
        routers_stream >> count;

        for (quint32 i = 0; i < count; ++i)
        {
            qint64 router_id = -1;
            QByteArray widget_state;

            routers_stream >> router_id;
            routers_stream >> widget_state;

            if (routers_stream.status() != QDataStream::Ok)
                break;

            if (widget_state.isEmpty())
                continue;

            auto it = router_widgets_.find(router_id);
            if (it != router_widgets_.end())
                it.value()->restoreState(widget_state);
        }
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
    const QList<qint64> old_ids = router_widgets_.keys();
    const QList<RouterConfig> routers = Database::instance().routerList();

    // Remove widgets for routers that no longer exist.
    for (qint64 id : std::as_const(old_ids))
    {
        bool found = false;
        for (const RouterConfig& router : std::as_const(routers))
        {
            if (router.routerId() == id)
            {
                found = true;
                break;
            }
        }

        if (!found)
            destroyRouterWidget(id);
    }

    ui->sidebar->reloadRouters();

    // Create new or update existing widgets.
    for (const RouterConfig& router : std::as_const(routers))
    {
        if (!router.isValid())
            continue;

        auto it = router_widgets_.find(router.routerId());
        if (it != router_widgets_.end())
            it.value()->updateConfig(router);
        else
            createRouterWidget(router);
    }
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
void HostsTab::onRouterStatusChanged(qint64 router_id, Router::Status status)
{
    Sidebar::RouterItem::Status sidebar_status = Sidebar::RouterItem::Status::OFFLINE;
    switch (status)
    {
        case Router::Status::OFFLINE:    sidebar_status = Sidebar::RouterItem::Status::OFFLINE;    break;
        case Router::Status::CONNECTING: sidebar_status = Sidebar::RouterItem::Status::CONNECTING; break;
        case Router::Status::ONLINE:     sidebar_status = Sidebar::RouterItem::Status::ONLINE;     break;
    }

    ui->sidebar->setRouterStatus(router_id, sidebar_status);

    if (status == Router::Status::ONLINE)
    {
        refreshSidebarWorkspaces(router_id);
    }
    else
    {
        ui->sidebar->setRouterWorkspaces(router_id, {});
    }
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onSwitchContent(Sidebar::Item::Type type)
{
    switch (type)
    {
        case Sidebar::Item::Type::LOCAL_GROUP:
        {
            switchContent(local_group_widget_);
            local_group_widget_->showGroup(ui->sidebar->currentGroupId());
        }
        break;

        case Sidebar::Item::Type::ROUTER:
        {
            Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
            if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
                break;

            Sidebar::RouterItem* router = static_cast<Sidebar::RouterItem*>(sidebar_item);
            RouterWidget* widget = router_widgets_.value(router->routerId());
            if (widget)
                switchContent(widget);
        }
        break;

        case Sidebar::Item::Type::ROUTER_GROUP:
        {
            switchContent(router_group_widget_);

            Sidebar::RouterGroupItem* item =
                static_cast<Sidebar::RouterGroupItem*>(ui->sidebar->currentItem());
            router_group_widget_->showGroup(item->routerId(), item->workspaceId(),
                                            item->workspaceName(), item->groupId());
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
void HostsTab::onSidebarContextMenu(Sidebar::Item::Type type, const QPoint& pos)
{
    QMenu menu;

    if (type == Sidebar::Item::Type::LOCAL_GROUP)
    {
        menu.addAction(ui->action_add_group);
        menu.addAction(ui->action_edit_group);
        menu.addAction(ui->action_delete_group);
        menu.addSeparator();
        menu.addAction(ui->action_add_host);
    }
    else if (type == Sidebar::Item::Type::ROUTER_GROUP)
    {
        Sidebar::Item* item = ui->sidebar->currentItem();
        if (!item || item->itemType() != Sidebar::Item::Type::ROUTER_GROUP)
            return;

        auto* group_item = static_cast<Sidebar::RouterGroupItem*>(item);

        proto::router::SessionType session_type = proto::router::SESSION_TYPE_CLIENT;
        if (Router* router = Router::instance(group_item->routerId()))
            session_type = router->config().sessionType();

        // Clients are read-only and cannot manage host groups.
        if (session_type != proto::router::SESSION_TYPE_CLIENT)
        {
            menu.addAction(ui->action_add_group);
            if (!group_item->isWorkspace())
            {
                menu.addAction(ui->action_edit_group);
                menu.addAction(ui->action_delete_group);
            }
        }
    }
    else if (type == Sidebar::Item::Type::ROUTER)
    {
        Sidebar::Item* item = ui->sidebar->currentItem();
        if (!item || item->itemType() != Sidebar::Item::Type::ROUTER)
            return;

        menu.addAction(ui->action_router_status);
        menu.addAction(ui->action_edit_router);
        menu.addAction(ui->action_delete_router);
        menu.exec(pos);
        return;
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
    else
    {
        // Connect from the Hosts tab of a RouterWidget: pull selected host_id from the widget
        // and dial it via the matching router.
        Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
        if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
            return;

        Sidebar::RouterItem* router = static_cast<Sidebar::RouterItem*>(sidebar_item);
        RouterWidget* widget = router_widgets_.value(router->routerId());
        if (!widget || widget->currentTabType() != RouterWidget::TabType::HOSTS ||
            !widget->hasSelectedHost())
        {
            return;
        }

        host.setRouterId(router->routerId());
        host.setAddress(QString::number(widget->selectedHostId()));
        host.setName(widget->selectedHostName());

        if (!validateHostForConnect(host))
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
void HostsTab::onSearchContextMenu(const QPoint& pos)
{
    SearchWidget::Item* item = search_widget_->currentItem();
    if (!item)
        return;

    QMenu menu;
    menu.addAction(ui->action_desktop_connect);
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
void HostsTab::onLocalHostContextMenu(qint64 entry_id, const QPoint& pos)
{
    QMenu menu;

    if (entry_id)
    {
        menu.addAction(ui->action_desktop_connect);
        menu.addAction(ui->action_file_transfer_connect);
        menu.addAction(ui->action_chat_connect);
        menu.addAction(ui->action_system_info_connect);
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
void HostsTab::onUserContextMenu(qint64 /* router_id */, const User& user, const QPoint& pos)
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
void HostsTab::onHostContextMenu(qint64 router_id, const QPoint& pos, int column)
{
    RouterWidget* widget = router_widgets_.value(router_id);
    if (!widget || !widget->hasSelectedHost())
        return;

    QMenu menu;
    menu.addAction(ui->action_disconnect);
    menu.addAction(ui->action_host_remove);
    if (widget->isSelectedHostOnline())
    {
        menu.addSeparator();
        menu.addAction(ui->action_desktop_connect);
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
        widget->copyCurrentHostRow();
    else if (action == copy_col)
        widget->copyCurrentHostColumn(column);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onClientContextMenu(qint64 router_id, const QPoint& pos, int column)
{
    RouterWidget* widget = router_widgets_.value(router_id);
    if (!widget || !widget->hasSelectedClient())
        return;

    QMenu menu;
    menu.addAction(ui->action_disconnect);
    menu.addAction(ui->action_disconnect_all);
    menu.addSeparator();

    QAction* copy_row = menu.addAction(tr("Copy Row"));
    QAction* copy_col = menu.addAction(tr("Copy Value"));

    QAction* action = menu.exec(pos);
    if (!action)
        return;

    if (action == copy_row)
        widget->copyCurrentClientRow();
    else if (action == copy_col)
        widget->copyCurrentClientColumn(column);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onRelayContextMenu(qint64 router_id, const QPoint& pos, int column)
{
    RouterWidget* widget = router_widgets_.value(router_id);
    if (!widget || !widget->hasSelectedRelay())
        return;

    QMenu menu;
    menu.addAction(ui->action_disconnect);
    menu.addAction(ui->action_disconnect_all);
    menu.addSeparator();

    QAction* copy_row = menu.addAction(tr("Copy Row"));
    QAction* copy_col = menu.addAction(tr("Copy Value"));

    QAction* action = menu.exec(pos);
    if (!action)
        return;

    if (action == copy_row)
        widget->copyCurrentRelayRow();
    else if (action == copy_col)
        widget->copyCurrentRelayColumn(column);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onWorkspaceContextMenu(qint64 router_id, const QPoint& pos)
{
    RouterWidget* widget = router_widgets_.value(router_id);
    if (!widget)
        return;

    QMenu menu;
    if (widget->hasSelectedWorkspace())
    {
        menu.addAction(ui->action_edit_workspace);
        menu.addAction(ui->action_delete_workspace);
    }
    else
    {
        menu.addAction(ui->action_add_workspace);
    }
    menu.exec(pos);
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
    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::RouterItem* router = static_cast<Sidebar::RouterItem*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (widget)
        widget->onAddUser();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onEditUserAction()
{
    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::RouterItem* router = static_cast<Sidebar::RouterItem*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (widget)
        widget->onModifyUser();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onDeleteUserAction()
{
    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::RouterItem* router = static_cast<Sidebar::RouterItem*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (widget)
        widget->onDeleteUser();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onAddWorkspaceAction()
{
    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::RouterItem* router = static_cast<Sidebar::RouterItem*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (widget)
        widget->onAddWorkspace();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onEditWorkspaceAction()
{
    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::RouterItem* router = static_cast<Sidebar::RouterItem*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (widget)
        widget->onModifyWorkspace();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onDeleteWorkspaceAction()
{
    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::RouterItem* router = static_cast<Sidebar::RouterItem*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (widget)
        widget->onDeleteWorkspace();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onAddGroupAction()
{
    Sidebar::Item* item = ui->sidebar->currentItem();
    if (!item)
        return;

    if (item->itemType() == Sidebar::Item::LOCAL_GROUP)
    {
        ui->sidebar->onAddGroup();
        return;
    }

    if (item->itemType() != Sidebar::Item::ROUTER_GROUP)
        return;

    auto* group_item = static_cast<Sidebar::RouterGroupItem*>(item);

    // For a workspace item groupId() is 0 (the new group goes to the workspace root); for a
    // host group it is the group's own id (the new group becomes its subgroup).
    const qint64 default_parent_id = group_item->groupId();
    const qint64 router_id = group_item->routerId();

    RouterGroupDialog dialog(router_id, group_item->workspaceId(), group_item->workspaceName(),
        0, default_parent_id, this);
    if (dialog.exec() == QDialog::Accepted)
        refreshSidebarHostGroups(router_id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onEditGroupAction()
{
    Sidebar::Item* item = ui->sidebar->currentItem();
    if (!item)
        return;

    if (item->itemType() == Sidebar::Item::LOCAL_GROUP)
    {
        ui->sidebar->onEditGroup();
        return;
    }

    if (item->itemType() != Sidebar::Item::ROUTER_GROUP)
        return;

    auto* group_item = static_cast<Sidebar::RouterGroupItem*>(item);
    if (group_item->isWorkspace())
        return;

    const qint64 router_id = group_item->routerId();
    RouterGroupDialog dialog(router_id, group_item->workspaceId(), group_item->workspaceName(),
        group_item->groupId(), 0, this);
    if (dialog.exec() == QDialog::Accepted)
        refreshSidebarHostGroups(router_id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onDeleteGroupAction()
{
    Sidebar::Item* item = ui->sidebar->currentItem();
    if (!item)
        return;

    if (item->itemType() == Sidebar::Item::LOCAL_GROUP)
    {
        ui->sidebar->onRemoveGroup();
        return;
    }

    if (item->itemType() != Sidebar::Item::ROUTER_GROUP)
        return;

    auto* group_item = static_cast<Sidebar::RouterGroupItem*>(item);
    if (group_item->isWorkspace())
        return;

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
        refreshSidebarHostGroups(router_id);
    });
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onRouterStatus()
{
    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::RouterItem* router = static_cast<Sidebar::RouterItem*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (widget)
        widget->showStatusDialog();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onImportOldBookAction()
{
    LOG(INFO) << "[ACTION] Import address book";

    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::Type::LOCAL_GROUP)
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

    JsonBackup::exportToFile(this, file_path);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onImportBookAction()
{
    LOG(INFO) << "[ACTION] Import address book (json)";

    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::Type::LOCAL_GROUP)
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

    if (!JsonBackup::importFromFile(this, file_path))
        return;

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
    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::RouterItem* router = static_cast<Sidebar::RouterItem*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (!widget)
        return;

    switch (widget->currentTabType())
    {
        case RouterWidget::TabType::HOSTS:
            widget->onDisconnectHost();
            break;
        case RouterWidget::TabType::CLIENTS:
            widget->onDisconnectClient();
            break;
        case RouterWidget::TabType::RELAYS:
            widget->onDisconnectRelay();
            break;
        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onDisconnectAllAction()
{
    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::RouterItem* router = static_cast<Sidebar::RouterItem*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (!widget)
        return;

    switch (widget->currentTabType())
    {
        case RouterWidget::TabType::HOSTS:
            widget->onDisconnectAllHosts();
            break;
        case RouterWidget::TabType::CLIENTS:
            widget->onDisconnectAllClients();
            break;
        case RouterWidget::TabType::RELAYS:
            widget->onDisconnectAllRelays();
            break;
        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onRemoveHostAction()
{
    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::RouterItem* router = static_cast<Sidebar::RouterItem*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (widget)
        widget->onRemoveHost();
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
    ui->action_router_status->setVisible(false);

    ui->action_add_host->setVisible(false);
    ui->action_delete_host->setVisible(false);
    ui->action_edit_host->setVisible(false);
    ui->action_copy_host->setVisible(false);

    ui->action_desktop->setVisible(false);
    ui->action_file_transfer->setVisible(false);
    ui->action_chat->setVisible(false);
    ui->action_system_info->setVisible(false);

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
    ui->action_online_check->setVisible(false);
    ui->action_desktop_connect->setVisible(false);
    ui->action_file_transfer_connect->setVisible(false);
    ui->action_chat_connect->setVisible(false);
    ui->action_system_info_connect->setVisible(false);

    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();

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
    }
    else if (sidebar_item && sidebar_item->itemType() == Sidebar::Item::Type::LOCAL_GROUP)
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
    else if (sidebar_item && sidebar_item->itemType() == Sidebar::Item::Type::ROUTER_GROUP)
    {
        auto* group_item = static_cast<Sidebar::RouterGroupItem*>(sidebar_item);

        proto::router::SessionType session_type = proto::router::SESSION_TYPE_CLIENT;
        if (Router* router = Router::instance(group_item->routerId()))
            session_type = router->config().sessionType();

        // Clients are read-only and cannot manage host groups.
        const bool can_manage_groups = session_type != proto::router::SESSION_TYPE_CLIENT;
        ui->action_add_group->setVisible(can_manage_groups);
        ui->action_edit_group->setVisible(can_manage_groups && !group_item->isWorkspace());
        ui->action_delete_group->setVisible(can_manage_groups && !group_item->isWorkspace());
    }

    if (sidebar_item && sidebar_item->itemType() == Sidebar::Item::ROUTER)
    {
        ui->action_edit_router->setVisible(true);
        ui->action_delete_router->setVisible(true);
        ui->action_router_status->setVisible(true);

        Sidebar::RouterItem* router = static_cast<Sidebar::RouterItem*>(sidebar_item);
        RouterWidget* widget = router_widgets_.value(router->routerId());

        bool on_users_tab = widget && widget->currentTabType() == RouterWidget::TabType::USERS;
        bool has_selection = on_users_tab && widget->hasSelectedUser();

        ui->action_add_user->setVisible(on_users_tab);
        ui->action_edit_user->setVisible(has_selection);
        ui->action_delete_user->setVisible(has_selection);

        bool on_workspaces_tab = widget && widget->currentTabType() == RouterWidget::TabType::WORKSPACES;
        bool has_workspace_selection = on_workspaces_tab && widget->hasSelectedWorkspace();

        ui->action_add_workspace->setVisible(on_workspaces_tab);
        ui->action_edit_workspace->setVisible(has_workspace_selection);
        ui->action_delete_workspace->setVisible(has_workspace_selection);

        bool on_hosts_tab = widget && widget->currentTabType() == RouterWidget::TabType::HOSTS;
        bool on_clients_tab = widget && widget->currentTabType() == RouterWidget::TabType::CLIENTS;
        bool on_relays_tab = widget && widget->currentTabType() == RouterWidget::TabType::RELAYS;

        bool has_target = (on_hosts_tab && widget->hasSelectedHost()) ||
                          (on_clients_tab && widget->hasSelectedClient()) ||
                          (on_relays_tab && widget->hasSelectedRelay());
        bool has_any = (on_hosts_tab && widget->hostCount() > 0) ||
                       (on_clients_tab && widget->clientCount() > 0) ||
                       (on_relays_tab && widget->relayCount() > 0);

        ui->action_disconnect->setVisible(has_target);
        ui->action_disconnect_all->setVisible(has_any);
        ui->action_host_remove->setVisible(on_hosts_tab && widget->hasSelectedHost());

        const bool can_connect_to_host = on_hosts_tab && widget->isSelectedHostOnline();
        ui->action_desktop_connect->setVisible(can_connect_to_host);
        ui->action_file_transfer_connect->setVisible(can_connect_to_host);
        ui->action_chat_connect->setVisible(can_connect_to_host);
        ui->action_system_info_connect->setVisible(can_connect_to_host);
    }
    else
    {
        ui->action_desktop->setVisible(true);
        ui->action_file_transfer->setVisible(true);
        ui->action_chat->setVisible(true);
        ui->action_system_info->setVisible(true);
    }

    if (current_content_ != search_widget_ &&
        sidebar_item && sidebar_item->itemType() == Sidebar::Item::ROUTER_GROUP)
    {
        Sidebar::RouterGroupItem* router_group_item =
            static_cast<Sidebar::RouterGroupItem*>(sidebar_item);

        proto::router::SessionType session_type = proto::router::SESSION_TYPE_CLIENT;
        Router* router = Router::instance(router_group_item->routerId());
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
    else
        return proto::peer::SESSION_TYPE_UNKNOWN;
}

//--------------------------------------------------------------------------------------------------
void HostsTab::destroyAllRouterWidgets()
{
    const QList<qint64> ids = router_widgets_.keys();
    for (qint64 id : ids)
        destroyRouterWidget(id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::destroyRouterWidget(qint64 router_id)
{
    auto it = router_widgets_.find(router_id);
    if (it == router_widgets_.end())
        return;

    RouterWidget* widget = it.value();
    router_widgets_.erase(it);

    if (current_content_ == widget)
        current_content_ = nullptr;
    if (previous_content_ == widget)
        previous_content_ = nullptr;

    ui->content_stack->removeWidget(widget);
    widget->deleteLater();
}

//--------------------------------------------------------------------------------------------------
RouterWidget* HostsTab::createRouterWidget(const RouterConfig& config)
{
    RouterWidget* widget = new RouterWidget(config, this);

    router_widgets_.insert(config.routerId(), widget);
    ui->content_stack->addWidget(widget);

    connect(widget, &RouterWidget::sig_statusChanged, this, &HostsTab::onRouterStatusChanged);
    connect(widget, &RouterWidget::sig_currentTabTypeChanged,
            this, [this](qint64, RouterWidget::TabType) { updateActionsState(); });
    connect(widget, &RouterWidget::sig_currentUserChanged,
            this, [this](qint64) { updateActionsState(); });
    connect(widget, &RouterWidget::sig_currentHostChanged,
            this, [this](qint64) { updateActionsState(); });
    connect(widget, &RouterWidget::sig_currentClientChanged,
            this, [this](qint64) { updateActionsState(); });
    connect(widget, &RouterWidget::sig_currentRelayChanged,
            this, [this](qint64) { updateActionsState(); });
    connect(widget, &RouterWidget::sig_currentWorkspaceChanged,
            this, [this](qint64) { updateActionsState(); });
    connect(widget, &RouterWidget::sig_userContextMenu, this, &HostsTab::onUserContextMenu);
    connect(widget, &RouterWidget::sig_hostContextMenu, this, &HostsTab::onHostContextMenu);
    connect(widget, &RouterWidget::sig_clientContextMenu, this, &HostsTab::onClientContextMenu);
    connect(widget, &RouterWidget::sig_relayContextMenu, this, &HostsTab::onRelayContextMenu);
    connect(widget, &RouterWidget::sig_workspaceContextMenu, this, &HostsTab::onWorkspaceContextMenu);

    // The router is constructed inside RouterWidget's ctor so router() is non-null here. We
    // subscribe to data-change notifications to keep the sidebar tree in sync without having
    // to wait for an OFFLINE/ONLINE flap.
    if (Router* router = widget->router().data())
    {
        const qint64 router_id = config.routerId();
        connect(router, &Router::sig_workspacesChanged, this, [this, router_id]
        {
            refreshSidebarWorkspaces(router_id);
        });
        connect(router, &Router::sig_groupsChanged, this, [this, router_id]
        {
            refreshSidebarHostGroups(router_id);
        });
    }

    widget->connectToRouter();
    return widget;
}

//--------------------------------------------------------------------------------------------------
void HostsTab::refreshSidebarWorkspaces(qint64 router_id)
{
    Router* router = Router::instance(router_id);
    if (!router)
        return;

    router->listWorkspaces(0, this, [this, router_id](const Router::WorkspaceList& list)
    {
        ui->sidebar->setRouterWorkspaces(router_id, list.workspaces);

        // Once the workspace items are in place, fan out a host-group fetch per workspace.
        // Each response builds the subtree under its workspace via setRouterHostGroups().
        Router* router = Router::instance(router_id);
        if (!router)
            return;

        for (const Router::Workspace& workspace : list.workspaces)
        {
            const qint64 workspace_id = workspace.entry_id;
            router->listGroups(workspace_id, this,
                [this, router_id, workspace_id](const Router::GroupList& result)
            {
                ui->sidebar->setRouterHostGroups(router_id, workspace_id, result.groups);
            });
        }
    });
}

//--------------------------------------------------------------------------------------------------
void HostsTab::refreshSidebarHostGroups(qint64 router_id)
{
    // The workspace items already exist in the sidebar; refetch the group tree for each one
    // without disturbing the workspaces themselves.
    Router* router = Router::instance(router_id);
    if (!router)
        return;

    const QList<qint64> workspace_ids = ui->sidebar->routerWorkspaceIds(router_id);
    for (qint64 workspace_id : workspace_ids)
    {
        router->listGroups(workspace_id, this,
            [this, router_id, workspace_id](const Router::GroupList& result)
        {
            ui->sidebar->setRouterHostGroups(router_id, workspace_id, result.groups);
        });
    }
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
