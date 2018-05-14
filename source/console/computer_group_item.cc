//
// PROJECT:         Aspia
// FILE:            console/computer_group_item.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/computer_group_item.h"

namespace aspia {

ComputerGroupItem::ComputerGroupItem(proto::address_book::ComputerGroup* computer_group,
                                     ComputerGroupItem* parent_item)
    : QTreeWidgetItem(parent_item),
      computer_group_(computer_group)
{
    setIcon(0, QIcon(QStringLiteral(":/icon/folder.png")));
    updateItem();

    for (int i = 0; i < computer_group_->computer_group_size(); ++i)
    {
        addChild(new ComputerGroupItem(computer_group_->mutable_computer_group(i), this));
    }
}

ComputerGroupItem::~ComputerGroupItem() = default;

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
            proto::address_book::Computer* computer = new proto::address_book::Computer();
            *computer = std::move(*computer_group_->mutable_computer(i));

            computer_group_->mutable_computer()->DeleteSubrange(i, 1);

            return computer;
        }
    }

    return nullptr;
}

void ComputerGroupItem::updateItem()
{
    setText(0, QString::fromStdString(computer_group_->name()));
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

} // namespace aspia
