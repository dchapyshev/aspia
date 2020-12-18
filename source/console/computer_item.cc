//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/net/address.h"
#include "base/strings/unicode.h"
#include "console/computer_group_item.h"

#include <QDateTime>

namespace console {

ComputerItem::ComputerItem(proto::address_book::Computer* computer,
                           ComputerGroupItem* parent_group_item)
    : computer_(computer),
      parent_group_item_(parent_group_item)
{
    setIcon(0, QIcon(":/img/computer.png"));
    updateItem();
}

void ComputerItem::updateItem()
{
    QString address_title = QString::fromStdString(computer_->address());
    bool host_id_entered = true;

    for (int i = 0; i < address_title.length(); ++i)
    {
        if (!address_title[i].isDigit())
        {
            host_id_entered = false;
            break;
        }
    }

    if (!host_id_entered)
    {
        base::Address address(DEFAULT_HOST_TCP_PORT);
        address.setHost(base::utf16FromUtf8(computer_->address()));
        address.setPort(computer_->port());

        address_title = QString::fromStdU16String(address.toString());
    }

    setText(COLUMN_INDEX_NAME, QString::fromStdString(computer_->name()));
    setText(COLUMN_INDEX_ADDRESS, address_title);
    setText(COLUMN_INDEX_COMMENT, QString::fromStdString(computer_->comment()).replace('\n', ' '));

    QLocale system_locale = QLocale::system();

    QString create_time = system_locale.toString(
        QDateTime::fromSecsSinceEpoch(computer_->create_time()), QLocale::ShortFormat);
    setText(COLUMN_INDEX_CREATED, create_time);

    QString modify_time = system_locale.toString(
        QDateTime::fromSecsSinceEpoch(computer_->modify_time()), QLocale::ShortFormat);
    setText(COLUMN_INDEX_MODIFIED, modify_time);
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
