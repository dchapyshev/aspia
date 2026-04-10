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

#include "client/ui/hosts/content_tree_item.h"

#include "client/local_data.h"

#include <QIcon>

namespace client {

namespace {

const int kColumnName = 0;
const int kColumnAddress = 1;
const int kColumnComment = 2;

} // namespace

//--------------------------------------------------------------------------------------------------
ContentTreeItem::ContentTreeItem(Type type, const ComputerData& computer, QTreeWidget* parent)
    : QTreeWidgetItem(parent),
      type_(type),
      computer_id_(computer.id),
      group_id_(computer.group_id),
      computer_name_(computer.name)
{
    setText(kColumnName, computer.name);
    setText(kColumnAddress, computer.address);
    setText(kColumnComment, computer.comment);
    setIcon(kColumnName, QIcon(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
ContentTreeItem::Type ContentTreeItem::itemType() const
{
    return type_;
}

//--------------------------------------------------------------------------------------------------
qint64 ContentTreeItem::computerId() const
{
    return computer_id_;
}

//--------------------------------------------------------------------------------------------------
qint64 ContentTreeItem::groupId() const
{
    return group_id_;
}

//--------------------------------------------------------------------------------------------------
QString ContentTreeItem::computerName() const
{
    return computer_name_;
}

//--------------------------------------------------------------------------------------------------
LocalComputerItem::LocalComputerItem(const ComputerData& computer, QTreeWidget* parent)
    : ContentTreeItem(Type::LOCAL_COMPUTER, computer, parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
RemoteComputerItem::RemoteComputerItem(const ComputerData& computer, QTreeWidget* parent)
    : ContentTreeItem(Type::REMOTE_COMPUTER, computer, parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SearchComputerItem::SearchComputerItem(const ComputerData& computer, QTreeWidget* parent)
    : ContentTreeItem(Type::SEARCH_COMPUTER, computer, parent)
{
    // Nothing
}

} // namespace client
