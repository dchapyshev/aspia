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
#include <QUuid>
#include <QVBoxLayout>

#include "base/logging.h"
#include "build/build_config.h"
#include "client/local_data.h"
#include "client/local_database.h"
#include "client/ui/hosts/local_group_widget.h"
#include "client/settings.h"
#include "common/ui/msg_box.h"

namespace client {

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

    GroupData local_root_data;
    local_root_data.id = 0;
    local_root_data.parent_id = 0;
    local_root_data.name = tr("Local");

    // Create root groups in the tree.
    local_root_ = new LocalGroup(local_root_data, tree_widget_);
    local_root_->setExpanded(true);

    loadRouters();

    // Setup drag-and-drop.
    tree_widget_->setAcceptDrops(true);
    tree_widget_->viewport()->installEventFilter(this);

    QTreeWidgetItem* invisible_root = tree_widget_->invisibleRootItem();
    invisible_root->setFlags(invisible_root->flags() ^ Qt::ItemIsDropEnabled);

    connect(tree_widget_, &QTreeWidget::currentItemChanged, this, &Sidebar::onCurrentItemChanged);
    connect(tree_widget_, &QTreeWidget::customContextMenuRequested, this, &Sidebar::onContextMenu);

    // Load groups from database under Local root.
    loadGroups(0, local_root_);
    tree_widget_->setCurrentItem(local_root_);
}

//--------------------------------------------------------------------------------------------------
Sidebar::~Sidebar() = default;

//--------------------------------------------------------------------------------------------------
void Sidebar::loadGroups(qint64 parent_id, QTreeWidgetItem* parent_item)
{
    QList<GroupData> groups = LocalDatabase::instance().groupList(parent_id);

    for (const GroupData& group : std::as_const(groups))
    {
        LocalGroup* item = new LocalGroup(group, parent_item);
        item->setExpanded(group.expanded);

        // Load child groups recursively.
        loadGroups(group.id, item);
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
    tree_widget_->setCurrentItem(selected);

    current_group_id_ = static_cast<Item*>(selected)->groupId();
    emit sig_switchContent(Item::LOCAL_GROUP);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::loadRouters()
{
    Settings settings;
    RouterConfigList router_configs = settings.routerConfigs();

    for (int i = 0; i < router_configs.size(); ++i)
    {
        const RouterConfig& config = router_configs[i];

        QString name;
        if (!config.name.isEmpty())
        {
            name = config.name;
        }
        else if (config.port != DEFAULT_ROUTER_TCP_PORT)
        {
            name = QString("%1:%2").arg(config.address).arg(config.port);
        }
        else
        {
            name = config.address;
        }

        Router* router = new Router(config.uuid, name, tree_widget_);
        router->setExpanded(true);
    }
}

//--------------------------------------------------------------------------------------------------
void Sidebar::reloadRouters()
{
    // Remove all existing Router items from the tree.
    for (int i = tree_widget_->topLevelItemCount() - 1; i >= 0; --i)
    {
        Item* item = static_cast<Item*>(tree_widget_->topLevelItem(i));
        if (item->itemType() == Item::ROUTER)
            delete tree_widget_->takeTopLevelItem(i);
    }

    loadRouters();
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
Sidebar::Router* Sidebar::routerByUuid(const QUuid& uuid) const
{
    for (int i = 0; i < tree_widget_->topLevelItemCount(); ++i)
    {
        Item* item = static_cast<Item*>(tree_widget_->topLevelItem(i));
        if (item->itemType() != Item::ROUTER)
            continue;

        Router* router = static_cast<Router*>(item);
        if (router->uuid() == uuid)
            return router;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
QList<QUuid> Sidebar::routerUuids() const
{
    QList<QUuid> uuids;

    for (int i = 0; i < tree_widget_->topLevelItemCount(); ++i)
    {
        Item* item = static_cast<Item*>(tree_widget_->topLevelItem(i));
        if (item->itemType() != Item::ROUTER)
            continue;

        uuids.append(static_cast<Router*>(item)->uuid());
    }

    return uuids;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::setComputerMimeType(const QString& mime_type)
{
    computer_mime_type_ = mime_type;
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::dragging() const
{
    return dragging_;
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

    if (mime_data->hasFormat(computer_mime_type_) || mime_data->hasFormat(group_mime_type_))
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
    else if (mime_data->hasFormat(computer_mime_type_))
    {
        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!target_tree_item || target_tree_item == tree_widget_->invisibleRootItem())
            return true;

        Item* target_item = static_cast<Item*>(target_tree_item);
        if (target_item->itemType() != Item::LOCAL_GROUP)
            return true;

        const LocalGroupWidget::ComputerMimeData* computer_mime_data =
            dynamic_cast<const LocalGroupWidget::ComputerMimeData*>(mime_data);
        if (!computer_mime_data)
            return true;

        LocalGroupWidget::Item* computer_item = computer_mime_data->computerItem();
        if (!computer_item)
            return true;

        // Don't allow drop to the same group.
        if (computer_item->groupId() == target_item->groupId())
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

        LocalGroup* source_group = group_mime_data->groupItem();
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
        QList<GroupData> target_groups = LocalDatabase::instance().groupList(target_item->groupId());
        for (const GroupData& existing : std::as_const(target_groups))
        {
            if (existing.id != source_group->groupId() && existing.name == source_group->groupName())
            {
                common::MsgBox::warning(tree_widget_,
                    tr("A group with this name already exists in the selected parent group."));
                restoreSelection();
                return true;
            }
        }

        // Update the group's parent in the database.
        if (!LocalDatabase::instance().moveGroup(source_group->groupId(), target_item->groupId()))
        {
            common::MsgBox::warning(tree_widget_,
                tr("Failed to move the group."));
            restoreSelection();
            return true;
        }

        event->acceptProposedAction();

        // Reload groups and select the moved group.
        reloadGroups(source_group->groupId());
        return true;
    }
    else if (mime_data->hasFormat(computer_mime_type_))
    {
        const LocalGroupWidget::ComputerMimeData* computer_mime_data =
            dynamic_cast<const LocalGroupWidget::ComputerMimeData*>(mime_data);
        if (!computer_mime_data)
        {
            restoreSelection();
            return true;
        }

        LocalGroupWidget::Item* computer_item = computer_mime_data->computerItem();
        if (!computer_item)
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

        if (computer_item->groupId() == target_item->groupId())
        {
            restoreSelection();
            return true;
        }

        // Check if a computer with the same name already exists in the target group.
        QList<ComputerData> target_computers =
            LocalDatabase::instance().computerList(target_item->groupId());
        for (const ComputerData& existing : std::as_const(target_computers))
        {
            if (existing.name == computer_item->computerName())
            {
                common::MsgBox::warning(tree_widget_,
                    tr("A computer with this name already exists in the selected group."));
                restoreSelection();
                return true;
            }
        }

        // Update the computer's group in the database.
        std::optional<ComputerData> computer =
            LocalDatabase::instance().findComputer(computer_item->computerId());
        if (!computer.has_value())
        {
            restoreSelection();
            return true;
        }

        computer->group_id = target_item->groupId();

        if (!LocalDatabase::instance().modifyComputer(*computer))
        {
            common::MsgBox::warning(tree_widget_,
                tr("Failed to move the computer to the selected group."));
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

    LocalGroup* group_item = static_cast<LocalGroup*>(item);

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
    LOG(INFO) << "GROUP ITEM CHANGED";
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

    emit sig_contextMenu(item->itemType(), tree_widget_->viewport()->mapToGlobal(pos));
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
Sidebar::LocalGroup::LocalGroup(const GroupData& group, QTreeWidget* parent)
    : Item(LOCAL_GROUP, group.id, parent),
      parent_id_(group.parent_id),
      group_name_(group.name)
{
    setText(0, group.name);
    setIcon(0, QIcon(":/img/folder.svg"));
}

//--------------------------------------------------------------------------------------------------
Sidebar::LocalGroup::LocalGroup(const GroupData& group, QTreeWidgetItem* parent)
    : Item(LOCAL_GROUP, group.id, parent),
      parent_id_(group.parent_id),
      group_name_(group.name)
{
    setText(0, group.name);
    setIcon(0, QIcon(":/img/folder.svg"));
}

//--------------------------------------------------------------------------------------------------
Sidebar::Router::Router(const QUuid& uuid, const QString& name, QTreeWidget* parent)
    : Item(ROUTER, -1, parent),
      uuid_(uuid),
      name_(name)
{
    setText(0, name);
    setIcon(0, QIcon(":/img/router-offline.svg"));
}

//--------------------------------------------------------------------------------------------------
const QUuid& Sidebar::Router::uuid() const
{
    return uuid_;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::Router::setStatus(Status status)
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
Sidebar::RouterGroup::RouterGroup(qint64 group_id, const QString& name, QTreeWidgetItem* parent)
    : Item(ROUTER_GROUP, group_id, parent)
{
    setText(0, name);
    setIcon(0, QIcon(":/img/folder.svg"));
}

} // namespace client
