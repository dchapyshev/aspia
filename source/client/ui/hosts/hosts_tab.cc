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

#include <optional>

#include <QActionGroup>
#include <QDateTime>
#include <QMenu>
#include <QStatusBar>

#include "common/ui/msg_box.h"
#include "base/logging.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "base/peer/user.h"
#include "build/build_config.h"
#include "client/database.h"
#include "client/settings.h"
#include "client/ui/hosts/content_widget.h"
#include "client/ui/hosts/local_computer_dialog.h"
#include "client/ui/hosts/sidebar.h"
#include "client/ui/hosts/local_group_dialog.h"
#include "client/ui/hosts/local_group_widget.h"
#include "client/ui/hosts/router_widget.h"
#include "client/ui/hosts/router_group_widget.h"
#include "client/ui/hosts/search_widget.h"
#include "client/ui/router_dialog.h"

namespace client {

//--------------------------------------------------------------------------------------------------
HostsTab::HostsTab(QWidget* parent)
    : ClientTab(Type::HOSTS, "hosts", parent)
{
    LOG(INFO) << "Ctor";

    if (!Database::instance().isValid())
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

    action_disconnect_ = new QAction(QIcon(":/img/host-disconnect.svg"), tr("Disconnect"), this);
    action_disconnect_all_ = new QAction(QIcon(":/img/host-disconnect-all.svg"), tr("Disconnect All"), this);

    action_host_remove_ = new QAction(QIcon(":/img/remove-computer.svg"), tr("Remove"), this);

    action_save_ = new QAction(QIcon(":/img/save.svg"), tr("Save..."), this);
    action_reload_ = new QAction(QIcon(":/img/reload.svg"), tr("Reload"), this);

    // Create content widgets.
    local_group_widget_ = new LocalGroupWidget(this);
    router_group_widget_ = new RouterGroupWidget(this);
    search_widget_ = new SearchWidget(this);

    ui.content_stack->addWidget(local_group_widget_);
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
    connect(this, &HostsTab::sig_connect, local_group_widget_,
            [this](qint64 computer_id, const ComputerConfig&, proto::peer::SessionType)
    {
        if (computer_id != -1)
            local_group_widget_->setConnectTime(computer_id, QDateTime::currentSecsSinceEpoch());
    });
    connect(action_add_computer_, &QAction::triggered, this, &HostsTab::onAddComputerAction);
    connect(action_edit_computer_, &QAction::triggered, this, &HostsTab::onEditComputerAction);
    connect(action_copy_computer_, &QAction::triggered, this, &HostsTab::onCopyComputerAction);
    connect(action_delete_computer_, &QAction::triggered, this, &HostsTab::onDeleteComputerAction);
    connect(action_add_group_, &QAction::triggered, this, &HostsTab::onAddGroupAction);
    connect(action_edit_group_, &QAction::triggered, this, &HostsTab::onEditGroupAction);
    connect(action_delete_group_, &QAction::triggered, this, &HostsTab::onDeleteGroupAction);
    connect(action_add_user_, &QAction::triggered, this, &HostsTab::onAddUserAction);
    connect(action_edit_user_, &QAction::triggered, this, &HostsTab::onEditUserAction);
    connect(action_delete_user_, &QAction::triggered, this, &HostsTab::onDeleteUserAction);
    connect(action_reload_, &QAction::triggered, this, &HostsTab::onReloadAction);
    connect(action_save_, &QAction::triggered, this, &HostsTab::onSaveAction);
    connect(action_disconnect_, &QAction::triggered, this, &HostsTab::onDisconnectAction);
    connect(action_disconnect_all_, &QAction::triggered, this, &HostsTab::onDisconnectAllAction);
    connect(action_host_remove_, &QAction::triggered, this, &HostsTab::onRemoveHostAction);
    connect(session_connect_group, &QActionGroup::triggered, this, &HostsTab::onConnectAction);

    // Register actions for toolbar and menus.
    addActions(ActionRole::FILE, { action_save_ });
    addActions(ActionRole::EDIT, { action_add_user_, action_edit_user_, action_delete_user_ });
    addActions(ActionRole::EDIT, { action_add_group_, action_edit_group_, action_delete_group_ });
    addActions(ActionRole::EDIT, { action_add_computer_, action_edit_computer_, action_copy_computer_, action_delete_computer_ });
    addActions(ActionRole::EDIT, { action_host_remove_, action_disconnect_, action_disconnect_all_ });
    addActions(ActionRole::VIEW, { action_reload_ });
    addActions(ActionRole::SESSION_TYPE, { action_desktop_, action_file_transfer_, action_chat_, action_system_info_ });

    local_group_widget_->showGroup(ui.sidebar->currentGroupId());
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
        stream.setVersion(QDataStream::Qt_5_15);

        stream << local_group_widget_->saveState();
        stream << router_group_widget_->saveState();
        stream << ui.splitter->saveState();

        QByteArray routers_buffer;
        {
            QDataStream routers_stream(&routers_buffer, QIODevice::WriteOnly);
            routers_stream.setVersion(QDataStream::Qt_5_15);

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
    stream.setVersion(QDataStream::Qt_5_15);

    QByteArray local_group_state;
    QByteArray router_group_state;
    QByteArray splitter_state;
    QByteArray routers_buffer;

    stream >> local_group_state;
    stream >> router_group_state;
    stream >> splitter_state;
    stream >> routers_buffer;

    if (!local_group_state.isEmpty())
        local_group_widget_->restoreState(local_group_state);

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

    if (!routers_buffer.isEmpty())
    {
        QDataStream routers_stream(routers_buffer);
        routers_stream.setVersion(QDataStream::Qt_5_15);

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
void HostsTab::attach(QStatusBar* statusbar)
{
    statusbar_ = statusbar;
    if (current_content_ && statusbar_)
        current_content_->attachStatusBar(statusbar_);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::detach(QStatusBar* /* statusbar */)
{
    if (current_content_ && statusbar_)
        current_content_->detachStatusBar(statusbar_);
    statusbar_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
bool HostsTab::hasSearchField() const
{
    return true;
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
            if (router.router_id == id)
            {
                found = true;
                break;
            }
        }

        if (!found)
            destroyRouterWidget(id);
    }

    ui.sidebar->reloadRouters();

    // Create new or update existing widgets.
    for (const RouterConfig& router : std::as_const(routers))
    {
        if (!router.isValid())
            continue;

        auto it = router_widgets_.find(router.router_id);
        if (it != router_widgets_.end())
            it.value()->updateConfig(router);
        else
            createRouterWidget(router);
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

    Database& db = Database::instance();

    std::optional<ComputerConfig> computer = db.findComputer(item->computerId());
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

    if (!Database::instance().removeComputer(item->computerId()))
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

    if (!Database::instance().removeGroup(local_group->groupId()))
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
            Sidebar::Item* sidebar_item = ui.sidebar->currentItem();
            if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
                break;

            Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
            RouterWidget* widget = router_widgets_.value(router->routerId());
            if (widget)
                switchContent(widget);
        }
        break;

        case Sidebar::Item::Type::ROUTER_GROUP:
        {
            router_group_widget_->showGroup(ui.sidebar->currentGroupId());
            switchContent(router_group_widget_);
        }
        break;
    }

    updateActionsState();
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
        return;
    }
    else if (type == Sidebar::Item::Type::ROUTER)
    {
        Sidebar::Item* item = ui.sidebar->currentItem();
        if (!item || item->itemType() != Sidebar::Item::Type::ROUTER)
            return;

        qint64 router_id = static_cast<Sidebar::Router*>(item)->routerId();

        QAction* status_action = menu.addAction(QIcon(":/img/info.svg"), tr("Status"));
        QAction* edit_action = menu.addAction(QIcon(":/img/pencil-drawing.svg"), tr("Edit"));
        QAction* delete_action = menu.addAction(QIcon(":/img/cancel.svg"), tr("Delete"));

        QAction* result = menu.exec(pos);
        if (result == status_action)
        {
            RouterWidget* widget = router_widgets_.value(router_id);
            if (widget)
                widget->showStatusDialog();
        }
        else if (result == edit_action)
            editRouter(router_id);
        else if (result == delete_action)
            deleteRouter(router_id);
        return;
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
    proto::peer::SessionType session_type;

    if (action == action_desktop_connect_)
        session_type = proto::peer::SESSION_TYPE_DESKTOP;
    else if (action == action_file_transfer_connect_)
        session_type = proto::peer::SESSION_TYPE_FILE_TRANSFER;
    else if (action == action_chat_connect_)
        session_type = proto::peer::SESSION_TYPE_TEXT_CHAT;
    else if (action == action_system_info_connect_)
        session_type = proto::peer::SESSION_TYPE_SYSTEM_INFO;
    else
        return;

    ComputerConfig computer;
    qint64 source_computer_id = -1;

    if (current_content_ == local_group_widget_)
    {
        LocalGroupWidget::Item* item = local_group_widget_->currentItem();
        if (!item)
            return;

        std::optional<ComputerConfig> found = Database::instance().findComputer(item->computerId());
        if (!found.has_value())
        {
            common::MsgBox::warning(this,
                tr("Failed to retrieve computer information from the local database."));
            return;
        }

        computer = *found;

        if (!validateComputerForConnect(computer))
            return;

        Database::instance().setConnectTime(computer.id, QDateTime::currentSecsSinceEpoch());
        source_computer_id = computer.id;
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
        std::optional<RouterConfig> router_data = Database::instance().findRouter(router->routerId());
        if (router_data)
            computer.router_id = router_data->router_id;
        // TODO
    }
    else
    {
        return;
    }

    emit sig_connect(source_computer_id, computer, session_type);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onLocalConnect(qint64 computer_id)
{
    std::optional<ComputerConfig> computer = Database::instance().findComputer(computer_id);
    if (!computer.has_value())
    {
        common::MsgBox::warning(this, tr("Failed to retrieve computer information from the local database."));
        return;
    }

    if (!validateComputerForConnect(*computer))
        return;

    Database::instance().setConnectTime(computer_id, QDateTime::currentSecsSinceEpoch());

    emit sig_connect(computer_id, *computer, defaultSessionType());
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

    if (current_content_ && statusbar_)
        current_content_->detachStatusBar(statusbar_);

    current_content_ = new_widget;
    ui.content_stack->setCurrentWidget(new_widget);

    if (statusbar_)
        current_content_->attachStatusBar(statusbar_);
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
    action_save_->setVisible(false);
    action_disconnect_->setVisible(false);
    action_disconnect_all_->setVisible(false);
    action_host_remove_->setVisible(false);

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
        Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
        RouterWidget* widget = router_widgets_.value(router->routerId());

        bool on_users_tab = widget && widget->currentTabType() == RouterWidget::TabType::USERS;
        bool has_selection = on_users_tab && widget->hasSelectedUser();

        action_add_user_->setVisible(on_users_tab);
        action_edit_user_->setVisible(has_selection);
        action_delete_user_->setVisible(has_selection);

        bool on_hosts_tab = widget && widget->currentTabType() == RouterWidget::TabType::HOSTS;
        bool on_relays_tab = widget && widget->currentTabType() == RouterWidget::TabType::RELAYS;

        bool has_target = (on_hosts_tab && widget->hasSelectedHost()) ||
                          (on_relays_tab && widget->hasSelectedRelay());
        bool has_any = (on_hosts_tab && widget->hostCount() > 0) ||
                       (on_relays_tab && widget->relayCount() > 0);

        action_disconnect_->setVisible(has_target);
        action_disconnect_all_->setVisible(has_any);
        action_host_remove_->setVisible(on_hosts_tab && widget->hasSelectedHost());
    }
    else
    {
        action_desktop_->setVisible(true);
        action_file_transfer_->setVisible(true);
        action_chat_->setVisible(true);
        action_system_info_->setVisible(true);
    }

    action_reload_->setVisible(current_content_ && current_content_->canReload());
    action_save_->setVisible(current_content_ && current_content_->canSave());
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
void HostsTab::onRouterStatusChanged(qint64 router_id, RouterConnection::Status status)
{
    Sidebar::Router* router = ui.sidebar->routerById(router_id);
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

    ui.content_stack->removeWidget(widget);
    widget->deleteLater();
}

//--------------------------------------------------------------------------------------------------
RouterWidget* HostsTab::createRouterWidget(const RouterConfig& config)
{
    RouterWidget* widget = new RouterWidget(config, this);

    router_widgets_.insert(config.router_id, widget);
    ui.content_stack->addWidget(widget);

    connect(widget, &RouterWidget::sig_statusChanged, this, &HostsTab::onRouterStatusChanged);
    connect(widget, &RouterWidget::sig_currentTabTypeChanged,
            this, [this](qint64, RouterWidget::TabType) { updateActionsState(); });
    connect(widget, &RouterWidget::sig_currentUserChanged,
            this, [this](qint64) { updateActionsState(); });
    connect(widget, &RouterWidget::sig_currentHostChanged,
            this, [this](qint64) { updateActionsState(); });
    connect(widget, &RouterWidget::sig_currentRelayChanged,
            this, [this](qint64) { updateActionsState(); });
    connect(widget, &RouterWidget::sig_userContextMenu, this, &HostsTab::onUserContextMenu);
    connect(widget, &RouterWidget::sig_hostContextMenu, this, &HostsTab::onHostContextMenu);
    connect(widget, &RouterWidget::sig_relayContextMenu, this, &HostsTab::onRelayContextMenu);

    widget->connectToRouter();
    return widget;
}

//--------------------------------------------------------------------------------------------------
void HostsTab::editRouter(qint64 router_id)
{
    LOG(INFO) << "[ACTION] Edit router" << router_id;

    RouterDialog dialog(router_id, this);
    if (dialog.exec() == QDialog::Accepted)
        reloadRouters();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::deleteRouter(qint64 router_id)
{
    LOG(INFO) << "[ACTION] Delete router" << router_id;

    Database& db = Database::instance();
    std::optional<RouterConfig> existing = db.findRouter(router_id);
    if (!existing)
    {
        LOG(ERROR) << "Router not found for id:" << router_id;
        return;
    }

    QString message = tr("Are you sure you want to delete router \"%1\"?").arg(existing->display_name);
    if (common::MsgBox::question(this, message) == common::MsgBox::No)
    {
        LOG(INFO) << "Action is rejected by user";
        return;
    }

    db.removeRouter(router_id);
    reloadRouters();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onUserContextMenu(qint64 /* router_id */, const base::User& user, const QPoint& pos)
{
    QMenu menu;
    if (user.isValid())
    {
        menu.addAction(action_edit_user_);
        menu.addAction(action_delete_user_);
    }
    else
    {
        menu.addAction(action_add_user_);
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
    menu.addAction(action_disconnect_);
    menu.addAction(action_host_remove_);
    menu.addSeparator();

    QAction* copy_row = menu.addAction(tr("Copy Row"));
    QAction* copy_col = menu.addAction(tr("Copy Value"));

    QAction* action = menu.exec(pos);
    if (!action)
        return;

    if (action == copy_row)
        widget->copyCurrentHostRow();
    else if (action == copy_col)
        widget->copyCurrentHostColumn(column);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onRelayContextMenu(qint64 router_id, const QPoint& pos, int column)
{
    RouterWidget* widget = router_widgets_.value(router_id);
    if (!widget || !widget->hasSelectedRelay())
        return;

    QMenu menu;
    menu.addAction(action_disconnect_);
    menu.addAction(action_disconnect_all_);
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
void HostsTab::onAddUserAction()
{
    Sidebar::Item* sidebar_item = ui.sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (widget)
        widget->onAddUser();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onEditUserAction()
{
    Sidebar::Item* sidebar_item = ui.sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (widget)
        widget->onModifyUser();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onDeleteUserAction()
{
    Sidebar::Item* sidebar_item = ui.sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (widget)
        widget->onDeleteUser();
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
    Sidebar::Item* sidebar_item = ui.sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (!widget)
        return;

    switch (widget->currentTabType())
    {
        case RouterWidget::TabType::HOSTS:
            widget->onDisconnectHost();
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
    Sidebar::Item* sidebar_item = ui.sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (!widget)
        return;

    switch (widget->currentTabType())
    {
        case RouterWidget::TabType::HOSTS:
            widget->onDisconnectAllHosts();
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
    Sidebar::Item* sidebar_item = ui.sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (widget)
        widget->onRemoveHost();
}

//--------------------------------------------------------------------------------------------------
bool HostsTab::validateComputerForConnect(const ComputerConfig& computer)
{
    if (computer.router_id != 0)
    {
        std::optional<RouterConfig> router = Database::instance().findRouter(computer.router_id);
        if (!router.has_value())
        {
            common::MsgBox::warning(this, tr("The router associated with this computer has been deleted. "
                "Edit the computer to select another router or switch to direct connection."));
            return false;
        }

        if (!base::isHostId(computer.address))
        {
            common::MsgBox::warning(this, tr("The computer has an invalid host ID."));
            return false;
        }
    }
    else
    {
        base::Address address = base::Address::fromString(computer.address, DEFAULT_HOST_TCP_PORT);
        if (!address.isValid())
        {
            common::MsgBox::warning(this, tr("The computer has an incorrect address."));
            return false;
        }
    }

    return true;
}

} // namespace client
