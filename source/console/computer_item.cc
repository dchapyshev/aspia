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

#include <QDateTime>

#include "build/build_config.h"
#include "console/computer_group_item.h"
#include "net/address.h"

namespace console {

ComputerItem::ComputerItem(proto::address_book::Computer* computer,
                           ComputerGroupItem* parent_group_item)
    : computer_(computer),
      parent_group_item_(parent_group_item)
{
    setIcon(0, QIcon(QStringLiteral(":/img/computer.png")));
    updateItem();
}

void ComputerItem::updateItem()
{
    net::Address address;
    address.setHost(QString::fromStdString(computer_->address()));
    address.setPort(computer_->port());

    setText(COLUMN_INDEX_NAME, QString::fromStdString(computer_->name()));
    setText(COLUMN_INDEX_ADDRESS, address.toString());
    setText(COLUMN_INDEX_COMMENT, QString::fromStdString(computer_->comment()).replace('\n', ' '));

    setText(COLUMN_INDEX_CREATED, QDateTime::fromSecsSinceEpoch(
        computer_->create_time()).toString(Qt::DefaultLocaleShortDate));

    setText(COLUMN_INDEX_MODIFIED, QDateTime::fromSecsSinceEpoch(
        computer_->modify_time()).toString(Qt::DefaultLocaleShortDate));
}

ComputerGroupItem* ComputerItem::parentComputerGroupItem()
{
    return parent_group_item_;
}

bool ComputerItem::operator<(const QTreeWidgetItem &other) const
{
    switch (treeWidget()->sortColumn())
    {
        case COLUMN_INDEX_CREATED:
        {
            const ComputerItem* other_item = dynamic_cast<const ComputerItem*>(&other);
            if (other_item)
                return computer_->create_time() < other_item->computer_->create_time();
        }
        break;

        case COLUMN_INDEX_MODIFIED:
        {
            const ComputerItem* other_item = dynamic_cast<const ComputerItem*>(&other);
            if (other_item)
                return computer_->modify_time() < other_item->computer_->modify_time();
        }
        break;

        default:
            break;
    }

    return QTreeWidgetItem::operator<(other);
}

} // namespace console
