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
#include <QMessageBox>
#include <QStatusBar>

#include "base/logging.h"
#include "base/net/address.h"
#include "build/build_config.h"
#include "client/local_database.h"
#include "client/ui/settings.h"
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

    // Create content widgets.
    local_group_widget_ = new LocalGroupWidget(this);
    router_widget_ = new RouterWidget(this);
    router_group_widget_ = new RouterGroupWidget(this);
    search_widget_ = new SearchWidget(this);

    ui.content_stack->addWidget(local_group_widget_);
    ui.content_stack->addWidget(router_widget_);
    ui.content_stack->addWidget(router_group_widget_);
    ui.content_stack->addWidget(search_widget_);

    // Connect signals.
    connect(ui.sidebar, &Sidebar::sig_switchContent, this, &HostsTab::onSwitchContent);
    connect(ui.sidebar, &Sidebar::sig_contextMenu, this, &HostsTab::onSidebarContextMenu);
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
    addActions(ActionGroup::EDIT, { action_add_group_, action_edit_group_, action_delete_group_ });
    addActions(ActionGroup::EDIT, { action_add_computer_, action_edit_computer_, action_copy_computer_, action_delete_computer_ });
    addActions(ActionGroup::SESSION_TYPE, { action_desktop_, action_file_transfer_, action_chat_, action_system_info_ });

    local_group_widget_->showGroup(ui.sidebar->currentGroupId());
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
        QMessageBox::warning(this, tr("Warning"), tr("Failed to retrieve computer information from the local database."));
        return;
    }

    computer->name += " " + tr("(copy)");

    if (!db.addComputer(*computer))
    {
        QMessageBox::warning(this, tr("Warning"), tr("Failed to add the computer to the local database."));
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

    QMessageBox messagebox(QMessageBox::Question, tr("Confirmation"), message,
                           QMessageBox::Yes | QMessageBox::No, this);
    messagebox.button(QMessageBox::Yes)->setText(tr("Yes"));
    messagebox.button(QMessageBox::No)->setText(tr("No"));

    if (messagebox.exec() == QMessageBox::No)
    {
        LOG(INFO) << "Action is rejected by user";
        return;
    }

    qint64 parent_id = local_group->parentId();

    if (!LocalDatabase::instance().removeGroup(local_group->groupId()))
    {
        QMessageBox::warning(this, tr("Warning"), tr("Unable to remove group"));
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
        menu.addAction(action_edit_group_);
        menu.addAction(action_delete_group_);
        menu.addSeparator();
        menu.addAction(action_add_group_);
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
void HostsTab::onLocalConnect(qint64 computer_id)
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
    Sidebar::Item* sidebar_item = ui.sidebar->currentItem();

    if (sidebar_item && sidebar_item->itemType() == Sidebar::Item::Type::LOCAL_GROUP)
    {
        action_add_group_->setVisible(true);
        action_delete_group_->setVisible(sidebar_item->groupId() != 0);
        action_edit_group_->setVisible(true);

        LocalGroupWidget::Item* computer_item = local_group_widget_->currentItem();

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

    if (sidebar_item && sidebar_item->itemType() == Sidebar::Item::ROUTER)
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

} // namespace client
