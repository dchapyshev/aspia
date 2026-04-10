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

#include "client/ui/computers_tab/computers_tab.h"

#include <QActionGroup>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>

#include "base/logging.h"
#include "base/net/address.h"
#include "build/build_config.h"
#include "client/local_database.h"
#include "client/ui/settings.h"
#include "client/ui/computers_tab/content_tree_item.h"
#include "client/ui/computers_tab/content_widget.h"
#include "client/ui/computers_tab/group_tree_item.h"
#include "client/ui/computers_tab/local_computer_dialog.h"
#include "client/ui/computers_tab/local_group_dialog.h"
#include "client/ui/computers_tab/local_group_widget.h"
#include "client/ui/computers_tab/router_widget.h"
#include "client/ui/computers_tab/router_group_widget.h"
#include "client/ui/computers_tab/search_widget.h"

namespace client {

//--------------------------------------------------------------------------------------------------
ComputersTab::ComputersTab(QWidget* parent)
    : ClientTab(Type::COMPUTERS, "computers", parent)
{
    LOG(INFO) << "Ctor";

    ui.setupUi(this);

    // Create toolbar actions.
    action_add_group_ = new QAction(QIcon(":/img/add-folder.svg"), tr("Add Group"), this);
    action_delete_group_ = new QAction(QIcon(":/img/remove-folder.svg"), tr("Delete Group"), this);
    action_edit_group_ = new QAction(QIcon(":/img/change-folder.svg"), tr("Edit Group"), this);

    action_add_computer_ = new QAction(QIcon(":/img/add-computer.svg"), tr("Add Computer"), this);
    action_delete_computer_ = new QAction(QIcon(":/img/remove-computer.svg"), tr("Delete Computer"), this);
    action_edit_computer_ = new QAction(QIcon(":/img/change-computer.svg"), tr("Edit Computer"), this);
    action_copy_computer_ = new QAction(QIcon(":/img/copy-computer.svg"), tr("Copy Computer"), this);

    action_desktop_ = new QAction(QIcon(":/img/workstation.svg"), tr("Desktop Manage"), this);
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

    action_desktop_connect_ = new QAction(QIcon(":/img/workstation.svg"), tr("Desktop Manage"), this);
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

    GroupData local_root_data;
    local_root_data.id = 0;
    local_root_data.parent_id = 0;
    local_root_data.name = tr("Local");

    // Create root groups in the tree.
    local_root_ = new LocalGroupItem(local_root_data, ui.tree_group);
    local_root_->setExpanded(true);

    remote_root_ = new RouterItem(tr("Remote"), ui.tree_group);
    remote_root_->setExpanded(true);

    // Create content widgets.
    local_group_widget_ = new LocalGroupWidget(this);
    router_widget_ = new RouterWidget(this);
    router_group_widget_ = new RouterGroupWidget(this);
    search_widget_ = new SearchWidget(this);

    ui.content_stack->addWidget(local_group_widget_);
    ui.content_stack->addWidget(router_widget_);
    ui.content_stack->addWidget(router_group_widget_);
    ui.content_stack->addWidget(search_widget_);

    switchContent(local_group_widget_);

    // Connect signals.
    connect(ui.tree_group, &QTreeWidget::currentItemChanged, this, &ComputersTab::onCurrentGroupChanged);
    connect(ui.tree_group, &QTreeWidget::customContextMenuRequested, this, &ComputersTab::onGroupContextMenu);
    connect(local_group_widget_, &LocalGroupWidget::sig_currentComputerChanged, this, &ComputersTab::onCurrentComputerChanged);
    connect(local_group_widget_, &LocalGroupWidget::sig_computerDoubleClicked, this, &ComputersTab::onLocalConnect);
    connect(local_group_widget_, &LocalGroupWidget::sig_computerContextMenu, this, &ComputersTab::onLocalComputerContextMenu);
    connect(action_add_computer_, &QAction::triggered, this, &ComputersTab::onAddComputerAction);
    connect(action_edit_computer_, &QAction::triggered, this, &ComputersTab::onEditComputerAction);
    connect(action_copy_computer_, &QAction::triggered, this, &ComputersTab::onCopyComputerAction);
    connect(action_delete_computer_, &QAction::triggered, this, &ComputersTab::onDeleteComputerAction);
    connect(action_add_group_, &QAction::triggered, this, &ComputersTab::onAddGroupAction);
    connect(action_edit_group_, &QAction::triggered, this, &ComputersTab::onEditGroupAction);
    connect(action_delete_group_, &QAction::triggered, this, &ComputersTab::onDeleteGroupAction);
    connect(session_connect_group, &QActionGroup::triggered, this, &ComputersTab::onConnectAction);

    // Register actions for toolbar and menus.
    addActions(ActionGroup::EDIT, { action_add_group_, action_edit_group_, action_delete_group_ });
    addActions(ActionGroup::EDIT, { action_add_computer_, action_edit_computer_, action_copy_computer_, action_delete_computer_ });
    addActions(ActionGroup::SESSION_TYPE, { action_desktop_, action_file_transfer_, action_chat_, action_system_info_ });

    if (!LocalDatabase::instance().isValid())
        LOG(ERROR) << "Failed to open or create book database";

    // Load groups from database under Local root.
    loadGroups(0, local_root_);

    // Show computers in root group by default.
    ui.tree_group->setCurrentItem(local_root_);
    local_group_widget_->showGroup(0);
    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
ComputersTab::~ComputersTab()
{
    LOG(INFO) << "Dtor";
    Settings settings;
    settings.setSessionType(defaultSessionType());
}

//--------------------------------------------------------------------------------------------------
QByteArray ComputersTab::saveState()
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
void ComputersTab::restoreState(const QByteArray& state)
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
void ComputersTab::onActivated(QStatusBar* statusbar)
{
    int count = current_content_ ? current_content_->itemCount() : 0;
    statusbar->showMessage(tr("Computers: %1").arg(count));
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::onDeactivated(QStatusBar* statusbar)
{
    statusbar->clearMessage();
}

//--------------------------------------------------------------------------------------------------
bool ComputersTab::hasSearchField() const
{
    return true;
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::onSearchTextChanged(const QString& text)
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
void ComputersTab::onAddComputerAction()
{
    LOG(INFO) << "[ACTION] Add computer";

    LocalGroupItem* item = static_cast<LocalGroupItem*>(ui.tree_group->currentItem());
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

    local_group_widget_->showGroup(current_group_id_);
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::onEditComputerAction()
{
    LOG(INFO) << "[ACTION] Edit computer";

    LocalComputerItem* item = local_group_widget_->currentComputer();
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

    local_group_widget_->showGroup(current_group_id_);
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::onCopyComputerAction()
{
    LOG(INFO) << "[ACTION] Copy computer";

    LocalComputerItem* item = local_group_widget_->currentComputer();
    if (!item)
    {
        LOG(INFO) << "No current local item";
        return;
    }

    LocalDatabase& db = LocalDatabase::instance();

    std::optional<ComputerData> computer = db.findComputer(item->computerId());
    if (!computer.has_value())
    {
        QMessageBox::warning(this, tr("Warning"), tr("Failed to retrieve computer information from the local database."));
        return;
    }

    computer->name += " " + tr("(copy)");

    if (!db.addComputer(*computer))
    {
        QMessageBox::warning(this, tr("Warning"), tr("Failed to add the computer to the local database."));
        return;
    }

    local_group_widget_->showGroup(current_group_id_);
    LocalComputerDialog(computer->id, computer->group_id, this).exec();
    local_group_widget_->showGroup(current_group_id_);
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::onDeleteComputerAction()
{
    LOG(INFO) << "[ACTION] Delete computer";

    LocalComputerItem* item = local_group_widget_->currentComputer();
    if (!item)
    {
        LOG(INFO) << "No current local item";
        return;
    }

    QString message = tr("Are you sure you want to delete computer \"%1\"?").arg(item->computerName());

    QMessageBox messagebox(QMessageBox::Question, tr("Confirmation"), message,
                            QMessageBox::Yes | QMessageBox::No, this);
    messagebox.button(QMessageBox::Yes)->setText(tr("Yes"));
    messagebox.button(QMessageBox::No)->setText(tr("No"));

    if (messagebox.exec() == QMessageBox::No)
    {
        LOG(INFO) << "Action is rejected by user";
        return;
    }

    if (!LocalDatabase::instance().removeComputer(item->computerId()))
    {
        QMessageBox::warning(this, tr("Warning"), tr("Unable to remove computer"));
        LOG(INFO) << "Unable to remove computer with id" << item->computerId();
        return;
    }

    local_group_widget_->showGroup(current_group_id_);
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::onAddGroupAction()
{
    LOG(INFO) << "[ACTION] Add group";

    LocalGroupItem* item = static_cast<LocalGroupItem*>(ui.tree_group->currentItem());
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

    reloadGroups(item->groupId());
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::onEditGroupAction()
{
    LOG(INFO) << "[ACTION] Edit group";

    LocalGroupItem* item = static_cast<LocalGroupItem*>(ui.tree_group->currentItem());
    if (!item)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    LocalGroupDialog dialog(item->groupId(), item->parentId(), this);
    if (dialog.exec() == LocalGroupDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    reloadGroups(item->groupId());
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::onDeleteGroupAction()
{
    LOG(INFO) << "[ACTION] Delete group";

    LocalGroupItem* item = static_cast<LocalGroupItem*>(ui.tree_group->currentItem());
    if (!item)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    if (item->groupId() == 0) // Root group.
        return;

    QString message = tr("Are you sure you want to delete group \"%1\"?").arg(item->groupName());

    QMessageBox messagebox(QMessageBox::Question, tr("Confirmation"), message,
                           QMessageBox::Yes | QMessageBox::No, this);
    messagebox.button(QMessageBox::Yes)->setText(tr("Yes"));
    messagebox.button(QMessageBox::No)->setText(tr("No"));

    if (messagebox.exec() == QMessageBox::No)
    {
        LOG(INFO) << "Action is rejected by user";
        return;
    }

    qint64 parent_id = item->parentId();

    if (!LocalDatabase::instance().removeGroup(item->groupId()))
    {
        QMessageBox::warning(this, tr("Warning"), tr("Unable to remove group"));
        LOG(INFO) << "Unable to remove group with id" << item->groupId();
        return;
    }

    reloadGroups(parent_id);
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::onCurrentGroupChanged(QTreeWidgetItem* current, QTreeWidgetItem* /* previous */)
{
    updateActionsState();

    if (!current)
        return;

    GroupTreeItem* group_item = static_cast<GroupTreeItem*>(current);

    switch (group_item->itemType())
    {
        case GroupTreeItem::Type::LOCAL_GROUP:
        {
            current_group_id_ = group_item->groupId();
            local_group_widget_->showGroup(current_group_id_);
            switchContent(local_group_widget_);
        }
        break;

        case GroupTreeItem::Type::ROUTER:
        {
            switchContent(router_widget_);
        }
        break;

        case GroupTreeItem::Type::ROUTER_GROUP:
        {
            router_group_widget_->showGroup(group_item->groupId());
            switchContent(router_group_widget_);
        }
        break;
    }
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::onGroupContextMenu(const QPoint& pos)
{
    GroupTreeItem* item = static_cast<GroupTreeItem*>(ui.tree_group->itemAt(pos));
    if (!item)
        return;

    QMenu menu;

    if (item->itemType() == GroupTreeItem::Type::LOCAL_GROUP)
    {
        menu.addAction(action_edit_group_);
        menu.addAction(action_delete_group_);
        menu.addSeparator();
        menu.addAction(action_add_group_);
    }
    else if (item->itemType() == GroupTreeItem::Type::ROUTER_GROUP)
    {
        // TODO
    }
    else if (item->itemType() == GroupTreeItem::Type::ROUTER)
    {
        // TODO
    }
    else
    {
        return;
    }

    menu.exec(ui.tree_group->viewport()->mapToGlobal(pos));
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::onCurrentComputerChanged(qint64 computer_id)
{
    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::onConnectAction(QAction* action)
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
        LocalComputerItem* item = local_group_widget_->currentComputer();
        if (!item)
            return;

        std::optional<ComputerData> computer =
            LocalDatabase::instance().findComputer(item->computerId());
        if (!computer.has_value())
        {
            QMessageBox::warning(this, tr("Warning"),
                tr("Failed to retrieve computer information from the local database."));
            return;
        }

        base::Address address = base::Address::fromString(computer->address, DEFAULT_HOST_TCP_PORT);
        if (!address.isValid())
        {
            QMessageBox::warning(this, tr("Warning"), tr("The computer has an incorrect address."));
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
        config.router_config = settings.routerConfig();
        // TODO
    }
    else
    {
        return;
    }

    emit sig_connect(config);
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::onLocalConnect(qint64 computer_id)
{
    std::optional<ComputerData> computer = LocalDatabase::instance().findComputer(computer_id);
    if (!computer.has_value())
    {
        QMessageBox::warning(this, tr("Warning"),
            tr("Failed to retrieve computer information from the local database."));
        return;
    }

    base::Address address = base::Address::fromString(computer->address, DEFAULT_HOST_TCP_PORT);
    if (!address.isValid())
    {
        QMessageBox::warning(this, tr("Warning"), tr("The computer has an incorrect address."));
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
void ComputersTab::onLocalComputerContextMenu(qint64 computer_id, const QPoint& pos)
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
void ComputersTab::loadGroups(qint64 parent_id, QTreeWidgetItem* parent_item)
{
    QList<GroupData> groups = LocalDatabase::instance().groupList(parent_id);

    for (const GroupData& group : std::as_const(groups))
    {
        LocalGroupItem* item = new LocalGroupItem(group, parent_item);
        item->setExpanded(group.expanded);

        // Load child groups recursively.
        loadGroups(group.id, item);
    }
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::reloadGroups(qint64 selected_group_id)
{
    // Remove all child items under local root.
    while (local_root_->childCount() > 0)
        delete local_root_->child(0);

    // Reload from database.
    loadGroups(0, local_root_);
    local_root_->setExpanded(true);

    // Find and select the requested group.
    QTreeWidgetItem* selected = nullptr;
    if (selected_group_id != 0)
        selected = findGroupItem(selected_group_id, local_root_);

    if (!selected)
        selected = local_root_;

    // Expand parents up to the selected item.
    for (QTreeWidgetItem* p = selected->parent(); p; p = p->parent())
        p->setExpanded(true);

    selected->setExpanded(true);
    ui.tree_group->setCurrentItem(selected);

    current_group_id_ = static_cast<GroupTreeItem*>(selected)->groupId();
    local_group_widget_->showGroup(current_group_id_);
    updateActionsState();
}

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* ComputersTab::findGroupItem(qint64 group_id, QTreeWidgetItem* parent) const
{
    for (int i = 0; i < parent->childCount(); ++i)
    {
        QTreeWidgetItem* child = parent->child(i);
        if (static_cast<GroupTreeItem*>(child)->groupId() == group_id)
            return child;

        QTreeWidgetItem* found = findGroupItem(group_id, child);
        if (found)
            return found;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::switchContent(ContentWidget* new_widget)
{
    if (!new_widget || new_widget == current_content_)
        return;

    current_content_ = new_widget;
    ui.content_stack->setCurrentWidget(new_widget);
}

//--------------------------------------------------------------------------------------------------
void ComputersTab::updateActionsState()
{
    GroupTreeItem* group_item = static_cast<GroupTreeItem*>(ui.tree_group->currentItem());

    if (group_item && group_item->itemType() == GroupTreeItem::Type::LOCAL_GROUP)
    {
        action_add_group_->setVisible(true);
        action_delete_group_->setVisible(group_item->groupId() != 0);
        action_edit_group_->setVisible(true);

        LocalComputerItem* computer_item = local_group_widget_->currentComputer();

        action_add_computer_->setVisible(true);
        action_delete_computer_->setVisible(computer_item != nullptr);
        action_edit_computer_->setVisible(computer_item != nullptr);
        action_copy_computer_->setVisible(computer_item != nullptr);
    }
    else
    {
        action_add_group_->setVisible(false);
        action_delete_group_->setVisible(false);
        action_edit_group_->setVisible(false);

        action_add_computer_->setVisible(false);
        action_delete_computer_->setVisible(false);
        action_edit_computer_->setVisible(false);
        action_copy_computer_->setVisible(false);
    }

    if (group_item && group_item->itemType() == GroupTreeItem::Type::ROUTER)
    {
        action_desktop_->setVisible(false);
        action_file_transfer_->setVisible(false);
        action_chat_->setVisible(false);
        action_system_info_->setVisible(false);
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
proto::peer::SessionType ComputersTab::defaultSessionType() const
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

} // namespace client
