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

#include "client/ui/computers_tab/sidebar.h"

#include <QHeaderView>
#include <QVBoxLayout>

#include "base/logging.h"
#include "client/local_data.h"
#include "client/local_database.h"

namespace client {

//--------------------------------------------------------------------------------------------------
Sidebar::Sidebar(QWidget* parent)
    : QWidget(parent)
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

    remote_root_ = new Router(tr("Remote"), tree_widget_);
    remote_root_->setExpanded(true);

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
void Sidebar::onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    LOG(INFO) << "GROUP ITEM CHANGED";
    if (!current)
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
Sidebar::Router::Router(const QString& name, QTreeWidget* parent)
    : Item(ROUTER, -1, parent)
{
    setText(0, name);
    setIcon(0, QIcon(":/img/stack.svg"));
}

//--------------------------------------------------------------------------------------------------
Sidebar::RouterGroup::RouterGroup(qint64 group_id, const QString& name, QTreeWidgetItem* parent)
    : Item(ROUTER_GROUP, group_id, parent)
{
    setText(0, name);
    setIcon(0, QIcon(":/img/folder.svg"));
}

} // namespace client
