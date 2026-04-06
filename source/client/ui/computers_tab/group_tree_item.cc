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

#include "client/ui/computers_tab/group_tree_item.h"

#include <QIcon>

namespace client {

//--------------------------------------------------------------------------------------------------
GroupTreeItem::GroupTreeItem(Type type, qint64 group_id, QTreeWidget* parent)
    : QTreeWidgetItem(parent),
      type_(type),
      group_id_(group_id)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
GroupTreeItem::GroupTreeItem(Type type, qint64 group_id, QTreeWidgetItem* parent)
    : QTreeWidgetItem(parent),
      type_(type),
      group_id_(group_id)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
GroupTreeItem::Type GroupTreeItem::itemType() const
{
    return type_;
}

//--------------------------------------------------------------------------------------------------
qint64 GroupTreeItem::groupId() const
{
    return group_id_;
}

//--------------------------------------------------------------------------------------------------
LocalGroupItem::LocalGroupItem(qint64 group_id, const QString& name, QTreeWidget* parent)
    : GroupTreeItem(Type::LOCAL_GROUP, group_id, parent)
{
    setText(0, name);
    setIcon(0, QIcon(":/img/folder.svg"));
}

//--------------------------------------------------------------------------------------------------
LocalGroupItem::LocalGroupItem(qint64 group_id, const QString& name, QTreeWidgetItem* parent)
    : GroupTreeItem(Type::LOCAL_GROUP, group_id, parent)
{
    setText(0, name);
    setIcon(0, QIcon(":/img/folder.svg"));
}

//--------------------------------------------------------------------------------------------------
RouterItem::RouterItem(const QString& name, QTreeWidget* parent)
    : GroupTreeItem(Type::ROUTER, -1, parent)
{
    setText(0, name);
    setIcon(0, QIcon(":/img/stack.svg"));
}

//--------------------------------------------------------------------------------------------------
RouterGroupItem::RouterGroupItem(qint64 group_id, const QString& name, QTreeWidgetItem* parent)
    : GroupTreeItem(Type::ROUTER_GROUP, group_id, parent)
{
    setText(0, name);
    setIcon(0, QIcon(":/img/folder.svg"));
}

} // namespace client
