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

#ifndef ASPIA_CONSOLE__COMPUTER_GROUP_ITEM_H_
#define ASPIA_CONSOLE__COMPUTER_GROUP_ITEM_H_

#include "console/computer_item.h"
#include "protocol/address_book.pb.h"

namespace aspia {

class ComputerGroupItem : public QTreeWidgetItem
{
public:
    ComputerGroupItem(proto::address_book::ComputerGroup* computer_group,
                      ComputerGroupItem* parent_item);
    virtual ~ComputerGroupItem() = default;

    ComputerGroupItem* addChildComputerGroup(proto::address_book::ComputerGroup* computer_group);
    bool deleteChildComputerGroup(ComputerGroupItem* computer_group_item);
    proto::address_book::ComputerGroup* takeChildComputerGroup(ComputerGroupItem* computer_group_item);
    void addChildComputer(proto::address_book::Computer* computer);
    bool deleteChildComputer(proto::address_book::Computer* computer);
    proto::address_book::Computer* takeChildComputer(proto::address_book::Computer* computer);

    void updateItem();

    bool IsExpanded() const;
    void SetExpanded(bool expanded);
    QList<QTreeWidgetItem*> ComputerList();

    proto::address_book::ComputerGroup* computerGroup() { return computer_group_; }

private:
    friend class ComputerGroupTree;

    proto::address_book::ComputerGroup* computer_group_;

    DISALLOW_COPY_AND_ASSIGN(ComputerGroupItem);
};

} // namespace aspia

#endif // ASPIA_CONSOLE__COMPUTER_GROUP_ITEM_H_
