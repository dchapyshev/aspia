//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "console/computer_tree.h"

#include <QApplication>
#include <QMouseEvent>

#include "console/computer_drag.h"
#include "console/computer_item.h"

namespace aspia {

ComputerTree::ComputerTree(QWidget* parent)
    : QTreeWidget(parent)
{
    // Nothing
}

void ComputerTree::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        start_pos_ = event->pos();

    QTreeWidget::mousePressEvent(event);
}

void ComputerTree::mouseMoveEvent(QMouseEvent* event)
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

void ComputerTree::dragEnterEvent(QDragEnterEvent* /* event */)
{
    // Nothing
}

void ComputerTree::dragMoveEvent(QDragMoveEvent* event)
{
    event->ignore();
    QWidget::dragMoveEvent(event);
}

void ComputerTree::dropEvent(QDropEvent* /* event */)
{
    // Nothing
}

void ComputerTree::startDrag(Qt::DropActions supported_actions)
{
    ComputerItem* computer_item = reinterpret_cast<ComputerItem*>(itemAt(start_pos_));
    if (computer_item)
    {
        ComputerDrag* drag = new ComputerDrag(this);

        drag->setComputerItem(computer_item);

        QIcon icon = computer_item->icon(0);
        drag->setPixmap(icon.pixmap(icon.actualSize(QSize(16, 16))));

        drag->exec(supported_actions);
    }
}

} // namespace aspia
