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
#include "client/ui/hosts/local_computer_dialog.h"
#include "client/ui/hosts/local_group_widget.h"
#include "client/ui/hosts/router_widget.h"
#include "client/ui/hosts/router_group_widget.h"
#include "client/ui/hosts/search_widget.h"
#include "proto/peer.h"
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

    // Setup drag-and-drop: pass the computer mime type from LocalGroupWidget to Sidebar.
    ui->sidebar->setComputerMimeType(local_group_widget_->mimeType());

    // Connect signals.
    connect(ui->sidebar, &Sidebar::sig_switchContent, this, &HostsTab::onSwitchContent);
    connect(ui->sidebar, &Sidebar::sig_contextMenu, this, &HostsTab::onSidebarContextMenu);
    connect(ui->sidebar, &Sidebar::sig_itemDropped, this, [this]()
    {
        local_group_widget_->showGroup(ui->sidebar->currentGroupId());
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
    connect(ui->action_add_computer, &QAction::triggered, this, &HostsTab::onAddComputer);
    connect(ui->action_edit_computer, &QAction::triggered, this, &HostsTab::onEditComputer);
    connect(ui->action_copy_computer, &QAction::triggered, this, &HostsTab::onCopyComputer);
    connect(ui->action_delete_computer, &QAction::triggered, this, &HostsTab::onRemoveComputer);
    connect(search_widget_, &SearchWidget::sig_currentChanged, this, &HostsTab::onCurrentComputerChanged);
    connect(search_widget_, &SearchWidget::sig_doubleClicked, this, &HostsTab::onLocalConnect);
    connect(search_widget_, &SearchWidget::sig_contextMenu, this, &HostsTab::onLocalComputerContextMenu);
    connect(ui->action_add_group, &QAction::triggered, ui->sidebar, &Sidebar::onAddGroup);
    connect(ui->action_edit_group, &QAction::triggered, ui->sidebar, &Sidebar::onEditGroup);
    connect(ui->action_delete_group, &QAction::triggered, ui->sidebar, &Sidebar::onRemoveGroup);
    connect(ui->action_add_router, &QAction::triggered, ui->sidebar, &Sidebar::onAddRouter);
    connect(ui->action_edit_router, &QAction::triggered, ui->sidebar, &Sidebar::onEditRouter);
    connect(ui->action_delete_router, &QAction::triggered, ui->sidebar, &Sidebar::onRemoveRouter);
    connect(ui->action_router_status, &QAction::triggered, this, &HostsTab::onRouterStatus);
    connect(ui->sidebar, &Sidebar::sig_routersChanged, this, &HostsTab::reloadRouters);
    connect(ui->action_add_user, &QAction::triggered, this, &HostsTab::onAddUserAction);
    connect(ui->action_edit_user, &QAction::triggered, this, &HostsTab::onEditUserAction);
    connect(ui->action_delete_user, &QAction::triggered, this, &HostsTab::onDeleteUserAction);
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
    addActions(ActionRole::EDIT, { ui->action_add_router, ui->action_edit_router, ui->action_delete_router, ui->action_router_status });
    addActions(ActionRole::EDIT, { ui->action_add_group, ui->action_edit_group, ui->action_delete_group });
    addActions(ActionRole::EDIT, { ui->action_add_computer, ui->action_edit_computer, ui->action_copy_computer, ui->action_delete_computer });
    addActions(ActionRole::EDIT, { ui->action_host_remove, ui->action_disconnect, ui->action_disconnect_all });
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
bool HostsTab::hasSearchField() const
{
    return true;
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
            if (router.router_id == id)
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

        auto it = router_widgets_.find(router.router_id);
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
    Sidebar::Router* router = ui->sidebar->routerById(router_id);
    if (!router)
        return;

    switch (status)
    {
        case Router::Status::OFFLINE:
            router->setStatus(Sidebar::Router::Status::OFFLINE);
            break;
        case Router::Status::CONNECTING:
            router->setStatus(Sidebar::Router::Status::CONNECTING);
            break;
        case Router::Status::ONLINE:
            router->setStatus(Sidebar::Router::Status::ONLINE);
            break;
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

            Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
            RouterWidget* widget = router_widgets_.value(router->routerId());
            if (widget)
                switchContent(widget);
        }
        break;

        case Sidebar::Item::Type::ROUTER_GROUP:
        {
            switchContent(router_group_widget_);
            router_group_widget_->showGroup(ui->sidebar->currentGroupId());
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
        menu.addAction(ui->action_add_computer);
    }
    else if (type == Sidebar::Item::Type::ROUTER_GROUP)
    {
        // TODO
        return;
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

    menu.exec(pos);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onCurrentComputerChanged(qint64 /* computer_id */)
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

    ComputerConfig computer;
    qint64 source_computer_id = -1;

    if (current_content_ == local_group_widget_ || current_content_ == search_widget_)
    {
        qint64 computer_id = -1;
        if (current_content_ == local_group_widget_)
        {
            LocalGroupWidget::Item* item = local_group_widget_->currentItem();
            if (!item)
                return;
            computer_id = item->computerId();
        }
        else
        {
            SearchWidget::Item* item = search_widget_->currentItem();
            if (!item)
                return;
            computer_id = item->computerId();
        }

        std::optional<ComputerConfig> found = Database::instance().findComputer(computer_id);
        if (!found.has_value())
        {
            MsgBox::warning(this,
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
        Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
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
        MsgBox::warning(this, tr("Failed to retrieve computer information from the local database."));
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
        menu.addAction(ui->action_desktop_connect);
        menu.addAction(ui->action_file_transfer_connect);
        menu.addAction(ui->action_chat_connect);
        menu.addAction(ui->action_system_info_connect);
        menu.addSeparator();
        menu.addAction(ui->action_edit_computer);
        menu.addAction(ui->action_copy_computer);
        menu.addAction(ui->action_delete_computer);
    }
    else
    {
        menu.addAction(ui->action_add_computer);
    }

    menu.exec(pos);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onAddComputer()
{
    LOG(INFO) << "[ACTION] Add computer";

    qint64 group_id = ui->sidebar->currentGroupId();
    if (group_id < 0)
    {
        LOG(INFO) << "No current group";
        return;
    }

    LocalComputerDialog dialog(-1, group_id, this);
    if (dialog.exec() == LocalComputerDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    qint64 new_id = dialog.computerId();
    local_group_widget_->showGroup(group_id);
    local_group_widget_->setCurrentComputer(new_id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onEditComputer()
{
    LOG(INFO) << "[ACTION] Edit computer";

    qint64 computer_id = currentComputerId();
    if (computer_id <= 0)
    {
        LOG(INFO) << "No current computer";
        return;
    }

    std::optional<ComputerConfig> computer = Database::instance().findComputer(computer_id);
    if (!computer.has_value())
    {
        MsgBox::warning(this, tr("Failed to retrieve computer information from the local database."));
        return;
    }

    LocalComputerDialog dialog(computer_id, computer->group_id, this);
    if (dialog.exec() == LocalComputerDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    refreshItem(computer_id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onCopyComputer()
{
    LOG(INFO) << "[ACTION] Copy computer";

    qint64 computer_id = currentComputerId();
    if (computer_id <= 0)
    {
        LOG(INFO) << "No current computer";
        return;
    }

    Database& db = Database::instance();

    std::optional<ComputerConfig> computer = db.findComputer(computer_id);
    if (!computer.has_value())
    {
        MsgBox::warning(this, tr("Failed to retrieve computer information from the local database."));
        return;
    }

    computer->name += " " + tr("(copy)");

    if (!db.addComputer(*computer))
    {
        MsgBox::warning(this, tr("Failed to add the computer to the local database."));
        return;
    }

    qint64 new_id = computer->id;

    LocalComputerDialog(new_id, computer->group_id, this).exec();

    if (current_content_ == search_widget_)
    {
        search_widget_->search(search_widget_->currentQuery());
        search_widget_->setCurrentComputer(new_id);
    }
    else
    {
        local_group_widget_->showGroup(local_group_widget_->currentGroupId());
        local_group_widget_->setCurrentComputer(new_id);
    }
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onRemoveComputer()
{
    LOG(INFO) << "[ACTION] Delete computer";

    qint64 computer_id = currentComputerId();
    if (computer_id <= 0)
    {
        LOG(INFO) << "No current computer";
        return;
    }

    std::optional<ComputerConfig> computer = Database::instance().findComputer(computer_id);
    if (!computer.has_value())
    {
        MsgBox::warning(this, tr("Failed to retrieve computer information from the local database."));
        return;
    }

    QString message = tr("Are you sure you want to delete computer \"%1\"?").arg(computer->name);

    if (MsgBox::question(this, message) == MsgBox::No)
    {
        LOG(INFO) << "Action is rejected by user";
        return;
    }

    if (!Database::instance().removeComputer(computer_id))
    {
        MsgBox::warning(this, tr("Unable to remove computer"));
        LOG(INFO) << "Unable to remove computer with id" << computer_id;
        return;
    }

    removeItem(computer_id);
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
void HostsTab::onAddUserAction()
{
    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
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
    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
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
    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
    RouterWidget* widget = router_widgets_.value(router->routerId());
    if (widget)
        widget->onDeleteUser();
}

//--------------------------------------------------------------------------------------------------
void HostsTab::onRouterStatus()
{
    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();
    if (!sidebar_item || sidebar_item->itemType() != Sidebar::Item::ROUTER)
        return;

    Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
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

    Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
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

    Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
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

    Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
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

    ui->action_add_computer->setVisible(false);
    ui->action_delete_computer->setVisible(false);
    ui->action_edit_computer->setVisible(false);
    ui->action_copy_computer->setVisible(false);

    ui->action_desktop->setVisible(false);
    ui->action_file_transfer->setVisible(false);
    ui->action_chat->setVisible(false);
    ui->action_system_info->setVisible(false);

    ui->action_add_user->setVisible(false);
    ui->action_edit_user->setVisible(false);
    ui->action_delete_user->setVisible(false);

    ui->action_reload->setVisible(false);
    ui->action_save->setVisible(false);
    ui->action_import_old_book->setVisible(false);
    ui->action_export_book->setVisible(false);
    ui->action_import_book->setVisible(false);
    ui->action_disconnect->setVisible(false);
    ui->action_disconnect_all->setVisible(false);
    ui->action_host_remove->setVisible(false);
    ui->action_online_check->setVisible(false);

    Sidebar::Item* sidebar_item = ui->sidebar->currentItem();

    if (current_content_ == search_widget_)
    {
        SearchWidget::Item* computer_item = search_widget_->currentItem();

        ui->action_delete_computer->setVisible(computer_item != nullptr);
        ui->action_edit_computer->setVisible(computer_item != nullptr);
        ui->action_copy_computer->setVisible(computer_item != nullptr);
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

        LocalGroupWidget::Item* computer_item = local_group_widget_->currentItem();

        ui->action_add_computer->setVisible(true);
        ui->action_delete_computer->setVisible(computer_item != nullptr);
        ui->action_edit_computer->setVisible(computer_item != nullptr);
        ui->action_copy_computer->setVisible(computer_item != nullptr);
    }

    if (sidebar_item && sidebar_item->itemType() == Sidebar::Item::ROUTER)
    {
        ui->action_edit_router->setVisible(true);
        ui->action_delete_router->setVisible(true);
        ui->action_router_status->setVisible(true);

        Sidebar::Router* router = static_cast<Sidebar::Router*>(sidebar_item);
        RouterWidget* widget = router_widgets_.value(router->routerId());

        bool on_users_tab = widget && widget->currentTabType() == RouterWidget::TabType::USERS;
        bool has_selection = on_users_tab && widget->hasSelectedUser();

        ui->action_add_user->setVisible(on_users_tab);
        ui->action_edit_user->setVisible(has_selection);
        ui->action_delete_user->setVisible(has_selection);

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
    }
    else
    {
        ui->action_desktop->setVisible(true);
        ui->action_file_transfer->setVisible(true);
        ui->action_chat->setVisible(true);
        ui->action_system_info->setVisible(true);
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

    router_widgets_.insert(config.router_id, widget);
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
    connect(widget, &RouterWidget::sig_userContextMenu, this, &HostsTab::onUserContextMenu);
    connect(widget, &RouterWidget::sig_hostContextMenu, this, &HostsTab::onHostContextMenu);
    connect(widget, &RouterWidget::sig_clientContextMenu, this, &HostsTab::onClientContextMenu);
    connect(widget, &RouterWidget::sig_relayContextMenu, this, &HostsTab::onRelayContextMenu);

    widget->connectToRouter();
    return widget;
}

//--------------------------------------------------------------------------------------------------
bool HostsTab::validateComputerForConnect(const ComputerConfig& computer)
{
    if (computer.router_id != 0)
    {
        std::optional<RouterConfig> router = Database::instance().findRouter(computer.router_id);
        if (!router.has_value())
        {
            MsgBox::warning(this, tr("The router associated with this computer has been deleted. "
                "Edit the computer to select another router or switch to direct connection."));
            return false;
        }

        if (!isHostId(computer.address))
        {
            MsgBox::warning(this, tr("The computer has an invalid host ID."));
            return false;
        }
    }
    else
    {
        Address address = Address::fromString(computer.address, DEFAULT_HOST_TCP_PORT);
        if (!address.isValid())
        {
            MsgBox::warning(this, tr("The computer has an incorrect address."));
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
qint64 HostsTab::currentComputerId() const
{
    if (current_content_ == local_group_widget_)
    {
        LocalGroupWidget::Item* item = local_group_widget_->currentItem();
        return item ? item->computerId() : -1;
    }

    if (current_content_ == search_widget_)
    {
        SearchWidget::Item* item = search_widget_->currentItem();
        return item ? item->computerId() : -1;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
void HostsTab::refreshItem(qint64 computer_id)
{
    if (current_content_ == local_group_widget_)
        local_group_widget_->refreshItem(computer_id);
    else if (current_content_ == search_widget_)
        search_widget_->refreshItem(computer_id);
}

//--------------------------------------------------------------------------------------------------
void HostsTab::removeItem(qint64 computer_id)
{
    if (current_content_ == local_group_widget_)
        local_group_widget_->removeItem(computer_id);
    else if (current_content_ == search_widget_)
        search_widget_->removeItem(computer_id);
}
