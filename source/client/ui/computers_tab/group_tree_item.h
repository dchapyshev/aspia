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

#ifndef CLIENT_UI_COMPUTERS_TAB_GROUP_TREE_ITEM_H
#define CLIENT_UI_COMPUTERS_TAB_GROUP_TREE_ITEM_H

#include <QTreeWidgetItem>

namespace client {

class GroupTreeItem : public QTreeWidgetItem
{
public:
    enum class Type { LOCAL_GROUP, ROUTER, ROUTER_GROUP };

    Type itemType() const;
    qint64 groupId() const;

protected:
    GroupTreeItem(Type type, qint64 group_id, QTreeWidget* parent);
    GroupTreeItem(Type type, qint64 group_id, QTreeWidgetItem* parent);

private:
    Type type_;
    qint64 group_id_;
};

class LocalGroupItem : public GroupTreeItem
{
public:
    LocalGroupItem(qint64 group_id, const QString& name, QTreeWidget* parent);
    LocalGroupItem(qint64 group_id, const QString& name, QTreeWidgetItem* parent);
};

class RouterItem : public GroupTreeItem
{
public:
    explicit RouterItem(const QString& name, QTreeWidget* parent);
};

class RouterGroupItem : public GroupTreeItem
{
public:
    RouterGroupItem(qint64 group_id, const QString& name, QTreeWidgetItem* parent);
};

} // namespace client

#endif // CLIENT_UI_COMPUTERS_TAB_GROUP_TREE_ITEM_H
