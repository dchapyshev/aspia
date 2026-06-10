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

#include "client/desktop/hosts/sidebar.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSet>
#include <QDateTime>
#include <QUuid>
#include <QVBoxLayout>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "base/net/tcp_channel.h"
#include "base/peer/user.h"
#include "client/config.h"
#include "client/database.h"
#include "client/settings.h"
#include "client/desktop/router_dialog.h"
#include "client/desktop/hosts/local_group_dialog.h"
#include "client/desktop/hosts/local_group_widget.h"
#include "common/desktop/credentials_dialog.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/two_factor_code_dialog.h"
#include "common/desktop/two_factor_enroll_dialog.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"

//--------------------------------------------------------------------------------------------------
Sidebar::Sidebar(QWidget* parent)
    : QWidget(parent),
      local_group_mime_type_(QString("application/%1").arg(QUuid::createUuid().toString())),
      router_group_mime_type_(QString("application/%1").arg(QUuid::createUuid().toString()))
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tree_widget_ = new QTreeWidget(this);
    tree_widget_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_widget_->setSelectionBehavior(QAbstractItemView::SelectItems);
    tree_widget_->header()->setVisible(false);
    tree_widget_->setColumnCount(1);

    layout->addWidget(tree_widget_);

    loadRouters();

    GroupConfig local_root_data;
    local_root_data.setId(0);
    local_root_data.setParentId(0);
    local_root_data.setName(tr("Local"));

    // Create local root after routers so it appears at the bottom.
    local_root_ = new SidebarLocalGroup(local_root_data, tree_widget_);
    local_root_->setExpanded(Settings().isLocalGroupExpanded(local_root_data.id()));

    // Setup drag-and-drop.
    tree_widget_->setAcceptDrops(true);
    tree_widget_->viewport()->installEventFilter(this);
    tree_widget_->installEventFilter(this);

    QTreeWidgetItem* invisible_root = tree_widget_->invisibleRootItem();
    invisible_root->setFlags(invisible_root->flags() ^ Qt::ItemIsDropEnabled);

    connect(tree_widget_, &QTreeWidget::currentItemChanged, this, &Sidebar::onCurrentItemChanged);
    connect(tree_widget_, &QTreeWidget::customContextMenuRequested, this, &Sidebar::onContextMenu);
    connect(tree_widget_, &QTreeWidget::itemExpanded, this, &Sidebar::onItemExpanded);
    connect(tree_widget_, &QTreeWidget::itemCollapsed, this, &Sidebar::onItemCollapsed);

    // Load groups from database under Local root.
    loadGroups(0, local_root_);
    tree_widget_->setCurrentItem(local_root_);
}

//--------------------------------------------------------------------------------------------------
Sidebar::~Sidebar() = default;

//--------------------------------------------------------------------------------------------------
void Sidebar::setLocalHostMimeType(const QString& mime_type)
{
    local_host_mime_type_ = mime_type;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::setRouterHostMimeType(const QString& mime_type)
{
    router_host_mime_type_ = mime_type;
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::dragging() const
{
    return dragging_;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::loadGroups(qint64 parent_id, QTreeWidgetItem* parent_item)
{
    QList<GroupConfig> groups = Database::instance().groupList(parent_id);

    Settings settings;

    for (const GroupConfig& group : std::as_const(groups))
    {
        SidebarLocalGroup* item = new SidebarLocalGroup(group, parent_item);
        item->setExpanded(settings.isLocalGroupExpanded(group.id()));

        // Load child groups recursively.
        loadGroups(group.id(), item);
    }
}

//--------------------------------------------------------------------------------------------------
void Sidebar::reloadGroups(qint64 selected_group_id)
{
    // Remove all child items under local root.
    while (local_root_->childCount() > 0)
        delete local_root_->child(0);

    // Reload from database.
    loadGroups(0, local_root_);
    local_root_->setExpanded(Settings().isLocalGroupExpanded(0));

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
    tree_widget_->setCurrentItem(selected);

    current_group_id_ = static_cast<SidebarItem*>(selected)->groupId();
    emit sig_switchContent(SidebarItem::LOCAL_GROUP);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::loadRouters()
{
    QList<RouterConfig> routers = Database::instance().routerList();

    for (const RouterConfig& router_config : std::as_const(routers))
    {
        SidebarRouter* router = new SidebarRouter(router_config.routerId(), router_config.displayLabel(), tree_widget_);
        router->setExpanded(true);

        if (router_config.isValid())
            createRouterSession(router_config);
    }
}

//--------------------------------------------------------------------------------------------------
void Sidebar::reloadRouters()
{
    QList<RouterConfig> routers = Database::instance().routerList();

    QSet<qint64> new_ids;
    new_ids.reserve(routers.size());
    for (const RouterConfig& router_data : std::as_const(routers))
        new_ids.insert(router_data.routerId());

    // Remove only Router items whose id no longer exists.
    for (int i = tree_widget_->topLevelItemCount() - 1; i >= 0; --i)
    {
        SidebarItem* item = static_cast<SidebarItem*>(tree_widget_->topLevelItem(i));
        if (item->itemType() != SidebarItem::ROUTER)
            continue;

        const qint64 router_id = static_cast<SidebarRouter*>(item)->routerId();
        if (!new_ids.contains(router_id))
        {
            destroyRouterSession(router_id);
            delete tree_widget_->takeTopLevelItem(i);
        }
    }

    // Update existing routers, append new ones at the end. Child items (Unassigned, ...) are
    // managed by setRouterStatus() based on the current connection state, not here.
    for (const RouterConfig& router_config : std::as_const(routers))
    {
        const QString name = router_config.displayLabel();

        SidebarRouter* router = routerById(router_config.routerId());
        if (router)
        {
            router->setName(name);

            if (Router* session = routers_.value(router_config.routerId()))
                session->updateConfig(router_config);
        }
        else
        {
            SidebarRouter* new_router = new SidebarRouter(router_config.routerId(), name, tree_widget_);
            new_router->setExpanded(true);

            if (router_config.isValid())
                createRouterSession(router_config);

            // Keep ordering: routers first, local root last.
            int local_idx = tree_widget_->indexOfTopLevelItem(local_root_);
            int router_idx = tree_widget_->indexOfTopLevelItem(new_router);
            if (router_idx > local_idx)
            {
                tree_widget_->takeTopLevelItem(router_idx);
                tree_widget_->insertTopLevelItem(local_idx, new_router);
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Sidebar::setRouterStatus(qint64 router_id, SidebarRouter::Status status)
{
    SidebarRouter* router = routerById(router_id);
    if (!router)
        return;

    router->setStatus(status);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::setRouterWorkspaces(qint64 router_id, const QList<Router::Workspace>& workspaces)
{
    SidebarRouter* router = routerById(router_id);
    if (!router)
        return;

    // Incremental diff with the existing workspace items. Keeping items in place preserves
    // selection, expansion state and the host-group subtrees across refreshes.
    QSet<qint64> incoming_ids;
    incoming_ids.reserve(workspaces.size());
    for (const Router::Workspace& workspace : workspaces)
        incoming_ids.insert(workspace.entry_id);

    // Iterate in reverse so takeChild() index shifts do not skip siblings.
    QHash<qint64, SidebarRouterWorkspace*> existing;
    for (int i = router->childCount() - 1; i >= 0; --i)
    {
        SidebarItem* child = static_cast<SidebarItem*>(router->child(i));
        if (child->itemType() != SidebarItem::ROUTER_WORKSPACE)
            continue;

        auto* workspace_item = static_cast<SidebarRouterWorkspace*>(child);
        if (!incoming_ids.contains(workspace_item->workspaceId()))
        {
            delete router->takeChild(i);
            continue;
        }
        existing.insert(workspace_item->workspaceId(), workspace_item);
    }

    Settings settings;
    for (const Router::Workspace& workspace : workspaces)
    {
        SidebarRouterWorkspace* item = existing.value(workspace.entry_id);
        if (!item)
        {
            item = new SidebarRouterWorkspace(router_id, workspace, router);
            item->setExpanded(settings.isWorkspaceExpanded(router_id, workspace.entry_id));
        }
        else
        {
            item->update(workspace);
        }
    }

    router->setExpanded(true);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::setRouterHostGroups(qint64 router_id, qint64 workspace_id, const QList<Router::Group>& groups)
{
    SidebarRouter* router = routerById(router_id);
    if (!router)
        return;

    // Find the workspace item under the router by its id.
    SidebarRouterWorkspace* workspace_item = nullptr;
    for (int i = 0; i < router->childCount(); ++i)
    {
        SidebarItem* child = static_cast<SidebarItem*>(router->child(i));
        if (child->itemType() != SidebarItem::ROUTER_WORKSPACE)
            continue;
        auto* candidate = static_cast<SidebarRouterWorkspace*>(child);
        if (candidate->workspaceId() == workspace_id)
        {
            workspace_item = candidate;
            break;
        }
    }

    if (!workspace_item)
        return;

    // Incremental diff with the existing subtree under the workspace. Keeping items in place
    // preserves selection and expansion state across refreshes, and avoids visible flicker.
    //
    // Three passes:
    //   1. Index the incoming list: a flat set of entry_ids for the deletion check, and a
    //      parent_id -> children index for the DFS in pass 3.
    //   2. Walk the existing subtree once. Items whose id is missing from the incoming set are
    //      dropped (their descendants go with them via Qt's child ownership); surviving items
    //      land in `existing` and stay where they are.
    //   3. DFS the incoming tree in parent-first order. For each node create it under its
    //      target parent if missing; otherwise update name and re-parent if it moved.

    QHash<qint64, QList<const Router::Group*>> children_of;
    QSet<qint64> incoming_ids;
    incoming_ids.reserve(groups.size());
    for (const Router::Group& group : groups)
    {
        children_of[group.parent_id].append(&group);
        incoming_ids.insert(group.entry_id);
    }

    QHash<qint64, SidebarRouterGroup*> existing;
    std::function<void(QTreeWidgetItem*)> sweep = [&](QTreeWidgetItem* parent)
    {
        // Iterate in reverse so takeChild() index shifts do not skip siblings.
        for (int i = parent->childCount() - 1; i >= 0; --i)
        {
            SidebarItem* child = static_cast<SidebarItem*>(parent->child(i));
            if (child->itemType() != SidebarItem::ROUTER_GROUP)
                continue;
            auto* group_item = static_cast<SidebarRouterGroup*>(child);
            if (!incoming_ids.contains(group_item->groupId()))
            {
                delete parent->takeChild(i);
                continue;
            }
            existing.insert(group_item->groupId(), group_item);
            sweep(group_item);
        }
    };
    sweep(workspace_item);

    Settings settings;
    std::function<void(qint64, QTreeWidgetItem*)> apply = [&](qint64 parent_id,
                                                              QTreeWidgetItem* tree_parent)
    {
        for (const Router::Group* group : std::as_const(children_of[parent_id]))
        {
            SidebarRouterGroup* item = existing.value(group->entry_id);
            if (!item)
            {
                item = new SidebarRouterGroup(router_id, *group, tree_parent);
                item->setExpanded(settings.isRouterGroupExpanded(router_id, group->entry_id));
                existing.insert(group->entry_id, item);
            }
            else
            {
                item->update(*group);
                if (item->parent() != tree_parent)
                {
                    QTreeWidgetItem* current_parent = item->parent();
                    current_parent->takeChild(current_parent->indexOfChild(item));
                    tree_parent->addChild(item);
                }
            }
            apply(group->entry_id, item);
        }
    };
    apply(0, workspace_item);
}

//--------------------------------------------------------------------------------------------------
qint64 Sidebar::currentGroupId() const
{
    return current_group_id_;
}

//--------------------------------------------------------------------------------------------------
SidebarItem* Sidebar::currentItem() const
{
    return static_cast<SidebarItem*>(tree_widget_->currentItem());
}

//--------------------------------------------------------------------------------------------------
SidebarRouter* Sidebar::routerById(qint64 router_id) const
{
    for (int i = 0; i < tree_widget_->topLevelItemCount(); ++i)
    {
        SidebarItem* item = static_cast<SidebarItem*>(tree_widget_->topLevelItem(i));
        if (item->itemType() != SidebarItem::ROUTER)
            continue;

        SidebarRouter* router = static_cast<SidebarRouter*>(item);
        if (router->routerId() == router_id)
            return router;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
QList<qint64> Sidebar::routerIds() const
{
    QList<qint64> ids;

    for (int i = 0; i < tree_widget_->topLevelItemCount(); ++i)
    {
        SidebarItem* item = static_cast<SidebarItem*>(tree_widget_->topLevelItem(i));
        if (item->itemType() != SidebarItem::ROUTER)
            continue;

        ids.append(static_cast<SidebarRouter*>(item)->routerId());
    }

    return ids;
}

//--------------------------------------------------------------------------------------------------
QList<qint64> Sidebar::routerWorkspaceIds(qint64 router_id) const
{
    QList<qint64> ids;

    SidebarRouter* router = routerById(router_id);
    if (!router)
        return ids;

    for (int i = 0; i < router->childCount(); ++i)
    {
        SidebarItem* child = static_cast<SidebarItem*>(router->child(i));
        if (child->itemType() == SidebarItem::ROUTER_WORKSPACE)
            ids.append(static_cast<SidebarRouterWorkspace*>(child)->workspaceId());
    }

    return ids;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::changeRouterPassword(qint64 router_id)
{
    Router* router = routers_.value(router_id);
    if (!router)
        return;

    CredentialsDialog dialog(CredentialsDialog::Type::SET_PASSWORD, this);
    dialog.setWindowTitle(tr("Change Password"));
    dialog.setValidator([](CredentialsDialog* dialog) -> bool
    {
        SecureString password = dialog->password();

        if (!User::isValidPassword(password))
        {
            MsgBox::warning(dialog, tr("Password can not be empty and should not exceed %n characters.",
                "", User::kMaxPasswordLength));
            return false;
        }

        if (!User::isSafePassword(password))
        {
            QString unsafe = tr("Password you entered does not meet the security requirements!");
            QString safe = tr("The password must contain lowercase and uppercase characters, "
                "numbers and should not be shorter than %n characters.",
                "", User::kSafePasswordLength);
            QString question = tr("Do you want to enter a different password?");

            if (MsgBox::warning(dialog, QString("<b>%1</b><br/>%2<br/>%3").arg(unsafe, safe, question),
                MsgBox::Yes | MsgBox::No) == MsgBox::Yes)
                return false;
        }

        return true;
    });

    if (dialog.exec() != QDialog::Accepted)
        return;

    // On success the router revokes this session's token and re-runs the 2FA stage, so the user
    // will be asked for a code again right after (handled by the existing two-factor plumbing).
    router->changePassword(dialog.password(), this,
        [this, router_id](const proto::router::ChangePasswordResult& result)
    {
        const std::string& error_code = result.error_code();
        if (error_code == proto::router::kErrorOk)
        {
            addRouterEvent(Severity::INFO, router_id,
                           tr("Password updated. Waiting for new encryption keys..."));
            return;
        }

        const char* message;
        if (error_code == proto::router::kErrorInvalidRequest)
            message = QT_TR_NOOP("Invalid password change request.");
        else if (error_code == proto::router::kErrorInternalError)
            message = QT_TR_NOOP("Unknown internal error.");
        else if (error_code == proto::router::kErrorInvalidData)
            message = QT_TR_NOOP("Invalid data was passed.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        MsgBox::warning(this, tr(message));
    });
}

//--------------------------------------------------------------------------------------------------
QList<RouterStatusWidget::Event> Sidebar::routerEvents(qint64 router_id) const
{
    return router_events_.value(router_id);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onRefreshWorkspaces(qint64 router_id)
{
    Router* router = routers_.value(router_id);
    if (!router)
        return;

    router->listWorkspaces(0, this, [this, router_id](const Router::WorkspaceList& list)
    {
        setRouterWorkspaces(router_id, list.workspaces);

        // Once the workspace items are in place, fan out a host-group fetch per workspace.
        // Each response builds the subtree under its workspace via setRouterHostGroups().
        Router* router = routers_.value(router_id);
        if (!router)
            return;

        for (const Router::Workspace& workspace : list.workspaces)
        {
            const qint64 workspace_id = workspace.entry_id;
            router->listGroups(workspace_id, this,
                [this, router_id, workspace_id](const Router::GroupList& result)
            {
                setRouterHostGroups(router_id, workspace_id, result.groups);
            });
        }
    });
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onRefreshHostGroups(qint64 router_id)
{
    // The workspace items already exist in the sidebar; refetch the group tree for each one
    // without disturbing the workspaces themselves.
    Router* router = routers_.value(router_id);
    if (!router)
        return;

    const QList<qint64> workspace_ids = routerWorkspaceIds(router_id);
    for (qint64 workspace_id : std::as_const(workspace_ids))
    {
        router->listGroups(workspace_id, this,
            [this, router_id, workspace_id](const Router::GroupList& result)
        {
            setRouterHostGroups(router_id, workspace_id, result.groups);
        });
    }
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onAddGroup()
{
    LOG(INFO) << "[ACTION] Add group";

    SidebarItem* item = currentItem();
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
void Sidebar::onEditGroup()
{
    LOG(INFO) << "[ACTION] Edit group";

    SidebarItem* item = currentItem();
    if (!item)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    if (item->itemType() != SidebarItem::LOCAL_GROUP)
        return;

    SidebarLocalGroup* local_group = static_cast<SidebarLocalGroup*>(item);
    if (local_group->groupId() == 0)
        return;

    LocalGroupDialog dialog(local_group->groupId(), local_group->parentId(), this);
    if (dialog.exec() == LocalGroupDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    reloadGroups(item->groupId());
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onRemoveGroup()
{
    LOG(INFO) << "[ACTION] Delete group";

    SidebarItem* item = currentItem();
    if (!item)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    if (item->itemType() != SidebarItem::LOCAL_GROUP || item->groupId() == 0) // Root group.
        return;

    SidebarLocalGroup* local_group = static_cast<SidebarLocalGroup*>(item);

    QString message = tr("Are you sure you want to delete group \"%1\"?").arg(local_group->groupName());

    if (MsgBox::question(this, message) == MsgBox::No)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    qint64 parent_id = local_group->parentId();
    qint64 group_id = local_group->groupId();

    if (!Database::instance().removeGroup(group_id))
    {
        MsgBox::warning(this, tr("Unable to remove group"));
        LOG(INFO) << "Unable to remove group with id" << group_id;
        return;
    }

    Settings().removeLocalGroupExpanded(group_id);

    reloadGroups(parent_id);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onAddRouter()
{
    LOG(INFO) << "[ACTION] Add router";

    RouterDialog dialog(-1, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    emit sig_routersChanged();
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onEditRouter()
{
    SidebarItem* item = currentItem();
    if (!item || item->itemType() != SidebarItem::ROUTER)
        return;

    qint64 router_id = static_cast<SidebarRouter*>(item)->routerId();
    LOG(INFO) << "[ACTION] Edit router" << router_id;

    RouterDialog dialog(router_id, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    emit sig_routersChanged();
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onRemoveRouter()
{
    SidebarItem* item = currentItem();
    if (!item || item->itemType() != SidebarItem::ROUTER)
        return;

    SidebarRouter* router_item = static_cast<SidebarRouter*>(item);
    qint64 router_id = router_item->routerId();
    LOG(INFO) << "[ACTION] Delete router" << router_id;

    Database& db = Database::instance();
    std::optional<RouterConfig> existing = db.findRouter(router_id);
    if (!existing)
    {
        LOG(ERROR) << "Router not found for id:" << router_id;
        return;
    }

    QString message = tr("Are you sure you want to delete router \"%1\"?").arg(existing->displayName());
    if (MsgBox::question(this, message) == MsgBox::No)
    {
        LOG(INFO) << "Action is rejected by user";
        return;
    }

    db.removeRouter(router_id);
    Settings().removeRouterExpanded(router_id);
    emit sig_routersChanged();
}

//--------------------------------------------------------------------------------------------------
void Sidebar::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    if (event->type() != QEvent::LanguageChange)
        return;

    if (local_root_)
        local_root_->setText(0, tr("Local"));

    for (int i = 0; i < tree_widget_->topLevelItemCount(); ++i)
    {
        SidebarItem* top = static_cast<SidebarItem*>(tree_widget_->topLevelItem(i));
        if (top->itemType() != SidebarItem::ROUTER)
            continue;

        for (int j = 0; j < top->childCount(); ++j)
            static_cast<SidebarItem*>(top->child(j))->retranslate();
    }
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == tree_widget_)
    {
        if (event->type() == QEvent::KeyPress)
        {
            switch (static_cast<QKeyEvent*>(event)->key())
            {
                case Qt::Key_Insert:
                    emit sig_addGroup();
                    return true;

                case Qt::Key_Delete:
                {
                    SidebarItem* item = currentItem();
                    if (item && item->itemType() == SidebarItem::ROUTER)
                        onRemoveRouter();
                    else
                        emit sig_removeGroup();
                    return true;
                }

                case Qt::Key_F2:
                {
                    SidebarItem* item = currentItem();
                    if (item && item->itemType() == SidebarItem::ROUTER)
                        onEditRouter();
                    else
                        emit sig_editGroup();
                    return true;
                }

                default:
                    break;
            }
        }
    }
    else if (watched == tree_widget_->viewport())
    {
        switch (event->type())
        {
            case QEvent::MouseButtonPress:
                return onMousePress(static_cast<QMouseEvent*>(event));

            case QEvent::MouseMove:
                return onMouseMove(static_cast<QMouseEvent*>(event));

            case QEvent::DragEnter:
                return onDragEnter(static_cast<QDragEnterEvent*>(event));

            case QEvent::DragMove:
                return onDragMove(static_cast<QDragMoveEvent*>(event));

            case QEvent::DragLeave:
                return onDragLeave(static_cast<QDragLeaveEvent*>(event));

            case QEvent::Drop:
                return onDrop(static_cast<QDropEvent*>(event));

            default:
                break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    if (!current)
        return;

    if (dragging_)
        return;

    SidebarItem* item = static_cast<SidebarItem*>(current);

    switch (item->itemType())
    {
        case SidebarItem::LOCAL_GROUP:
            current_group_id_ = item->groupId();
            break;

        case SidebarItem::ROUTER:
        case SidebarItem::ROUTER_HOSTS:
        case SidebarItem::ROUTER_USERS:
        case SidebarItem::ROUTER_CLIENTS:
        case SidebarItem::ROUTER_RELAYS:
            // Nothing
            break;

        case SidebarItem::ROUTER_WORKSPACE:
        case SidebarItem::ROUTER_GROUP:
            current_group_id_ = item->groupId();
            break;
    }

    emit sig_switchContent(item->itemType());
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onContextMenu(const QPoint& pos)
{
    SidebarItem* item = static_cast<SidebarItem*>(tree_widget_->itemAt(pos));
    if (!item)
        return;

    tree_widget_->setCurrentItem(item);

    emit sig_contextMenu(item->itemType(), tree_widget_->viewport()->mapToGlobal(pos));
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onItemExpanded(QTreeWidgetItem* item)
{
    CHECK(item);

    SidebarItem* sidebar_item = static_cast<SidebarItem*>(item);
    if (sidebar_item->itemType() == SidebarItem::LOCAL_GROUP)
    {
        Settings().setLocalGroupExpanded(sidebar_item->groupId(), true);
    }
    else if (sidebar_item->itemType() == SidebarItem::ROUTER_WORKSPACE)
    {
        auto* workspace_item = static_cast<SidebarRouterWorkspace*>(sidebar_item);
        Settings().setWorkspaceExpanded(workspace_item->routerId(), workspace_item->workspaceId(), true);
    }
    else if (sidebar_item->itemType() == SidebarItem::ROUTER_GROUP)
    {
        auto* group_item = static_cast<SidebarRouterGroup*>(sidebar_item);
        Settings().setRouterGroupExpanded(group_item->routerId(), group_item->groupId(), true);
    }
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onItemCollapsed(QTreeWidgetItem* item)
{
    CHECK(item);

    SidebarItem* sidebar_item = static_cast<SidebarItem*>(item);
    if (sidebar_item->itemType() == SidebarItem::LOCAL_GROUP)
    {
        Settings().setLocalGroupExpanded(sidebar_item->groupId(), false);
    }
    else if (sidebar_item->itemType() == SidebarItem::ROUTER_WORKSPACE)
    {
        auto* workspace_item = static_cast<SidebarRouterWorkspace*>(sidebar_item);
        Settings().setWorkspaceExpanded(workspace_item->routerId(), workspace_item->workspaceId(), false);
    }
    else if (sidebar_item->itemType() == SidebarItem::ROUTER_GROUP)
    {
        auto* group_item = static_cast<SidebarRouterGroup*>(sidebar_item);
        Settings().setRouterGroupExpanded(group_item->routerId(), group_item->groupId(), false);
    }
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onRouterStatusChanged(qint64 router_id, Router::Status status)
{
    if (Router* router = routers_.value(router_id))
    {
        const QString address = router->config().address();
        switch (status)
        {
            case Router::Status::CONNECTING:
                addRouterEvent(Severity::INFO, router_id,
                               tr("Connecting to router %1...").arg(address));
                break;
            case Router::Status::ONLINE:
                addRouterEvent(Severity::INFO, router_id,
                               tr("Connection to router %1 established.").arg(address));
                break;
            case Router::Status::OFFLINE:
                addRouterEvent(Severity::WARNING, router_id,
                               tr("Disconnected from router %1.").arg(address));
                break;
        }
    }

    SidebarRouter::Status sidebar_status = SidebarRouter::Status::OFFLINE;
    switch (status)
    {
        case Router::Status::OFFLINE:    sidebar_status = SidebarRouter::Status::OFFLINE;    break;
        case Router::Status::CONNECTING: sidebar_status = SidebarRouter::Status::CONNECTING; break;
        case Router::Status::ONLINE:     sidebar_status = SidebarRouter::Status::ONLINE;     break;
    }
    setRouterStatus(router_id, sidebar_status);

    if (status == Router::Status::ONLINE)
    {
        buildRouterSections(router_id);
        onRefreshWorkspaces(router_id);
    }
    else
    {
        removeRouterSections(router_id);
        setRouterWorkspaces(router_id, {});
    }
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onRouterErrorOccurred(qint64 router_id, TcpChannel::ErrorCode error_code)
{
    Severity severity = Severity::WARNING;
    if (error_code == TcpChannel::ErrorCode::CRYPTO_ERROR ||
        error_code == TcpChannel::ErrorCode::ACCESS_DENIED)
    {
        severity = Severity::CRITICAL;
    }

    const QString message = tr("Network error: %1.").arg(TcpChannel::errorToString(error_code));
    addRouterEvent(severity, router_id, message);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onRouterPasswordChangeRequired(qint64 router_id)
{
    MsgBox::information(this,
        tr("To complete the migration from a previous version, you need to change your password."));
    changeRouterPassword(router_id);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onRouterTwoFactorCodeRequired(qint64 router_id)
{
    Router* router = routers_.value(router_id);
    if (!router)
        return;

    TwoFactorCodeDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
        router->submitTwoFactorCode(dialog.code());
    else
        router->disconnectFromRouter();
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onRouterTwoFactorEnrollment(qint64 router_id, const QString& otpauth_uri)
{
    Router* router = routers_.value(router_id);
    if (!router)
        return;

    TwoFactorEnrollDialog dialog(otpauth_uri, this);
    if (dialog.exec() == QDialog::Accepted)
        router->submitTwoFactorCode(dialog.code());
    else
        router->disconnectFromRouter();
}

//--------------------------------------------------------------------------------------------------
void Sidebar::createRouterSession(const RouterConfig& config)
{
    const qint64 router_id = config.routerId();
    if (routers_.contains(router_id))
        return;

    router_events_.insert(router_id, {});

    Router* router = new Router(config, this);
    routers_.insert(router_id, router);

    connect(router, &Router::sig_statusChanged, this, &Sidebar::onRouterStatusChanged);
    connect(router, &Router::sig_errorOccurred, this, &Sidebar::onRouterErrorOccurred);
    connect(router, &Router::sig_passwordChangeRequired, this, &Sidebar::onRouterPasswordChangeRequired);
    connect(router, &Router::sig_twoFactorCodeRequired, this, &Sidebar::onRouterTwoFactorCodeRequired);
    connect(router, &Router::sig_twoFactorEnrollment, this, &Sidebar::onRouterTwoFactorEnrollment);
    connect(router, &Router::sig_workspacesChanged, this, &Sidebar::onRefreshWorkspaces);
    connect(router, &Router::sig_groupsChanged, this, &Sidebar::onRefreshHostGroups);

    router->connectToRouter();
}

//--------------------------------------------------------------------------------------------------
void Sidebar::destroyRouterSession(qint64 router_id)
{
    if (Router* router = routers_.take(router_id))
    {
        router->disconnectFromRouter();
        delete router;
    }

    router_events_.remove(router_id);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::buildRouterSections(qint64 router_id)
{
    SidebarRouter* router = routerById(router_id);
    if (!router)
        return;

    // Administrative sections are only meaningful for an administrator session.
    Router* instance = routers_.value(router_id);
    if (!instance || instance->config().sessionType() != proto::router::SESSION_TYPE_ADMIN)
        return;

    removeRouterSections(router_id);

    // Sections live above the workspace subtree. Insert each at the top in reverse display order
    // so the final order stays Hosts -> Clients -> Relays -> Users.
    auto move_to_top = [router](QTreeWidgetItem* item)
    {
        router->insertChild(0, router->takeChild(router->indexOfChild(item)));
    };

    move_to_top(new SidebarRouterUsers(router_id, router));
    move_to_top(new SidebarRouterHosts(router_id, router));
    move_to_top(new SidebarRouterClients(router_id, router));
    move_to_top(new SidebarRouterRelays(router_id, router));

    router->setExpanded(true);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::removeRouterSections(qint64 router_id)
{
    SidebarRouter* router = routerById(router_id);
    if (!router)
        return;

    for (int i = router->childCount() - 1; i >= 0; --i)
    {
        SidebarItem* child = static_cast<SidebarItem*>(router->child(i));
        const SidebarItem::Type type = child->itemType();
        if (type == SidebarItem::ROUTER_HOSTS || type == SidebarItem::ROUTER_USERS ||
            type == SidebarItem::ROUTER_CLIENTS || type == SidebarItem::ROUTER_RELAYS)
        {
            delete router->takeChild(i);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Sidebar::addRouterEvent(Severity severity, qint64 router_id, const QString& message)
{
    const RouterStatusWidget::Event entry{ QDateTime::currentDateTime(), severity, message };

    auto it = router_events_.find(router_id);
    if (it != router_events_.end())
        it->append(entry);

    emit sig_routerEvent(router_id, entry);
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::onMousePress(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        start_pos_ = event->pos();

    return false;
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::onMouseMove(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        int distance = (event->pos() - start_pos_).manhattanLength();
        if (distance > QApplication::startDragDistance())
        {
            startDrag();
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::startDrag()
{
    QTreeWidgetItem* tree_item = tree_widget_->itemAt(start_pos_);
    if (!tree_item)
        return;

    SidebarItem* item = static_cast<SidebarItem*>(tree_item);
    if (item->itemType() == SidebarItem::LOCAL_GROUP)
    {
        SidebarLocalGroup* group_item = static_cast<SidebarLocalGroup*>(item);

        // Don't allow dragging the root "Local" group.
        if (group_item->groupId() == 0)
            return;

        LocalGroupDrag drag(this);
        drag.setGroupItem(group_item, local_group_mime_type_);

        const QIcon icon = group_item->icon(0);
        drag.setPixmap(icon.pixmap(icon.actualSize(QSize(16, 16))));

        drag.exec(Qt::MoveAction);
    }
    else if (item->itemType() == SidebarItem::ROUTER_GROUP)
    {
        auto* group_item = static_cast<SidebarRouterGroup*>(item);

        // Clients are read-only and cannot move host groups.
        Router* router = Router::instance(group_item->routerId());
        if (!router || router->config().sessionType() == proto::router::SESSION_TYPE_CLIENT)
            return;

        RouterGroupDrag drag(this);
        drag.setGroupItem(group_item, router_group_mime_type_);

        const QIcon icon = group_item->icon(0);
        drag.setPixmap(icon.pixmap(icon.actualSize(QSize(16, 16))));

        drag.exec(Qt::MoveAction);
    }
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::onDragEnter(QDragEnterEvent* event)
{
    const QMimeData* mime_data = event->mimeData();

    if (mime_data->hasFormat(local_host_mime_type_) || mime_data->hasFormat(router_host_mime_type_) ||
        mime_data->hasFormat(local_group_mime_type_) || mime_data->hasFormat(router_group_mime_type_))
    {
        event->acceptProposedAction();
        dragging_ = true;
        drag_source_item_ = tree_widget_->currentItem();
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::onDragMove(QDragMoveEvent* event)
{
    event->ignore();

    const QMimeData* mime_data = event->mimeData();

    if (mime_data->hasFormat(local_group_mime_type_))
    {
        QTreeWidgetItem* source_item = tree_widget_->itemAt(start_pos_);
        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());

        if (isAllowedDropTarget(target_tree_item, source_item))
        {
            tree_widget_->clearSelection();
            target_tree_item->setSelected(true);
            event->acceptProposedAction();
        }

        return true;
    }
    else if (mime_data->hasFormat(router_group_mime_type_))
    {
        const RouterGroupMimeData* group_mime_data = dynamic_cast<const RouterGroupMimeData*>(mime_data);
        if (!group_mime_data)
            return true;

        SidebarRouterGroup* source_group = group_mime_data->groupItem();
        if (!source_group)
            return true;

        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!target_tree_item || target_tree_item == tree_widget_->invisibleRootItem() ||
            target_tree_item == source_group)
        {
            return true;
        }

        // The target is either a host group (nest under it) or the workspace item (move to
        // the workspace root). Cross-router/workspace moves are not supported by modifyGroup.
        SidebarItem* target_item = static_cast<SidebarItem*>(target_tree_item);
        if (target_item->itemType() == SidebarItem::ROUTER_GROUP)
        {
            auto* target_group = static_cast<SidebarRouterGroup*>(target_item);
            if (target_group->routerId() != source_group->routerId() ||
                target_group->workspaceId() != source_group->workspaceId())
            {
                return true;
            }
        }
        else if (target_item->itemType() == SidebarItem::ROUTER_WORKSPACE)
        {
            auto* target_workspace = static_cast<SidebarRouterWorkspace*>(target_item);
            if (target_workspace->routerId() != source_group->routerId() ||
                target_workspace->workspaceId() != source_group->workspaceId())
            {
                return true;
            }
        }
        else
        {
            return true;
        }

        // No-op move: target is already the source's parent.
        QTreeWidgetItem* current_parent = source_group->parent();
        if (current_parent == target_tree_item)
            return true;

        // Prevent a cycle: target must not be source itself or any of its descendants.
        std::function<bool(QTreeWidgetItem*, QTreeWidgetItem*)> isChild =
            [&](QTreeWidgetItem* parent, QTreeWidgetItem* child) -> bool
        {
            for (int i = 0; i < parent->childCount(); ++i)
            {
                QTreeWidgetItem* current = parent->child(i);
                if (current == child)
                    return true;
                if (isChild(current, child))
                    return true;
            }
            return false;
        };
        if (isChild(source_group, target_tree_item))
            return true;

        tree_widget_->clearSelection();
        target_tree_item->setSelected(true);
        event->acceptProposedAction();
        return true;
    }
    else if (mime_data->hasFormat(local_host_mime_type_))
    {
        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!target_tree_item || target_tree_item == tree_widget_->invisibleRootItem())
            return true;

        SidebarItem* target_item = static_cast<SidebarItem*>(target_tree_item);
        if (target_item->itemType() != SidebarItem::LOCAL_GROUP)
            return true;

        const LocalHostMimeData* host_mime_data = dynamic_cast<const LocalHostMimeData*>(mime_data);
        if (!host_mime_data)
            return true;

        LocalGroupWidget::Item* host_item = host_mime_data->hostItem();
        if (!host_item)
            return true;

        // Don't allow drop to the same group.
        if (host_item->groupId() == target_item->groupId())
            return true;

        tree_widget_->clearSelection();
        target_tree_item->setSelected(true);
        event->acceptProposedAction();
        return true;
    }
    else if (mime_data->hasFormat(router_host_mime_type_))
    {
        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!target_tree_item || target_tree_item == tree_widget_->invisibleRootItem())
            return true;

        const RouterHostMimeData* host_mime_data = dynamic_cast<const RouterHostMimeData*>(mime_data);
        if (!host_mime_data)
            return true;

        const Router::Host& host = host_mime_data->host();

        // The target is either a host group or the workspace item (move to the workspace root,
        // group id 0). Cross-router or cross-workspace moves are forbidden by the server;
        // reflect that in the UI by refusing the drop here.
        SidebarItem* target_item = static_cast<SidebarItem*>(target_tree_item);
        qint64 target_router_id = 0;
        qint64 target_workspace_id = 0;
        qint64 target_group_id = 0;

        if (target_item->itemType() == SidebarItem::ROUTER_GROUP)
        {
            auto* group_item = static_cast<SidebarRouterGroup*>(target_item);
            target_router_id = group_item->routerId();
            target_workspace_id = group_item->workspaceId();
            target_group_id = group_item->groupId();
        }
        else if (target_item->itemType() == SidebarItem::ROUTER_WORKSPACE)
        {
            auto* workspace_item = static_cast<SidebarRouterWorkspace*>(target_item);
            target_router_id = workspace_item->routerId();
            target_workspace_id = workspace_item->workspaceId();
        }
        else
        {
            return true;
        }

        if (target_router_id != host_mime_data->routerId() || target_workspace_id != host.workspace_id)
            return true;

        // Don't allow drop to the same group.
        if (target_group_id == host.group_id)
            return true;

        tree_widget_->clearSelection();
        target_tree_item->setSelected(true);
        event->acceptProposedAction();
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::onDragLeave(QDragLeaveEvent* /* event */)
{
    dragging_ = false;
    if (drag_source_item_)
    {
        tree_widget_->setCurrentItem(drag_source_item_);
        drag_source_item_ = nullptr;
    }
    return false;
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::onDrop(QDropEvent* event)
{
    dragging_ = false;

    QTreeWidgetItem* saved_item = drag_source_item_;
    drag_source_item_ = nullptr;

    auto restoreSelection = [&]()
    {
        if (saved_item)
            tree_widget_->setCurrentItem(saved_item);
    };

    const QMimeData* mime_data = event->mimeData();
    if (!mime_data)
        return false;

    if (mime_data->hasFormat(local_group_mime_type_))
    {
        const LocalGroupMimeData* group_mime_data = dynamic_cast<const LocalGroupMimeData*>(mime_data);
        if (!group_mime_data)
        {
            restoreSelection();
            return true;
        }

        SidebarLocalGroup* source_group = group_mime_data->groupItem();
        if (!source_group)
        {
            restoreSelection();
            return true;
        }

        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!isAllowedDropTarget(target_tree_item, source_group))
        {
            restoreSelection();
            return true;
        }

        SidebarItem* target_item = static_cast<SidebarItem*>(target_tree_item);

        // Check if a group with the same name already exists in the target group.
        QList<GroupConfig> target_groups = Database::instance().groupList(target_item->groupId());
        for (const GroupConfig& existing : std::as_const(target_groups))
        {
            if (existing.id() != source_group->groupId() && existing.name() == source_group->groupName())
            {
                MsgBox::warning(tree_widget_,
                    tr("A group with this name already exists in the selected parent group."));
                restoreSelection();
                return true;
            }
        }

        // Update the group's parent in the database.
        if (!Database::instance().moveGroup(source_group->groupId(), target_item->groupId()))
        {
            MsgBox::warning(tree_widget_, tr("Failed to move the group."));
            restoreSelection();
            return true;
        }

        event->acceptProposedAction();

        // Reload groups and select the moved group.
        reloadGroups(source_group->groupId());
        return true;
    }
    else if (mime_data->hasFormat(local_host_mime_type_))
    {
        const LocalHostMimeData* host_mime_data = dynamic_cast<const LocalHostMimeData*>(mime_data);
        if (!host_mime_data)
        {
            restoreSelection();
            return true;
        }

        LocalGroupWidget::Item* host_item = host_mime_data->hostItem();
        if (!host_item)
        {
            restoreSelection();
            return true;
        }

        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!target_tree_item || target_tree_item == tree_widget_->invisibleRootItem())
        {
            restoreSelection();
            return true;
        }

        SidebarItem* target_item = static_cast<SidebarItem*>(target_tree_item);
        if (target_item->itemType() != SidebarItem::LOCAL_GROUP)
        {
            restoreSelection();
            return true;
        }

        if (host_item->groupId() == target_item->groupId())
        {
            restoreSelection();
            return true;
        }

        // Check if a host with the same name already exists in the target group.
        QList<HostConfig> target_hosts = Database::instance().hostList(target_item->groupId());
        for (const HostConfig& existing : std::as_const(target_hosts))
        {
            if (existing.name() == host_item->computerName())
            {
                MsgBox::warning(tree_widget_, tr("A host with this name already exists in the selected group."));
                restoreSelection();
                return true;
            }
        }

        // Update the host's group in the database.
        std::optional<HostConfig> host = Database::instance().findHost(host_item->entryId());
        if (!host.has_value())
        {
            restoreSelection();
            return true;
        }

        host->setGroupId(target_item->groupId());

        if (!Database::instance().modifyHost(*host))
        {
            MsgBox::warning(tree_widget_, tr("Failed to move the host to the selected group."));
            restoreSelection();
            return true;
        }

        event->acceptProposedAction();
        restoreSelection();
        emit sig_itemDropped();
        return true;
    }
    else if (mime_data->hasFormat(router_group_mime_type_))
    {
        const RouterGroupMimeData* group_mime_data = dynamic_cast<const RouterGroupMimeData*>(mime_data);
        if (!group_mime_data)
        {
            restoreSelection();
            return true;
        }

        SidebarRouterGroup* source_group = group_mime_data->groupItem();
        if (!source_group)
        {
            restoreSelection();
            return true;
        }

        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!target_tree_item || target_tree_item == tree_widget_->invisibleRootItem() ||
            target_tree_item == source_group)
        {
            restoreSelection();
            return true;
        }

        // The target is either a host group (drop = nest under it, parent_id is its entry_id)
        // or the workspace item (drop = move to the workspace root, parent_id 0).
        SidebarItem* target_item = static_cast<SidebarItem*>(target_tree_item);
        qint64 target_router_id = 0;
        qint64 target_workspace_id = 0;
        qint64 target_group_id = 0;

        if (target_item->itemType() == SidebarItem::ROUTER_GROUP)
        {
            auto* target_group = static_cast<SidebarRouterGroup*>(target_item);
            target_router_id = target_group->routerId();
            target_workspace_id = target_group->workspaceId();
            target_group_id = target_group->groupId();
        }
        else if (target_item->itemType() == SidebarItem::ROUTER_WORKSPACE)
        {
            auto* target_workspace = static_cast<SidebarRouterWorkspace*>(target_item);
            target_router_id = target_workspace->routerId();
            target_workspace_id = target_workspace->workspaceId();
        }
        else
        {
            restoreSelection();
            return true;
        }

        if (target_router_id != source_group->routerId() ||
            target_workspace_id != source_group->workspaceId())
        {
            restoreSelection();
            return true;
        }

        if (source_group->parent() == target_tree_item)
        {
            restoreSelection();
            return true;
        }

        const qint64 router_id = source_group->routerId();
        Router* router = Router::instance(router_id);
        if (!router)
        {
            restoreSelection();
            return true;
        }

        // Copy the existing record and just rewrite parent_id.
        Router::Group group = source_group->group();
        group.parent_id = target_group_id;

        router->modifyGroup(source_group->workspaceId(), group, this,
            [this, router_id](const proto::router::GroupResult& result)
        {
            if (result.error_code() != proto::router::kErrorOk)
            {
                LOG(ERROR) << "Move group failed:" << result.error_code();
                MsgBox::warning(tree_widget_, tr("Failed to move the group."));
                return;
            }
            emit sig_routerGroupMoved(router_id);
        });

        event->acceptProposedAction();
        restoreSelection();
        return true;
    }
    else if (mime_data->hasFormat(router_host_mime_type_))
    {
        const RouterHostMimeData* host_mime_data = dynamic_cast<const RouterHostMimeData*>(mime_data);
        if (!host_mime_data)
        {
            restoreSelection();
            return true;
        }

        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!target_tree_item || target_tree_item == tree_widget_->invisibleRootItem())
        {
            restoreSelection();
            return true;
        }

        // The target is either a host group or the workspace item (move to the workspace root).
        SidebarItem* target_item = static_cast<SidebarItem*>(target_tree_item);
        qint64 target_router_id = 0;
        qint64 target_workspace_id = 0;
        qint64 target_group_id = 0;

        if (target_item->itemType() == SidebarItem::ROUTER_GROUP)
        {
            auto* group_item = static_cast<SidebarRouterGroup*>(target_item);
            target_router_id = group_item->routerId();
            target_workspace_id = group_item->workspaceId();
            target_group_id = group_item->groupId();
        }
        else if (target_item->itemType() == SidebarItem::ROUTER_WORKSPACE)
        {
            auto* workspace_item = static_cast<SidebarRouterWorkspace*>(target_item);
            target_router_id = workspace_item->routerId();
            target_workspace_id = workspace_item->workspaceId();
        }
        else
        {
            restoreSelection();
            return true;
        }

        Router::Host host = host_mime_data->host();
        const qint64 router_id = host_mime_data->routerId();

        // Repeat the eligibility checks from onDragMove in case the user releases over a
        // target that wasn't validated (DragLeave without DragMove can happen).
        if (target_router_id != router_id ||
            target_workspace_id != host.workspace_id ||
            target_group_id == host.group_id)
        {
            restoreSelection();
            return true;
        }

        Router* router = Router::instance(router_id);
        if (!router)
        {
            restoreSelection();
            return true;
        }

        host.group_id = target_group_id;
        router->editHost(host, this, [this, router_id](const proto::router::HostResult& result)
        {
            if (result.error_code() != proto::router::kErrorOk)
            {
                LOG(ERROR) << "Move host failed:" << result.error_code();
                MsgBox::warning(tree_widget_, tr("Failed to move the host to the selected group."));
                return;
            }
            emit sig_routerHostMoved(router_id);
        });

        event->acceptProposedAction();
        restoreSelection();
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::isAllowedDropTarget(QTreeWidgetItem* target, QTreeWidgetItem* source) const
{
    if (!target || !source)
        return false;

    if (target == tree_widget_->invisibleRootItem() || target == source)
        return false;

    SidebarItem* target_item = static_cast<SidebarItem*>(target);
    if (target_item->itemType() != SidebarItem::LOCAL_GROUP)
        return false;

    // Prevent dropping a group onto its own descendant.
    std::function<bool(QTreeWidgetItem*, QTreeWidgetItem*)> isChild =
        [&](QTreeWidgetItem* parent, QTreeWidgetItem* child) -> bool
    {
        for (int i = 0; i < parent->childCount(); ++i)
        {
            QTreeWidgetItem* current = parent->child(i);
            if (current == child)
                return true;
            if (isChild(current, child))
                return true;
        }
        return false;
    };

    return !isChild(source, target);
}

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* Sidebar::findGroupItem(qint64 group_id, QTreeWidgetItem* parent) const
{
    for (int i = 0; i < parent->childCount(); ++i)
    {
        QTreeWidgetItem* child = parent->child(i);
        if (static_cast<SidebarItem*>(child)->groupId() == group_id)
            return child;

        QTreeWidgetItem* found = findGroupItem(group_id, child);
        if (found)
            return found;
    }

    return nullptr;
}
