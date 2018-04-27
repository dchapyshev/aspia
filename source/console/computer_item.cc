//
// PROJECT:         Aspia
// FILE:            console/computer_item.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/computer_item.h"

#include "console/computer_group_item.h"

namespace aspia {

ComputerItem::ComputerItem(proto::Computer* computer, ComputerGroupItem* parent_group_item)
    : computer_(computer),
      parent_group_item_(parent_group_item)
{
    setIcon(0, QIcon(QStringLiteral(":/icon/computer.png")));
    updateItem();
}

ComputerItem::~ComputerItem() = default;

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
