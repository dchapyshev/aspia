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

#include "console/computer_item.h"

#include "console/computer_group_item.h"

namespace aspia {

ComputerItem::ComputerItem(proto::address_book::Computer* computer,
                           ComputerGroupItem* parent_group_item)
    : computer_(computer),
      parent_group_item_(parent_group_item)
{
    setIcon(0, QIcon(QStringLiteral(":/icon/computer.png")));
    updateItem();
}

void ComputerItem::updateItem()
{
    setText(0, QString::fromStdString(computer_->name()));
    setText(1, QString::fromStdString(computer_->address()));
    setText(2, QString::number(computer_->port()));
}

ComputerGroupItem* ComputerItem::parentComputerGroupItem()
{
    return parent_group_item_;
}

} // namespace aspia
