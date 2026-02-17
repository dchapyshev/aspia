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

#ifndef CONSOLE_COMPUTER_ITEM_H
#define CONSOLE_COMPUTER_ITEM_H

#include <QTreeWidget>

#include "proto/address_book.h"

namespace console {

class ComputerGroupItem;

class ComputerItem final : public QTreeWidgetItem
{
public:
    ComputerItem(proto::address_book::Computer* computer, ComputerGroupItem* parent_group_item);
    ~ComputerItem() final = default;

    void updateItem();

    enum ColumnIndex
    {
        COLUMN_INDEX_NAME      = 0,
        COLUMN_INDEX_ADDRESS   = 1,
        COLUMN_INDEX_COMMENT   = 2,
        COLUMN_INDEX_CREATED   = 3,
        COLUMN_INDEX_MODIFIED  = 4,
        COLUMN_INDEX_STATUS    = 5
    };

    proto::address_book::Computer* computer() { return computer_; }
    proto::address_book::Computer computerToConnect();
    int computerId() const { return computer_id_; }

    ComputerGroupItem* parentComputerGroupItem();

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem &other) const final;

private:
    friend class ComputerGroupItem;

    proto::address_book::Computer* computer_;
    ComputerGroupItem* parent_group_item_;
    int computer_id_ = 0;

    Q_DISABLE_COPY(ComputerItem)
};

} // namespace console

#endif // CONSOLE_COMPUTER_ITEM_H
