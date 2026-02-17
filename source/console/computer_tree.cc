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

#include "console/computer_tree.h"

#include <QApplication>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QUuid>

#include "console/computer_drag.h"
#include "console/computer_item.h"

namespace console {

namespace {

class ColumnAction : public QAction
{
public:
    ColumnAction(const QString& text, int index, QObject* parent)
        : QAction(text, parent),
          index_(index)
    {
        setCheckable(true);
    }

    int columnIndex() const { return index_; }

private:
    const int index_;
    Q_DISABLE_COPY(ColumnAction)
};

} // namespace

//--------------------------------------------------------------------------------------------------
ComputerTree::ComputerTree(QWidget* parent)
    : QTreeWidget(parent),
      mime_type_(QString("application/%1").arg(QUuid::createUuid().toString()))
{
    header()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(header(), &QHeaderView::customContextMenuRequested,
            this, &ComputerTree::onHeaderContextMenu);
}

//--------------------------------------------------------------------------------------------------
void ComputerTree::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        start_pos_ = event->pos();

    QTreeWidget::mousePressEvent(event);
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
void ComputerTree::dragEnterEvent(QDragEnterEvent* /* event */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void ComputerTree::dragMoveEvent(QDragMoveEvent* event)
{
    event->ignore();
    QWidget::dragMoveEvent(event);
}

//--------------------------------------------------------------------------------------------------
void ComputerTree::dropEvent(QDropEvent* /* event */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void ComputerTree::startDrag(Qt::DropActions supported_actions)
{
    ComputerItem* computer_item = reinterpret_cast<ComputerItem*>(itemAt(start_pos_));
    if (computer_item)
    {
        ComputerDrag* drag = new ComputerDrag(this);

        drag->setComputerItem(computer_item, mime_type_);

        QIcon icon = computer_item->icon(0);
        drag->setPixmap(icon.pixmap(icon.actualSize(QSize(16, 16))));

        drag->exec(supported_actions);
    }
}

//--------------------------------------------------------------------------------------------------
void ComputerTree::onHeaderContextMenu(const QPoint& pos)
{
    QMenu menu;

    for (int i = 1; i < header()->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(headerItem()->text(i), i, &menu);
        action->setChecked(!header()->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(
        menu.exec(header()->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header()->setSectionHidden(action->columnIndex(), !action->isChecked());
}

} // namespace console
