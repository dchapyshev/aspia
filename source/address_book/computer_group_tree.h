//
// PROJECT:         Aspia
// FILE:            address_book/computer_group_tree.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_TREE_H
#define _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_TREE_H

#include <QtWidgets/QTreeWidget>

#include "address_book/computer_group_drag.h"

class ComputerGroupTree : public QTreeWidget
{
    Q_OBJECT

public:
    ComputerGroupTree(QWidget* parent);
    ~ComputerGroupTree() = default;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    bool isAllowedDropTarget(ComputerGroup* target, ComputerGroup* item);
    void startDrag();

    QPoint start_pos_;
};

#endif // _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_TREE_H
