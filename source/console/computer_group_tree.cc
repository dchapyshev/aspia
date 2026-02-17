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

#include "console/computer_group_tree.h"

#include <QApplication>
#include <QDropEvent>
#include <QUuid>

#include "console/computer_mime_data.h"

namespace console {

//--------------------------------------------------------------------------------------------------
ComputerGroupTree::ComputerGroupTree(QWidget* parent)
    : QTreeWidget(parent),
      computer_group_mime_type_(QString("application/%1").arg(QUuid::createUuid().toString()))
{
    QTreeWidgetItem* invisible_root = invisibleRootItem();
    invisible_root->setFlags(invisible_root->flags() ^ Qt::ItemIsDropEnabled);
    setAcceptDrops(true);
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupTree::setComputerMimeType(const QString& mime_type)
{
    computer_mime_type_ = mime_type;
}

//--------------------------------------------------------------------------------------------------
bool ComputerGroupTree::dragging() const
{
    return dragging_;
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupTree::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        start_pos_ = event->pos();

    QTreeWidget::mousePressEvent(event);
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupTree::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        int distance = (event->pos() - start_pos_).manhattanLength();

        if (distance > QApplication::startDragDistance())
        {
            startDrag(Qt::MoveAction);
            return;
        }
    }

    QTreeWidget::mouseMoveEvent(event);
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupTree::dragEnterEvent(QDragEnterEvent* event)
{
    const QMimeData* mime_data = event->mimeData();

    if (mime_data->hasFormat(computer_group_mime_type_) ||
        mime_data->hasFormat(computer_mime_type_))
    {
        event->acceptProposedAction();
    }

    dragging_ = true;
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupTree::dragLeaveEvent(QDragLeaveEvent* event)
{
    dragging_ = false;
    QTreeWidget::dragLeaveEvent(event);
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupTree::dragMoveEvent(QDragMoveEvent* event)
{
    event->ignore();

    const QMimeData* mime_data = event->mimeData();

    if (mime_data->hasFormat(computer_group_mime_type_))
    {
        ComputerGroupItem* source_item = dynamic_cast<ComputerGroupItem*>(itemAt(start_pos_));
        ComputerGroupItem* target_item = dynamic_cast<ComputerGroupItem*>(itemAt(event->position().toPoint()));

        if (isAllowedDropTarget(target_item, source_item))
        {
            clearSelection();

            target_item->setSelected(true);
            event->acceptProposedAction();
        }
    }
    else if (mime_data->hasFormat(computer_mime_type_))
    {
        ComputerGroupItem* target_item = dynamic_cast<ComputerGroupItem*>(itemAt(event->position().toPoint()));
        if (target_item)
        {
            const ComputerMimeData* computer_mime_data =
                dynamic_cast<const ComputerMimeData*>(mime_data);
            if (computer_mime_data)
            {
                ComputerItem* computer_item = computer_mime_data->computerItem();
                if (computer_item && computer_item->parentComputerGroupItem() != target_item)
                {
                    clearSelection();

                    target_item->setSelected(true);
                    event->acceptProposedAction();
                }
            }
        }
    }
    else
    {
        // Ignore other.
    }

    QWidget::dragMoveEvent(event);
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupTree::dropEvent(QDropEvent* event)
{
    const QMimeData* mime_data = event->mimeData();
    if (!mime_data)
        return;

    if (mime_data->hasFormat(computer_group_mime_type_))
    {
        const ComputerGroupMimeData* computer_group_mime_data =
            dynamic_cast<const ComputerGroupMimeData*>(event->mimeData());
        if (computer_group_mime_data)
        {
            ComputerGroupItem* target_group_item =
                dynamic_cast<ComputerGroupItem*>(itemAt(event->position().toPoint()));
            if (!target_group_item)
                return;

            ComputerGroupItem* source_group_item =
                computer_group_mime_data->computerGroup();

            if (!isAllowedDropTarget(target_group_item, source_group_item))
                return;

            ComputerGroupItem* parent_group_item =
                dynamic_cast<ComputerGroupItem*>(source_group_item->parent());
            if (!parent_group_item)
                return;

            proto::address_book::ComputerGroup* computer_group =
                parent_group_item->takeChildComputerGroup(source_group_item);
            if (!computer_group)
                return;

            // After calling takeChildComputerGroup, instance source_group_item is destroyed.
            // We create a new instance with the same data in a new location.
            source_group_item = target_group_item->addChildComputerGroup(computer_group);
            target_group_item->setExpanded(true);

            setCurrentItem(source_group_item);
            emit sig_itemDropped();
        }
    }
    else if (mime_data->hasFormat(computer_mime_type_))
    {
        const ComputerMimeData* computer_mime_data =
            dynamic_cast<const ComputerMimeData*>(event->mimeData());
        if (computer_mime_data)
        {
            ComputerItem* computer_item = computer_mime_data->computerItem();

            ComputerGroupItem* target_group_item =
                dynamic_cast<ComputerGroupItem*>(itemAt(event->position().toPoint()));
            ComputerGroupItem* source_group_item =
                computer_item->parentComputerGroupItem();

            if (target_group_item && source_group_item)
            {
                setCurrentItem(source_group_item);

                proto::address_book::Computer* computer =
                    source_group_item->takeChildComputer(computer_item->computer());

                if (computer)
                {
                    target_group_item->addChildComputer(computer);
                    emit sig_itemDropped();
                }
            }
        }
    }

    dragging_ = false;
}

//--------------------------------------------------------------------------------------------------
bool ComputerGroupTree::isAllowedDropTarget(ComputerGroupItem* target, ComputerGroupItem* item)
{
    if (!target || !item)
        return false;

    if (target == invisibleRootItem() || target == item)
        return false;

    std::function<bool(ComputerGroupItem*, ComputerGroupItem*)> is_child_item =
        [&](ComputerGroupItem* parent_item, ComputerGroupItem* child_item)
    {
        int count = parent_item->childCount();
        for (int i = 0; i < count; ++i)
        {
            ComputerGroupItem* current = dynamic_cast<ComputerGroupItem*>(parent_item->child(i));
            if (current)
            {
                if (current == child_item)
                    return true;

                if (is_child_item(current, child_item))
                    return true;
            }
        }

        return false;
    };

    return !is_child_item(item, target);
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupTree::startDrag(Qt::DropActions supported_actions)
{
    ComputerGroupItem* computer_group = dynamic_cast<ComputerGroupItem*>(itemAt(start_pos_));
    if (computer_group)
    {
        ComputerGroupDrag* drag = new ComputerGroupDrag(this);

        drag->setComputerGroupItem(computer_group, computer_group_mime_type_);

        QIcon icon = computer_group->icon(0);
        drag->setPixmap(icon.pixmap(icon.actualSize(QSize(16, 16))));

        drag->exec(supported_actions);
    }
}

} // namespace console
