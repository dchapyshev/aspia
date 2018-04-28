//
// PROJECT:         Aspia
// FILE:            console/computer_item.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__COMPUTER_ITEM_H
#define _ASPIA_CONSOLE__COMPUTER_ITEM_H

#include <QTreeWidget>

#include "protocol/address_book.pb.h"

namespace aspia {

class ComputerGroupItem;

class ComputerItem : public QTreeWidgetItem
{
public:
    ComputerItem(proto::address_book::Computer* computer, ComputerGroupItem* parent_group_item);
    ~ComputerItem();

    void updateItem();

    proto::address_book::Computer* computer() { return computer_; }

    ComputerGroupItem* parentComputerGroupItem();

private:
    friend class ComputerGroupItem;

    proto::address_book::Computer* computer_;
    ComputerGroupItem* parent_group_item_;

    Q_DISABLE_COPY(ComputerItem)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__COMPUTER_ITEM_H
