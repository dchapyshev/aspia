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

#include "console/computer_group_item.h"

#include <QApplication>

namespace console {

ComputerGroupItem::ComputerGroupItem(proto::address_book::ComputerGroup* computer_group,
                                     ComputerGroupItem* parent_item)
    : QTreeWidgetItem(parent_item),
      computer_group_(computer_group)
{
    setIcon(0, QIcon(":/img/folder.png"));
    updateItem();

    for (int i = 0; i < computer_group_->computer_group_size(); ++i)
    {
        addChild(new ComputerGroupItem(computer_group_->mutable_computer_group(i), this));
    }
}

ComputerGroupItem* ComputerGroupItem::addChildComputerGroup(
    proto::address_book::ComputerGroup* computer_group)
{
    computer_group_->mutable_computer_group()->AddAllocated(computer_group);

    ComputerGroupItem* item = new ComputerGroupItem(computer_group, this);
    addChild(item);
    return item;
}

bool ComputerGroupItem::deleteChildComputerGroup(ComputerGroupItem* computer_group_item)
{
    for (int i = 0; i < computer_group_->computer_group_size(); ++i)
    {
        if (computer_group_->mutable_computer_group(i) == computer_group_item->computerGroup())
        {
            computer_group_->mutable_computer_group()->DeleteSubrange(i, 1);
            removeChild(computer_group_item);
            return true;
        }
    }

    return false;
}

proto::address_book::ComputerGroup* ComputerGroupItem::takeChildComputerGroup(
    ComputerGroupItem* computer_group_item)
{
    proto::address_book::ComputerGroup* computer_group = nullptr;

    for (int i = 0; i < computer_group_->computer_group_size(); ++i)
    {
        if (computer_group_->mutable_computer_group(i) == computer_group_item->computerGroup())
        {
            computer_group = new proto::address_book::ComputerGroup();
            *computer_group = std::move(*computer_group_->mutable_computer_group(i));
            computer_group_->mutable_computer_group()->DeleteSubrange(i, 1);
            break;
        }
    }

    if (computer_group)
        removeChild(computer_group_item);

    return computer_group;
}

void ComputerGroupItem::addChildComputer(proto::address_book::Computer* computer)
{
    computer_group_->mutable_computer()->AddAllocated(computer);
}

bool ComputerGroupItem::deleteChildComputer(proto::address_book::Computer* computer)
{
    for (int i = 0; i < computer_group_->computer_size(); ++i)
    {
        if (computer_group_->mutable_computer(i) == computer)
        {
            computer_group_->mutable_computer()->DeleteSubrange(i, 1);
            return true;
        }
    }

    return false;
}

proto::address_book::Computer* ComputerGroupItem::takeChildComputer(
    proto::address_book::Computer* computer)
{
    for (int i = 0; i < computer_group_->computer_size(); ++i)
    {
        if (computer_group_->mutable_computer(i) == computer)
        {
            proto::address_book::Computer* new_computer = new proto::address_book::Computer();
            *new_computer = std::move(*computer_group_->mutable_computer(i));

            computer_group_->mutable_computer()->DeleteSubrange(i, 1);

            return new_computer;
        }
    }

    return nullptr;
}

void ComputerGroupItem::updateItem()
{
    bool has_parent = parent() != nullptr;
    if (has_parent)
        setText(0, QString::fromStdString(computer_group_->name()));
    else
        setText(0, QApplication::translate("ComputerGroupItem", "Root Group"));
}

bool ComputerGroupItem::IsExpanded() const
{
    return computer_group_->expanded();
}

void ComputerGroupItem::SetExpanded(bool expanded)
{
    computer_group_->set_expanded(expanded);
}

QList<QTreeWidgetItem*> ComputerGroupItem::ComputerList()
{
    QList<QTreeWidgetItem*> list;

    for (int i = 0; i < computer_group_->computer_size(); ++i)
        list.push_back(new ComputerItem(computer_group_->mutable_computer(i), this));

    return list;
}

} // namespace console
