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

#include "client/ui/hosts/group_combo_box.h"

#include "client/database.h"

#include <QStandardItemModel>

namespace client {

//--------------------------------------------------------------------------------------------------
GroupComboBox::GroupComboBox(QWidget* parent)
    : QComboBox(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void GroupComboBox::loadGroups(const QString& root_name, qint64 exclude_id)
{
    QStandardItemModel* model = new QStandardItemModel(this);

    QStandardItem* root = new QStandardItem(QIcon(":/img/folder.svg"), root_name);
    root->setData(static_cast<qint64>(0), kGroupIdRole);
    model->appendRow(root);

    addGroupItems(0, root, exclude_id);

    QTreeView* tree_view = new QTreeView(this);
    tree_view->setHeaderHidden(true);

    setModel(model);
    setView(tree_view);

    tree_view->expandAll();
}

//--------------------------------------------------------------------------------------------------
void GroupComboBox::selectGroup(qint64 group_id)
{
    QAbstractItemModel* m = model();

    QModelIndexList matches = m->match(
        m->index(0, 0), kGroupIdRole, QVariant::fromValue(group_id), 1,
        Qt::MatchExactly | Qt::MatchRecursive);

    if (!matches.isEmpty())
    {
        QModelIndex index = matches.first();
        setRootModelIndex(index.parent());
        setCurrentIndex(index.row());
    }
}

//--------------------------------------------------------------------------------------------------
qint64 GroupComboBox::currentGroupId() const
{
    return currentData(kGroupIdRole).toLongLong();
}

//--------------------------------------------------------------------------------------------------
void GroupComboBox::showPopup()
{
    setRootModelIndex(QModelIndex());
    QComboBox::showPopup();

    if (QTreeView* tv = qobject_cast<QTreeView*>(view()))
        tv->expandAll();
}

//--------------------------------------------------------------------------------------------------
void GroupComboBox::addGroupItems(qint64 parent_id, QStandardItem* parent_item, qint64 exclude_id)
{
    QIcon folder_icon(":/img/folder.svg");
    QList<GroupConfig> groups = Database::instance().groupList(parent_id);

    for (const GroupConfig& group : std::as_const(groups))
    {
        if (group.id == exclude_id)
            continue;

        QStandardItem* item = new QStandardItem(folder_icon, group.name);
        item->setData(group.id, kGroupIdRole);
        parent_item->appendRow(item);

        addGroupItems(group.id, item, exclude_id);
    }
}

} // namespace client
