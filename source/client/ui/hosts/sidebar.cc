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

#include "client/ui/hosts/sidebar.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QMouseEvent>
#include <QSet>
#include <QUuid>
#include <QVBoxLayout>

#include "base/logging.h"
#include "client/config.h"
#include "client/database.h"
#include "client/settings.h"
#include "client/ui/hosts/local_group_dialog.h"
#include "client/ui/hosts/local_group_widget.h"
#include "client/ui/router_dialog.h"
#include "common/ui/msg_box.h"

//--------------------------------------------------------------------------------------------------
Sidebar::Sidebar(QWidget* parent)
    : QWidget(parent),
      group_mime_type_(QString("application/%1").arg(QUuid::createUuid().toString()))
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
    local_root_ = new LocalGroupItem(local_root_data, tree_widget_);
    local_root_->setExpanded(Settings().isGroupExpanded(local_root_data.id()));

    // Setup drag-and-drop.
    tree_widget_->setAcceptDrops(true);
    tree_widget_->viewport()->installEventFilter(this);

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
void Sidebar::loadGroups(qint64 parent_id, QTreeWidgetItem* parent_item)
{
    QList<GroupConfig> groups = Database::instance().groupList(parent_id);

    Settings settings;

    for (const GroupConfig& group : std::as_const(groups))
    {
        LocalGroupItem* item = new LocalGroupItem(group, parent_item);
        item->setExpanded(settings.isGroupExpanded(group.id()));

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
    local_root_->setExpanded(Settings().isGroupExpanded(0));

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

    current_group_id_ = static_cast<Item*>(selected)->groupId();
    emit sig_switchContent(Item::LOCAL_GROUP);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::loadRouters()
{
    QList<RouterConfig> routers = Database::instance().routerList();

    for (const RouterConfig& router_config : std::as_const(routers))
    {
        RouterItem* router = new RouterItem(router_config.routerId(), router_config.displayLabel(), tree_widget_);
        router->setExpanded(true);
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
        Item* item = static_cast<Item*>(tree_widget_->topLevelItem(i));
        if (item->itemType() != Item::ROUTER)
            continue;

        if (!new_ids.contains(static_cast<RouterItem*>(item)->routerId()))
            delete tree_widget_->takeTopLevelItem(i);
    }

    // Update existing routers, append new ones at the end. Child items (Unassigned, ...) are
    // managed by setRouterStatus() based on the current connection state, not here.
    for (const RouterConfig& router_config : std::as_const(routers))
    {
        const QString name = router_config.displayLabel();

        RouterItem* router = routerById(router_config.routerId());
        if (router)
        {
            router->setName(name);
        }
        else
        {
            RouterItem* new_router = new RouterItem(router_config.routerId(), name, tree_widget_);
            new_router->setExpanded(true);

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
void Sidebar::setRouterStatus(qint64 router_id, RouterItem::Status status)
{
    RouterItem* router = routerById(router_id);
    if (!router)
        return;

    router->setStatus(status);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::setRouterWorkspaces(qint64 router_id, const QList<Router::Workspace>& workspaces)
{
    RouterItem* router = routerById(router_id);
    if (!router)
        return;

    // Drop existing ROUTER_GROUP children, keep anything else untouched.
    for (int i = router->childCount() - 1; i >= 0; --i)
    {
        Item* child = static_cast<Item*>(router->child(i));
        if (child->itemType() == Item::ROUTER_GROUP)
            delete router->takeChild(i);
    }

    for (const Router::Workspace& workspace : workspaces)
        new RouterGroupItem(router_id, workspace.entry_id, workspace.name, router);

    router->setExpanded(true);
}

//--------------------------------------------------------------------------------------------------
qint64 Sidebar::currentGroupId() const
{
    return current_group_id_;
}

//--------------------------------------------------------------------------------------------------
Sidebar::Item* Sidebar::currentItem() const
{
    return static_cast<Item*>(tree_widget_->currentItem());
}

//--------------------------------------------------------------------------------------------------
Sidebar::RouterItem* Sidebar::routerById(qint64 router_id) const
{
    for (int i = 0; i < tree_widget_->topLevelItemCount(); ++i)
    {
        Item* item = static_cast<Item*>(tree_widget_->topLevelItem(i));
        if (item->itemType() != Item::ROUTER)
            continue;

        RouterItem* router = static_cast<RouterItem*>(item);
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
        Item* item = static_cast<Item*>(tree_widget_->topLevelItem(i));
        if (item->itemType() != Item::ROUTER)
            continue;

        ids.append(static_cast<RouterItem*>(item)->routerId());
    }

    return ids;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::setHostMimeType(const QString& mime_type)
{
    host_mime_type_ = mime_type;
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::dragging() const
{
    return dragging_;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onAddGroup()
{
    LOG(INFO) << "[ACTION] Add group";

    Item* item = currentItem();
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

    Item* item = currentItem();
    if (!item)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    if (item->itemType() != Item::LOCAL_GROUP)
        return;

    LocalGroupItem* local_group = static_cast<LocalGroupItem*>(item);
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

    Item* item = currentItem();
    if (!item)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    if (item->itemType() != Item::Type::LOCAL_GROUP || item->groupId() == 0) // Root group.
        return;

    LocalGroupItem* local_group = static_cast<LocalGroupItem*>(item);

    QString message = tr("Are you sure you want to delete group \"%1\"?").arg(local_group->groupName());

    if (MsgBox::question(this, message) == MsgBox::No)
    {
        LOG(INFO) << "Action is rejected by user";
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

    Settings().removeGroupExpanded(group_id);

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
    Item* item = currentItem();
    if (!item || item->itemType() != Item::ROUTER)
        return;

    qint64 router_id = static_cast<RouterItem*>(item)->routerId();
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
    Item* item = currentItem();
    if (!item || item->itemType() != Item::ROUTER)
        return;

    RouterItem* router_item = static_cast<RouterItem*>(item);
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
    emit sig_routersChanged();
}

//--------------------------------------------------------------------------------------------------
void Sidebar::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    if (event->type() == QEvent::LanguageChange && local_root_)
        local_root_->setText(0, tr("Local"));
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == tree_widget_->viewport())
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
bool Sidebar::onDragEnter(QDragEnterEvent* event)
{
    const QMimeData* mime_data = event->mimeData();

    if (mime_data->hasFormat(host_mime_type_) || mime_data->hasFormat(group_mime_type_))
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

    if (mime_data->hasFormat(group_mime_type_))
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
    else if (mime_data->hasFormat(host_mime_type_))
    {
        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!target_tree_item || target_tree_item == tree_widget_->invisibleRootItem())
            return true;

        Item* target_item = static_cast<Item*>(target_tree_item);
        if (target_item->itemType() != Item::LOCAL_GROUP)
            return true;

        const LocalGroupWidget::HostMimeData* host_mime_data =
            dynamic_cast<const LocalGroupWidget::HostMimeData*>(mime_data);
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

    if (mime_data->hasFormat(group_mime_type_))
    {
        const GroupMimeData* group_mime_data = dynamic_cast<const GroupMimeData*>(mime_data);
        if (!group_mime_data)
        {
            restoreSelection();
            return true;
        }

        LocalGroupItem* source_group = group_mime_data->groupItem();
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

        Item* target_item = static_cast<Item*>(target_tree_item);

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
            MsgBox::warning(tree_widget_,
                tr("Failed to move the group."));
            restoreSelection();
            return true;
        }

        event->acceptProposedAction();

        // Reload groups and select the moved group.
        reloadGroups(source_group->groupId());
        return true;
    }
    else if (mime_data->hasFormat(host_mime_type_))
    {
        const LocalGroupWidget::HostMimeData* host_mime_data =
            dynamic_cast<const LocalGroupWidget::HostMimeData*>(mime_data);
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

        Item* target_item = static_cast<Item*>(target_tree_item);
        if (target_item->itemType() != Item::LOCAL_GROUP)
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
        QList<HostConfig> target_hosts =
            Database::instance().hostList(target_item->groupId());
        for (const HostConfig& existing : std::as_const(target_hosts))
        {
            if (existing.name() == host_item->computerName())
            {
                MsgBox::warning(tree_widget_,
                    tr("A host with this name already exists in the selected group."));
                restoreSelection();
                return true;
            }
        }

        // Update the host's group in the database.
        std::optional<HostConfig> host =
            Database::instance().findHost(host_item->entryId());
        if (!host.has_value())
        {
            restoreSelection();
            return true;
        }

        host->setGroupId(target_item->groupId());

        if (!Database::instance().modifyHost(*host))
        {
            MsgBox::warning(tree_widget_,
                tr("Failed to move the host to the selected group."));
            restoreSelection();
            return true;
        }

        event->acceptProposedAction();
        restoreSelection();
        emit sig_itemDropped();
        return true;
    }

    return false;
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

    Item* item = static_cast<Item*>(tree_item);
    if (item->itemType() != Item::LOCAL_GROUP)
        return;

    LocalGroupItem* group_item = static_cast<LocalGroupItem*>(item);

    // Don't allow dragging the root "Local" group.
    if (group_item->groupId() == 0)
        return;

    GroupDrag* drag = new GroupDrag(this);
    drag->setGroupItem(group_item, group_mime_type_);

    QIcon icon = group_item->icon(0);
    drag->setPixmap(icon.pixmap(icon.actualSize(QSize(16, 16))));

    drag->exec(Qt::MoveAction);
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::isAllowedDropTarget(QTreeWidgetItem* target, QTreeWidgetItem* source) const
{
    if (!target || !source)
        return false;

    if (target == tree_widget_->invisibleRootItem() || target == source)
        return false;

    Item* target_item = static_cast<Item*>(target);
    if (target_item->itemType() != Item::LOCAL_GROUP)
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
void Sidebar::onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    if (!current)
        return;

    if (dragging_)
        return;

    Item* item = static_cast<Item*>(current);

    switch (item->itemType())
    {
        case Item::LOCAL_GROUP:
            current_group_id_ = item->groupId();
            break;

        case Item::ROUTER:
            // Nothing
            break;

        case Item::ROUTER_GROUP:
            current_group_id_ = item->groupId();
            break;
    }

    emit sig_switchContent(item->itemType());
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onContextMenu(const QPoint& pos)
{
    Item* item = static_cast<Item*>(tree_widget_->itemAt(pos));
    if (!item)
        return;

    tree_widget_->setCurrentItem(item);

    emit sig_contextMenu(item->itemType(), tree_widget_->viewport()->mapToGlobal(pos));
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onItemExpanded(QTreeWidgetItem* item)
{
    CHECK(item);

    Item* sidebar_item = static_cast<Item*>(item);
    if (sidebar_item->itemType() != Item::LOCAL_GROUP)
        return;

    Settings().setGroupExpanded(sidebar_item->groupId(), true);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onItemCollapsed(QTreeWidgetItem* item)
{
    CHECK(item);

    Item* sidebar_item = static_cast<Item*>(item);
    if (sidebar_item->itemType() != Item::LOCAL_GROUP)
        return;

    Settings().setGroupExpanded(sidebar_item->groupId(), false);
}

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* Sidebar::findGroupItem(qint64 group_id, QTreeWidgetItem* parent) const
{
    for (int i = 0; i < parent->childCount(); ++i)
    {
        QTreeWidgetItem* child = parent->child(i);
        if (static_cast<Item*>(child)->groupId() == group_id)
            return child;

        QTreeWidgetItem* found = findGroupItem(group_id, child);
        if (found)
            return found;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
Sidebar::Item::Item(Type type, qint64 group_id, QTreeWidget* parent)
    : QTreeWidgetItem(parent),
      type_(type),
      group_id_(group_id)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Sidebar::Item::Item(Type type, qint64 group_id, QTreeWidgetItem* parent)
    : QTreeWidgetItem(parent),
      type_(type),
      group_id_(group_id)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Sidebar::LocalGroupItem::LocalGroupItem(const GroupConfig& group, QTreeWidget* parent)
    : Item(LOCAL_GROUP, group.id(), parent),
      parent_id_(group.parentId()),
      group_name_(group.name())
{
    setText(0, group.name());
    setIcon(0, QIcon(":/img/folder.svg"));
}

//--------------------------------------------------------------------------------------------------
Sidebar::LocalGroupItem::LocalGroupItem(const GroupConfig& group, QTreeWidgetItem* parent)
    : Item(LOCAL_GROUP, group.id(), parent),
      parent_id_(group.parentId()),
      group_name_(group.name())
{
    setText(0, group.name());
    setIcon(0, QIcon(":/img/folder.svg"));
}

//--------------------------------------------------------------------------------------------------
Sidebar::RouterItem::RouterItem(qint64 router_id, const QString& name, QTreeWidget* parent)
    : Item(ROUTER, -1, parent),
      router_id_(router_id),
      name_(name)
{
    setText(0, name);
    setIcon(0, QIcon(":/img/router-offline.svg"));
}

//--------------------------------------------------------------------------------------------------
qint64 Sidebar::RouterItem::routerId() const
{
    return router_id_;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::RouterItem::setName(const QString& name)
{
    name_ = name;
    setText(0, name);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::RouterItem::setStatus(Status status)
{
    switch (status)
    {
        case Status::OFFLINE:    setIcon(0, QIcon(":/img/router-offline.svg"));    break;
        case Status::CONNECTING: setIcon(0, QIcon(":/img/router-connecting.svg")); break;
        case Status::ONLINE:     setIcon(0, QIcon(":/img/router-online.svg"));     break;
        default: break;
    }
}

//--------------------------------------------------------------------------------------------------
Sidebar::RouterGroupItem::RouterGroupItem(qint64 router_id, qint64 group_id, const QString& name,
                                          QTreeWidgetItem* parent)
    : Item(ROUTER_GROUP, group_id, parent),
      router_id_(router_id)
{
    setText(0, name);
    setIcon(0, QIcon(":/img/workspace.svg"));
}
