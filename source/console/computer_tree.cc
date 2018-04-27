//
// PROJECT:         Aspia
// FILE:            console/computer_tree.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
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

ComputerTree::~ComputerTree() = default;

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
