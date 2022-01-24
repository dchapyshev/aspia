//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CONSOLE__COMPUTER_ITEM_H
#define CONSOLE__COMPUTER_ITEM_H

#include "base/macros_magic.h"
#include "proto/address_book.pb.h"

#include <QTreeWidget>

namespace console {

class ComputerGroupItem;

class ComputerItem : public QTreeWidgetItem
{
public:
    ComputerItem(proto::address_book::Computer* computer, ComputerGroupItem* parent_group_item);
    ~ComputerItem() = default;

    void updateItem();

    enum ColumnIndex
    {
        COLUMN_INDEX_NAME      = 0,
        COLUMN_INDEX_ADDRESS   = 1,
        COLUMN_INDEX_COMMENT   = 2,
        COLUMN_INDEX_CREATED   = 3,
        COLUMN_INDEX_MODIFIED  = 4
    };

    proto::address_book::Computer* computer() { return computer_; }
    ComputerGroupItem* parentComputerGroupItem();

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem &other) const override;

private:
    friend class ComputerGroupItem;

    proto::address_book::Computer* computer_;
    ComputerGroupItem* parent_group_item_;

    DISALLOW_COPY_AND_ASSIGN(ComputerItem);
};

} // namespace console

#endif // CONSOLE__COMPUTER_ITEM_H
