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

#include "client/desktop/management/group_combo_box.h"

#include <functional>

#include <QHash>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyleOption>
#include <QStyledItemDelegate>

namespace {

// The marker QComboBox puts on the separators it inserts.
const char kSeparator[] = "separator";

//--------------------------------------------------------------------------------------------------
bool isSeparator(const QModelIndex& index)
{
    return index.data(Qt::AccessibleDescriptionRole).toString() == QLatin1String(kSeparator);
}

// QComboBox draws separators only in the list view of its own, so the tree view of the popup
// draws them the same way here.
class ItemDelegate final : public QStyledItemDelegate
{
public:
    ItemDelegate(QComboBox* combo_box, QObject* parent)
        : QStyledItemDelegate(parent),
          combo_box_(combo_box)
    {
        // Nothing
    }

    ~ItemDelegate() final = default;

    // QStyledItemDelegate implementation.
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const final
    {
        if (!isSeparator(index))
        {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOption separator_option;
        separator_option.rect = option.rect;
        if (const QAbstractItemView* view = qobject_cast<const QAbstractItemView*>(option.widget))
            separator_option.rect.setWidth(view->viewport()->width());

        combo_box_->style()->drawPrimitive(
            QStyle::PE_IndicatorToolBarSeparator, &separator_option, painter, combo_box_);
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const final
    {
        if (!isSeparator(index))
            return QStyledItemDelegate::sizeHint(option, index);

        const int width = combo_box_->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, combo_box_);
        return QSize(width, width);
    }

private:
    QComboBox* combo_box_ = nullptr;

    Q_DISABLE_COPY_MOVE(ItemDelegate)
};

} // namespace

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

    addGroups(root, entries, exclude_id);
    setGroupModel(model);
}

//--------------------------------------------------------------------------------------------------
void GroupComboBox::loadGroupsWithNone(const QString& none_name, const QList<Entry>& entries)
{
    QStandardItemModel* model = new QStandardItemModel(this);

    QStandardItem* none = new QStandardItem(QIcon(":/img/folder.svg"), none_name);
    none->setData(static_cast<qint64>(0), kGroupIdRole);
    model->appendRow(none);

    if (!entries.isEmpty())
    {
        QStandardItem* separator = new QStandardItem();
        separator->setData(QString::fromLatin1(kSeparator), Qt::AccessibleDescriptionRole);
        separator->setFlags(Qt::NoItemFlags);
        model->appendRow(separator);
    }

    addGroups(model->invisibleRootItem(), entries, -1);
    setGroupModel(model);
}

//--------------------------------------------------------------------------------------------------
void GroupComboBox::clearGroups()
{
    // QComboBox::clear() removes the rows of the current root only, and selectGroup() moves the root
    // down to the parent of a nested group.
    setRootModelIndex(QModelIndex());
    clear();
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
// static
void GroupComboBox::addGroups(QStandardItem* parent_item, const QList<Entry>& entries, qint64 exclude_id)
{
    QHash<qint64, QList<const Entry*>> children_of;
    for (const Entry& entry : entries)
        children_of[entry.parent_id].append(&entry);

    const QIcon folder_icon(":/img/folder.svg");
    std::function<void(qint64, QStandardItem*)> add =
        [&](qint64 parent_id, QStandardItem* parent)
    {
        const QList<const Entry*>& children = children_of.value(parent_id);
        for (const Entry* child : children)
        {
            if (child->id == exclude_id)
                continue;

            QStandardItem* item = new QStandardItem(folder_icon, child->name);
            item->setData(child->id, kGroupIdRole);
            parent->appendRow(item);

            add(child->id, item);
        }
    };
    add(0, parent_item);
}

//--------------------------------------------------------------------------------------------------
void GroupComboBox::setGroupModel(QStandardItemModel* model)
{
    QTreeView* tree_view = new QTreeView(this);
    tree_view->setHeaderHidden(true);
    tree_view->setItemsExpandable(false);
    tree_view->setRootIsDecorated(false);
    tree_view->setExpandsOnDoubleClick(false);
    tree_view->setItemDelegate(new ItemDelegate(this, tree_view));

    setModel(model);
    setView(tree_view);

    tree_view->expandAll();
}
