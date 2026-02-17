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

#include "console/computer_item.h"

#include <QCollator>
#include <QDateTime>

#include "base/net/address.h"
#include "build/build_config.h"
#include "console/computer_group_item.h"

namespace console {

//--------------------------------------------------------------------------------------------------
ComputerItem::ComputerItem(proto::address_book::Computer* computer,
                           ComputerGroupItem* parent_group_item)
    : computer_(computer),
      parent_group_item_(parent_group_item)
{
    static int computer_id = 0;
    computer_id_ = computer_id;
    ++computer_id;

    setIcon(0, QIcon(":/img/computer.svg"));
    updateItem();
}

//--------------------------------------------------------------------------------------------------
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
        address.setHost(QString::fromStdString(computer_->address()));
        address.setPort(static_cast<quint16>(computer_->port()));

        address_title = address.toString();
    }

    setText(COLUMN_INDEX_NAME, QString::fromStdString(computer_->name()));
    setText(COLUMN_INDEX_ADDRESS, address_title);
    setText(COLUMN_INDEX_COMMENT, QString::fromStdString(
        computer_->comment()).replace(QLatin1Char('\n'), QLatin1Char(' ')));

    QLocale system_locale = QLocale::system();

    QString create_time = system_locale.toString(
        QDateTime::fromSecsSinceEpoch(computer_->create_time()), QLocale::ShortFormat);
    setText(COLUMN_INDEX_CREATED, create_time);

    QString modify_time = system_locale.toString(
        QDateTime::fromSecsSinceEpoch(computer_->modify_time()), QLocale::ShortFormat);
    setText(COLUMN_INDEX_MODIFIED, modify_time);
}

//--------------------------------------------------------------------------------------------------
proto::address_book::Computer ComputerItem::computerToConnect()
{
    proto::address_book::Computer computer(*computer_);

    if (!computer.has_inherit())
    {
        proto::address_book::InheritConfig* inherit = computer.mutable_inherit();
        inherit->set_credentials(false);
        inherit->set_desktop_manage(false);
        inherit->set_desktop_view(false);
    }

    ComputerGroupItem* group_item = parentComputerGroupItem();
    if (group_item)
    {
        proto::address_book::ComputerGroupConfig default_config = group_item->defaultConfig();
        const proto::address_book::InheritConfig& inherit = computer.inherit();

        if (inherit.credentials())
        {
            computer.set_username(default_config.username());
            computer.set_password(default_config.password());
        }

        if (inherit.desktop_manage())
        {
            computer.mutable_session_config()->mutable_desktop_manage()->CopyFrom(
                default_config.session_config().desktop_manage());
        }

        if (inherit.desktop_view())
        {
            computer.mutable_session_config()->mutable_desktop_view()->CopyFrom(
                default_config.session_config().desktop_view());
        }
    }

    return computer;
}

//--------------------------------------------------------------------------------------------------
ComputerGroupItem* ComputerItem::parentComputerGroupItem()
{
    return parent_group_item_;
}

//--------------------------------------------------------------------------------------------------
bool ComputerItem::operator<(const QTreeWidgetItem& other) const
{
    switch (treeWidget()->sortColumn())
    {
        case COLUMN_INDEX_NAME:
        {
            QString this_computer_name = text(COLUMN_INDEX_NAME);
            QString other_computer_name = other.text(COLUMN_INDEX_NAME);

            QCollator collator;
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            collator.setNumericMode(true);

            return collator.compare(this_computer_name, other_computer_name) <= 0;
        }
        break;

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
