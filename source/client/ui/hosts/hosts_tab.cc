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

#include "client/ui/hosts/hosts_tab.h"

#include <QActionGroup>
#include <QMenu>
#include <QStatusBar>

#include "common/ui/msg_box.h"
#include "base/logging.h"
#include "base/net/address.h"
#include "build/build_config.h"
#include "base/gui_application.h"
#include "client/local_database.h"
#include "client/router_connection.h"
#include "client/settings.h"
#include "client/ui/hosts/content_widget.h"
#include "client/ui/hosts/local_computer_dialog.h"
#include "client/ui/hosts/sidebar.h"
#include "client/ui/hosts/local_group_dialog.h"
#include "client/ui/hosts/local_group_widget.h"
#include "client/ui/hosts/router_widget.h"
#include "client/ui/hosts/router_group_widget.h"
#include "client/ui/hosts/search_widget.h"

namespace client {

//--------------------------------------------------------------------------------------------------
HostsTab::HostsTab(QWidget* parent)
    : ClientTab(Type::HOSTS, "hosts", parent)
{
    LOG(INFO) << "Ctor";

    if (!LocalDatabase::instance().isValid())
        LOG(ERROR) << "Failed to open or create book database";

    ui.setupUi(this);

    // Create toolbar actions.
    action_add_group_ = new QAction(QIcon(":/img/add-folder.svg"), tr("Add Group"), this);
    action_delete_group_ = new QAction(QIcon(":/img/remove-folder.svg"), tr("Delete Group"), this);
    action_edit_group_ = new QAction(QIcon(":/img/change-folder.svg"), tr("Edit Group"), this);

    action_add_computer_ = new QAction(QIcon(":/img/add-computer.svg"), tr("Add Computer"), this);
    action_delete_computer_ = new QAction(QIcon(":/img/remove-computer.svg"), tr("Delete Computer"), this);
    action_edit_computer_ = new QAction(QIcon(":/img/change-computer.svg"), tr("Edit Computer"), this);
    action_copy_computer_ = new QAction(QIcon(":/img/copy-computer.svg"), tr("Copy Computer"), this);

    action_desktop_ = new QAction(QIcon(":/img/workstation.svg"), tr("Desktop Management"), this);
    action_file_transfer_ = new QAction(QIcon(":/img/file-explorer.svg"), tr("File Transfer"), this);
    action_chat_ = new QAction(QIcon(":/img/chat.svg"), tr("Chat"), this);
    action_system_info_ = new QAction(QIcon(":/img/system-information.svg"), tr("System Information"), this);

    action_desktop_->setCheckable(true);
    action_file_transfer_->setCheckable(true);
    action_chat_->setCheckable(true);
    action_system_info_->setCheckable(true);

    QActionGroup* session_type_group = new QActionGroup(this);
    session_type_group->addAction(action_desktop_);
    session_type_group->addAction(action_file_transfer_);
    session_type_group->addAction(action_chat_);
    session_type_group->addAction(action_system_info_);

    action_desktop_connect_ = new QAction(QIcon(":/img/workstation.svg"), tr("Desktop Management"), this);
    action_file_transfer_connect_ = new QAction(QIcon(":/img/file-explorer.svg"), tr("File Transfer"), this);
    action_chat_connect_ = new QAction(QIcon(":/img/chat.svg"), tr("Chat"), this);
    action_system_info_connect_ = new QAction(QIcon(":/img/system-information.svg"), tr("System Information"), this);

    QActionGroup* session_connect_group = new QActionGroup(this);
    session_connect_group->addAction(action_desktop_connect_);
    session_connect_group->addAction(action_file_transfer_connect_);
    session_connect_group->addAction(action_chat_connect_);
    session_connect_group->addAction(action_system_info_connect_);

    Settings settings;

    switch (settings.sessionType())
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
            action_desktop_->setChecked(true);
            break;
        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            action_file_transfer_->setChecked(true);
            break;
        case proto::peer::SESSION_TYPE_TEXT_CHAT:
            action_chat_->setChecked(true);
            break;
        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
            action_system_info_->setChecked(true);
            break;
        default:
            break;
    }

    action_add_user_ = new QAction(QIcon(":/img/user-add.svg"), tr("Add User"), this);
    action_edit_user_ = new QAction(QIcon(":/img/user-edit.svg"), tr("Edit User"), this);
    action_delete_user_ = new QAction(QIcon(":/img/user-delete.svg"), tr("Delete User"), this);

    action_reload_ = new QAction(QIcon(":/img/reload.svg"), tr("Update"), this);

    // Create content widgets.
    local_group_widget_ = new LocalGroupWidget(this);
    router_widget_ = new RouterWidget(this);
    router_group_widget_ = new RouterGroupWidget(this);
    search_widget_ = new SearchWidget(this);

    ui.content_stack->addWidget(local_group_widget_);
    ui.content_stack->addWidget(router_widget_);
    ui.content_stack->addWidget(router_group_widget_);
    ui.content_stack->addWidget(search_widget_);

    // Setup drag-and-drop: pass the computer mime type from LocalGroupWidget to Sidebar.
    ui.sidebar->setComputerMimeType(local_group_widget_->mimeType());

    // Connect signals.
    connect(ui.sidebar, &Sidebar::sig_switchContent, this, &HostsTab::onSwitchContent);
    connect(ui.sidebar, &Sidebar::sig_contextMenu, this, &HostsTab::onSidebarContextMenu);
    connect(ui.sidebar, &Sidebar::sig_itemDropped, this, [this]()
    {
        local_group_widget_->showGroup(ui.sidebar->currentGroupId());
        updateActionsState();
    });
    connect(local_group_widget_, &LocalGroupWidget::sig_currentChanged, this, &HostsTab::onCurrentComputerChanged);
    connect(local_group_widget_, &LocalGroupWidget::sig_doubleClicked, this, &HostsTab::onLocalConnect);
    connect(local_group_widget_, &LocalGroupWidget::sig_contextMenu, this, &HostsTab::onLocalComputerContextMenu);
    connect(action_add_computer_, &QAction::triggered, this, &HostsTab::onAddComputerAction);
    connect(action_edit_computer_, &QAction::triggered, this, &HostsTab::onEditComputerAction);
    connect(action_copy_computer_, &QAction::triggered, this, &HostsTab::onCopyComputerAction);
    connect(action_delete_computer_, &QAction::triggered, this, &HostsTab::onDeleteComputerAction);
    connect(action_add_group_, &QAction::triggered, this, &HostsTab::onAddGroupAction);
    connect(action_edit_group_, &QAction::triggered, this, &HostsTab::onEditGroupAction);
    connect(action_delete_group_, &QAction::triggered, this, &HostsTab::onDeleteGroupAction);
    connect(session_connect_group, &QActionGroup::triggered, this, &HostsTab::onConnectAction);

    // Register actions for toolbar and menus.
    addActions(ActionGroup::EDIT, { action_add_user_, action_edit_user_, action_delete_user_ });
    addActions(ActionGroup::EDIT, { action_add_group_, action_edit_group_, action_delete_group_ });
    addActions(ActionGroup::EDIT, { action_add_computer_, action_edit_computer_, action_copy_computer_, action_delete_computer_ });
    addActions(ActionGroup::VIEW, { action_reload_ });
    addActions(ActionGroup::SESSION_TYPE, { action_desktop_, action_file_transfer_, action_chat_, action_system_info_ });

    local_group_widget_->showGroup(ui.sidebar->currentGroupId());
    switchContent(local_group_widget_);
    updateActionsState();

    Settings router_settings;
    RouterConfigList configs = router_settings.routerConfigs();
    for (const RouterConfig& config : std::as_const(configs))
    {
        if (config.isValid())
            createRouterConnection(config);
    }
}

//--------------------------------------------------------------------------------------------------
HostsTab::~HostsTab()
{
    LOG(INFO) << "Dtor";
    destroyAllRouterConnections();
    Settings settings;
    settings.setSessionType(defaultSessionType());
}

//--------------------------------------------------------------------------------------------------
QByteArray HostsTab::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_5_15);

        stream << local_group_widget_->saveState();
        stream << router_widget_->saveState();
        stream << router_group_widget_->saveState();
        stream << ui.splitter->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void HostsTab::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_5_15);

    QByteArray local_group_state;
    QByteArray router_state;
    QByteArray router_group_state;
    QByteArray splitter_state;

    stream >> local_group_state;
    stream >> router_state;
    stream >> router_group_state;
    stream >> splitter_state;

    if (!local_group_state.isEmpty())
        local_group_widget_->restoreState(local_group_state);

    if (!router_state.isEmpty())
        router_widget_->restoreState(router_state);

    if (!router_group_state.isEmpty())
        router_group_widget_->restoreState(router_group_state);

    if (!splitter_state.isEmpty())
    {
        ui.splitter->restoreState(splitter_state);
    }
    else
    {
        QList<int> sizes;
        sizes.emplace_back(200);
        sizes.emplace_back(width() - 200);
        ui.splitter->setSizes(sizes);
    }
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onActivated(QStatusBar* statusbar)
{
    int count = current_content_ ? current_content_->itemCount() : 0;
    statusbar->showMessage(tr("Computers: %1").arg(count));
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onDeactivated(QStatusBar* statusbar)
{
    statusbar->clearMessage();
}

//--------------------------------------------------------------------------------------------------
bool HostsTab::hasSearchField() const
{
    return true;
}

//--------------------------------------------------------------------------------------------------
void HostsTab::reloadRouters()
{
    QList<QUuid> old_uuids = ui.sidebar->routerUuids();

    Settings settings;
    RouterConfigList configs = settings.routerConfigs();

    // Remove connections for routers that no longer exist.
    for (const QUuid& uuid : std::as_const(old_uuids))
    {
        bool found = false;
        for (const RouterConfig& cfg : std::as_const(configs))
        {
            if (cfg.uuid == uuid)
            {
                found = true;
                break;
            }
        }

        if (!found)
            emit sig_destroyRouterConnection(uuid);
    }

    ui.sidebar->reloadRouters();

    // Create new or update existing connections.
    for (const RouterConfig& config : std::as_const(configs))
    {
        if (!config.isValid())
            continue;

        if (old_uuids.contains(config.uuid))
            emit sig_updateRouterConfig(config.uuid, config);
        else
            createRouterConnection(config);
    }
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onSearchTextChanged(const QString& text)
{
    if (text.isEmpty())
    {
        // Return to the previous content widget.
        if (previous_content_)
        {
            switchContent(previous_content_);
            previous_content_ = nullptr;
        }
    }
    else
    {
        // Save the current content widget before switching to search.
        if (current_content_ != search_widget_)
            previous_content_ = current_content_;

        switchContent(search_widget_);
        search_widget_->search(text);
    }
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onAddComputerAction()
{
    LOG(INFO) << "[ACTION] Add computer";

    Sidebar::Item* item = ui.sidebar->currentItem();
    if (!item)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    LocalComputerDialog dialog(-1, item->groupId(), this);
    if (dialog.exec() == LocalComputerDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    local_group_widget_->showGroup(ui.sidebar->currentGroupId());
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onEditComputerAction()
{
    LOG(INFO) << "[ACTION] Edit computer";

    LocalGroupWidget::Item* item = local_group_widget_->currentItem();
    if (!item)
    {
        LOG(INFO) << "No current local item";
        return;
    }

    LocalComputerDialog dialog(item->computerId(), item->groupId(), this);
    if (dialog.exec() == LocalComputerDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    local_group_widget_->showGroup(ui.sidebar->currentGroupId());
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onCopyComputerAction()
{
    LOG(INFO) << "[ACTION] Copy computer";

    LocalGroupWidget::Item* item = local_group_widget_->currentItem();
    if (!item)
    {
        LOG(INFO) << "No current local item";
        return;
    }

    LocalDatabase& db = LocalDatabase::instance();

    std::optional<ComputerData> computer = db.findComputer(item->computerId());
    if (!computer.has_value())
    {
        common::MsgBox::warning(this, tr("Failed to retrieve computer information from the local database."));
        return;
    }

    computer->name += " " + tr("(copy)");

    if (!db.addComputer(*computer))
    {
        common::MsgBox::warning(this, tr("Failed to add the computer to the local database."));
        return;
    }

    qint64 current_group_id = ui.sidebar->currentGroupId();

    local_group_widget_->showGroup(current_group_id);
    LocalComputerDialog(computer->id, computer->group_id, this).exec();
    local_group_widget_->showGroup(current_group_id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onDeleteComputerAction()
{
    LOG(INFO) << "[ACTION] Delete computer";

    LocalGroupWidget::Item* item = local_group_widget_->currentItem();
    if (!item)
    {
        LOG(INFO) << "No current local item";
        return;
    }

    QString message = tr("Are you sure you want to delete computer \"%1\"?").arg(item->computerName());

    if (common::MsgBox::question(this, message) == common::MsgBox::No)
    {
        LOG(INFO) << "Action is rejected by user";
        return;
    }

    if (!LocalDatabase::instance().removeComputer(item->computerId()))
    {
        common::MsgBox::warning(this, tr("Unable to remove computer"));
        LOG(INFO) << "Unable to remove computer with id" << item->computerId();
        return;
    }

    local_group_widget_->showGroup(ui.sidebar->currentGroupId());
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onAddGroupAction()
{
    LOG(INFO) << "[ACTION] Add group";

    Sidebar::Item* item = ui.sidebar->currentItem();
    if (!item)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    LocalGroupDialog dialog(-1, item->groupId(), this);
    if (dialog.exec() == LocalGroupDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    ui.sidebar->reloadGroups(item->groupId());
    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onEditGroupAction()
{
    LOG(INFO) << "[ACTION] Edit group";

    Sidebar::Item* item = ui.sidebar->currentItem();
    if (!item)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    if (item->itemType() != Sidebar::Item::LOCAL_GROUP)
        return;

    Sidebar::LocalGroup* local_group = static_cast<Sidebar::LocalGroup*>(item);
    if (local_group->groupId() == 0)
        return;

    LocalGroupDialog dialog(local_group->groupId(), local_group->parentId(), this);
    if (dialog.exec() == LocalGroupDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    ui.sidebar->reloadGroups(item->groupId());
    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onDeleteGroupAction()
{
    LOG(INFO) << "[ACTION] Delete group";

    Sidebar::Item* item = ui.sidebar->currentItem();
    if (!item)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    if (item->itemType() != Sidebar::Item::Type::LOCAL_GROUP || item->groupId() == 0) // Root group.
        return;

    Sidebar::LocalGroup* local_group = static_cast<Sidebar::LocalGroup*>(item);

    QString message = tr("Are you sure you want to delete group \"%1\"?").arg(local_group->groupName());

    if (common::MsgBox::question(this, message) == common::MsgBox::No)
    {
        LOG(INFO) << "Action is rejected by user";
        return;
    }

    qint64 parent_id = local_group->parentId();

    if (!LocalDatabase::instance().removeGroup(local_group->groupId()))
    {
        common::MsgBox::warning(this, tr("Unable to remove group"));
        LOG(INFO) << "Unable to remove group with id" << local_group->groupId();
        return;
    }

    ui.sidebar->reloadGroups(parent_id);
    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onSwitchContent(Sidebar::Item::Type type)
{
    updateActionsState();

    switch (type)
    {
        case Sidebar::Item::Type::LOCAL_GROUP:
        {
            local_group_widget_->showGroup(ui.sidebar->currentGroupId());
            switchContent(local_group_widget_);
        }
        break;

        case Sidebar::Item::Type::ROUTER:
        {
            switchContent(router_widget_);
        }
        break;

        case Sidebar::Item::Type::ROUTER_GROUP:
        {
            router_group_widget_->showGroup(ui.sidebar->currentGroupId());
            switchContent(router_group_widget_);
        }
        break;
    }
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onSidebarContextMenu(Sidebar::Item::Type type, const QPoint& pos)
{
    QMenu menu;

    if (type == Sidebar::Item::Type::LOCAL_GROUP)
    {
        menu.addAction(action_add_group_);
        menu.addAction(action_edit_group_);
        menu.addAction(action_delete_group_);
        menu.addSeparator();
        menu.addAction(action_add_computer_);
    }
    else if (type == Sidebar::Item::Type::ROUTER_GROUP)
    {
        // TODO
    }
    else if (type == Sidebar::Item::Type::ROUTER)
    {
        // TODO
    }
    else
    {
        return;
    }

    menu.exec(pos);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onCurrentComputerChanged(qint64 computer_id)
{
    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onConnectAction(QAction* action)
{
    Config config;

    if (action == action_desktop_connect_)
        config.session_type = proto::peer::SESSION_TYPE_DESKTOP;
    else if (action == action_file_transfer_connect_)
        config.session_type = proto::peer::SESSION_TYPE_FILE_TRANSFER;
    else if (action == action_chat_connect_)
        config.session_type = proto::peer::SESSION_TYPE_TEXT_CHAT;
    else if (action == action_system_info_connect_)
        config.session_type = proto::peer::SESSION_TYPE_SYSTEM_INFO;
    else
        return;

    Settings settings;
    config.display_name = settings.displayName();

    if (current_content_ == local_group_widget_)
    {
        LocalGroupWidget::Item* item = local_group_widget_->currentItem();
        if (!item)
            return;

        std::optional<ComputerData> computer =
            LocalDatabase::instance().findComputer(item->computerId());
        if (!computer.has_value())
        {
            common::MsgBox::warning(this,
                tr("Failed to retrieve computer information from the local database."));
            return;
        }

        base::Address address = base::Address::fromString(computer->address, DEFAULT_HOST_TCP_PORT);
        if (!address.isValid())
        {
            common::MsgBox::warning(this, tr("The computer has an incorrect address."));
            return;
        }

        config.address_or_id = address.host();
        config.port = address.port();
        config.computer_name = computer->name;
        config.username = computer->username;
        config.password = computer->password;
    }
    else if (current_content_ == router_group_widget_)
    {
        Sidebar::Item* sidebar_item = ui.sidebar->currentItem();
        if (!sidebar_item)
            return;

        // Find the parent Router item for the current RouterGroup.
        Sidebar::Item* router_item = sidebar_item;
        while (router_item && router_item->itemType() != Sidebar::Item::ROUTER)
            router_item = static_cast<Sidebar::Item*>(router_item->parent());

        if (!router_item)
            return;

        Sidebar::Router* router = static_cast<Sidebar::Router*>(router_item);
        RouterConfigList router_configs = settings.routerConfigs();

        for (const RouterConfig& rc : std::as_const(router_configs))
        {
            if (rc.uuid == router->uuid())
            {
                config.router_config = rc;
                break;
            }
        }
        // TODO
    }
    else
    {
        return;
    }

    emit sig_connect(config);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onLocalConnect(qint64 computer_id)
{
    std::optional<ComputerData> computer = LocalDatabase::instance().findComputer(computer_id);
    if (!computer.has_value())
    {
        common::MsgBox::warning(this,
            tr("Failed to retrieve computer information from the local database."));
        return;
    }

    base::Address address = base::Address::fromString(computer->address, DEFAULT_HOST_TCP_PORT);
    if (!address.isValid())
    {
        common::MsgBox::warning(this, tr("The computer has an incorrect address."));
        return;
    }

    Config config;
    config.address_or_id = address.host();
    config.port = address.port();
    config.session_type = defaultSessionType();
    config.display_name = Settings().displayName();
    config.computer_name = computer->name;
    config.username = computer->username;
    config.password = computer->password;

    emit sig_connect(config);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onLocalComputerContextMenu(qint64 computer_id, const QPoint& pos)
{
    QMenu menu;

    if (computer_id)
    {
        menu.addAction(action_desktop_connect_);
        menu.addAction(action_file_transfer_connect_);
        menu.addAction(action_chat_connect_);
        menu.addAction(action_system_info_connect_);
        menu.addSeparator();
        menu.addAction(action_edit_computer_);
        menu.addAction(action_copy_computer_);
        menu.addAction(action_delete_computer_);
    }
    else
    {
        menu.addAction(action_add_computer_);
    }

    menu.exec(pos);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::switchContent(ContentWidget* new_widget)
{
    if (!new_widget || new_widget == current_content_)
        return;

    current_content_ = new_widget;
    ui.content_stack->setCurrentWidget(new_widget);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::updateActionsState()
{
    action_add_group_->setVisible(false);
    action_delete_group_->setVisible(false);
    action_edit_group_->setVisible(false);

    action_add_computer_->setVisible(false);
    action_delete_computer_->setVisible(false);
    action_edit_computer_->setVisible(false);
    action_copy_computer_->setVisible(false);

    action_desktop_->setVisible(false);
    action_file_transfer_->setVisible(false);
    action_chat_->setVisible(false);
    action_system_info_->setVisible(false);

    action_add_user_->setVisible(false);
    action_edit_user_->setVisible(false);
    action_delete_user_->setVisible(false);

    action_reload_->setVisible(false);

    Sidebar::Item* sidebar_item = ui.sidebar->currentItem();

    if (sidebar_item && sidebar_item->itemType() == Sidebar::Item::Type::LOCAL_GROUP)
    {
        action_add_group_->setVisible(true);
        action_delete_group_->setVisible(sidebar_item->groupId() != 0);
        action_edit_group_->setVisible(sidebar_item->groupId() != 0);

        LocalGroupWidget::Item* computer_item = local_group_widget_->currentItem();

        action_add_computer_->setVisible(true);
        action_delete_computer_->setVisible(computer_item != nullptr);
        action_edit_computer_->setVisible(computer_item != nullptr);
        action_copy_computer_->setVisible(computer_item != nullptr);
    }

    if (sidebar_item && sidebar_item->itemType() == Sidebar::Item::ROUTER)
    {
        action_add_user_->setVisible(true);
        action_edit_user_->setVisible(true);
        action_delete_user_->setVisible(true);

        action_reload_->setVisible(true);
    }
    else
    {
        action_desktop_->setVisible(true);
        action_file_transfer_->setVisible(true);
        action_chat_->setVisible(true);
        action_system_info_->setVisible(true);
    }
}

//--------------------------------------------------------------------------------------------------
proto::peer::SessionType HostsTab::defaultSessionType() const
{
    if (action_desktop_->isChecked())
        return proto::peer::SESSION_TYPE_DESKTOP;
    else if (action_file_transfer_->isChecked())
        return proto::peer::SESSION_TYPE_FILE_TRANSFER;
    else if (action_chat_->isChecked())
        return proto::peer::SESSION_TYPE_TEXT_CHAT;
    else if (action_system_info_->isChecked())
        return proto::peer::SESSION_TYPE_SYSTEM_INFO;
    else
        return proto::peer::SESSION_TYPE_UNKNOWN;
}

//--------------------------------------------------------------------------------------------------
void HostsTab::destroyAllRouterConnections()
{
    QList<QUuid> uuids = ui.sidebar->routerUuids();
    for (const QUuid& uuid : std::as_const(uuids))
        emit sig_destroyRouterConnection(uuid);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::createRouterConnection(const RouterConfig& config)
{
    RouterConnection* conn = new RouterConnection(config);
    conn->moveToThread(base::GuiApplication::ioThread());

    QUuid uuid = config.uuid;

    connect(conn, &RouterConnection::sig_statusChanged, this,
            [this](const QUuid& uuid, RouterConnection::Status status)
    {
        Sidebar::Router* router = ui.sidebar->routerByUuid(uuid);
        if (!router)
            return;

        switch (status)
        {
            case RouterConnection::Status::OFFLINE:
                router->setStatus(Sidebar::Router::Status::OFFLINE);
                break;
            case RouterConnection::Status::CONNECTING:
                router->setStatus(Sidebar::Router::Status::CONNECTING);
                break;
            case RouterConnection::Status::ONLINE:
                router->setStatus(Sidebar::Router::Status::ONLINE);
                break;
        }
    }, Qt::QueuedConnection);

    connect(this, &HostsTab::sig_destroyRouterConnection, conn,
            [conn, uuid](const QUuid& destroy_uuid)
    {
        if (destroy_uuid == uuid)
            conn->deleteLater();
    }, Qt::QueuedConnection);

    connect(this, &HostsTab::sig_updateRouterConfig, conn,
            [conn](const QUuid& target_uuid, const RouterConfig& config)
    {
        if (target_uuid == conn->uuid())
            conn->onUpdateConfig(config);
    }, Qt::QueuedConnection);

    connect(conn, &RouterConnection::sig_relayListReceived,
            router_widget_, &RouterWidget::onRelayListReceived, Qt::QueuedConnection);
    connect(router_widget_, &RouterWidget::sig_relayListRequest,
            conn, &RouterConnection::onRelayListRequest, Qt::QueuedConnection);

    QMetaObject::invokeMethod(conn, &RouterConnection::onConnectToRouter, Qt::QueuedConnection);
}

} // namespace client
