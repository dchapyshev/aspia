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

#include "client/desktop/hosts/group_combo_box.h"

#include <functional>

#include <QHash>
#include <QStandardItemModel>

//--------------------------------------------------------------------------------------------------
GroupComboBox::GroupComboBox(QWidget* parent)
    : QComboBox(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void GroupComboBox::loadGroups(
    const QString& root_name, const QIcon& root_icon,
    const QList<Entry>& entries, qint64 exclude_id)
{
    QStandardItemModel* model = new QStandardItemModel(this);

    QStandardItem* root = new QStandardItem(root_icon, root_name);
    root->setData(static_cast<qint64>(0), kGroupIdRole);
    model->appendRow(root);

    QHash<qint64, QList<const Entry*>> children_of;
    for (const Entry& entry : entries)
        children_of[entry.parent_id].append(&entry);

    const QIcon folder_icon(":/img/folder.svg");
    std::function<void(qint64, QStandardItem*)> add =
        [&](qint64 parent_id, QStandardItem* parent_item)
    {
        const QList<const Entry*>& children = children_of.value(parent_id);
        for (const Entry* child : children)
        {
            if (child->id == exclude_id)
                continue;

            QStandardItem* item = new QStandardItem(folder_icon, child->name);
            item->setData(child->id, kGroupIdRole);
            parent_item->appendRow(item);

            add(child->id, item);
        }
    };
    add(0, root);

    QTreeView* tree_view = new QTreeView(this);
    tree_view->setHeaderHidden(true);
    tree_view->setItemsExpandable(false);
    tree_view->setRootIsDecorated(false);
    tree_view->setExpandsOnDoubleClick(false);

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
