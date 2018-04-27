//
// PROJECT:         Aspia
// FILE:            console/computer_group_tree.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__COMPUTER_GROUP_TREE_H
#define _ASPIA_CONSOLE__COMPUTER_GROUP_TREE_H

#include <QTreeWidget>

#include "console/computer_group_drag.h"

namespace aspia {

class ComputerGroupTree : public QTreeWidget
{
    Q_OBJECT

public:
    ComputerGroupTree(QWidget* parent);
    ~ComputerGroupTree();

signals:
    void itemDropped();

protected:
    // QTreeWidget implementation.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void startDrag(Qt::DropActions supported_actions) override;

private:
    bool isAllowedDropTarget(ComputerGroup* target, ComputerGroup* item);

    QPoint start_pos_;

    Q_DISABLE_COPY(ComputerGroupTree)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__COMPUTER_GROUP_TREE_H
