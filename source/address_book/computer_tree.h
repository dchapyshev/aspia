//
// PROJECT:         Aspia
// FILE:            address_book/computer_tree.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_ADDRESS_BOOK__COMPUTER_TREE_H
#define _ASPIA_ADDRESS_BOOK__COMPUTER_TREE_H

#include <QtWidgets/QTreeWidget>

class ComputerTree : public QTreeWidget
{
    Q_OBJECT

public:
    ComputerTree(QWidget *parent);
    ~ComputerTree() = default;
};

#endif // _ASPIA_ADDRESS_BOOK__COMPUTER_TREE_H
