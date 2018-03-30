//
// PROJECT:         Aspia
// FILE:            console/computer_group_tree.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/computer_group_tree.h"

#include <QDropEvent>
#include <QApplication>
#include <QMessageBox>

namespace aspia {

ComputerGroupTree::ComputerGroupTree(QWidget* parent)
    : QTreeWidget(parent)
{
    QTreeWidgetItem* invisible_root = invisibleRootItem();
    invisible_root->setFlags(invisible_root->flags() ^ Qt::ItemIsDropEnabled);
    setAcceptDrops(true);
}

ComputerGroupTree::~ComputerGroupTree() = default;

void ComputerGroupTree::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        start_pos_ = event->pos();

    QTreeWidget::mousePressEvent(event);
}

void ComputerGroupTree::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        int distance = (event->pos() - start_pos_).manhattanLength();

        if (distance > QApplication::startDragDistance())
        {
            startDrag();
            return;
        }
    }

    QTreeWidget::mouseMoveEvent(event);
}

void ComputerGroupTree::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(ComputerGroupMimeData::mimeType()))
        event->acceptProposedAction();
}

void ComputerGroupTree::dragMoveEvent(QDragMoveEvent* event)
{
    event->ignore();

    ComputerGroup* source = reinterpret_cast<ComputerGroup*>(itemAt(start_pos_));
    ComputerGroup* target = reinterpret_cast<ComputerGroup*>(itemAt(event->pos()));

    if (isAllowedDropTarget(target, source))
        event->acceptProposedAction();

    QWidget::dragMoveEvent(event);
}

void ComputerGroupTree::dropEvent(QDropEvent* event)
{
    const ComputerGroupMimeData* mime_data =
        reinterpret_cast<const ComputerGroupMimeData*>(event->mimeData());

    if (!mime_data)
        return;

    ComputerGroup* target_group = reinterpret_cast<ComputerGroup*>(itemAt(event->pos()));
    ComputerGroup* source_group = mime_data->computerGroup();

    if (!isAllowedDropTarget(target_group, source_group))
        return;

    ComputerGroup* parent_group = source_group->ParentComputerGroup();

    source_group = parent_group->TakeChildComputerGroup(source_group);
    if (!source_group)
        return;

    target_group->AddChildComputerGroup(source_group);
    target_group->setExpanded(true);
    setCurrentItem(source_group);
}

bool ComputerGroupTree::isAllowedDropTarget(ComputerGroup* target, ComputerGroup* item)
{
    if (!target || !item)
        return false;

    if (target == invisibleRootItem() || target == item)
        return false;

    std::function<bool(ComputerGroup*, ComputerGroup*)> is_child_item =
        [&](ComputerGroup* parent_item, ComputerGroup* child_item)
    {
        for (int i = 0; i < parent_item->childCount(); ++i)
        {
            ComputerGroup* current = reinterpret_cast<ComputerGroup*>(parent_item->child(i));

            if (current == child_item)
                return true;

            if (is_child_item(current, child_item))
                return true;
        }

        return false;
    };

    return !is_child_item(item, target);
}

void ComputerGroupTree::startDrag()
{
    ComputerGroup* computer_group = reinterpret_cast<ComputerGroup*>(itemAt(start_pos_));
    if (computer_group)
    {
        ComputerGroupDrag* drag = new ComputerGroupDrag(this);

        drag->setComputerGroup(computer_group);

        QIcon icon = computer_group->icon(0);
        drag->setPixmap(icon.pixmap(icon.actualSize(QSize(16, 16))));

        drag->exec(Qt::MoveAction);
    }
}

} // namespace aspia
