//
// PROJECT:         Aspia
// FILE:            console/address_book.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/address_book.h"

namespace aspia {

AddressBook::AddressBook(proto::ComputerGroup* root_group)
    : ComputerGroup(QIcon(QStringLiteral(":/icon/book.png")), root_group, nullptr)
{
    setFlags(flags() ^ Qt::ItemIsDragEnabled);
}

// static
std::unique_ptr<AddressBook> AddressBook::Create()
{
    return std::unique_ptr<AddressBook>(new AddressBook(new proto::ComputerGroup()));
}

// static
std::unique_ptr<AddressBook> AddressBook::Open(const std::string& buffer)
{
    std::unique_ptr<proto::ComputerGroup> computer_group =
        std::make_unique<proto::ComputerGroup>();

    if (!computer_group->ParseFromString(buffer))
        return nullptr;

    return std::unique_ptr<AddressBook>(new AddressBook(computer_group.release()));
}

std::string AddressBook::Serialize() const
{
    return group_->SerializeAsString();
}

void AddressBook::RestoreAppearance()
{
    if (group_->expanded())
        setExpanded(true);

    std::function<void(ComputerGroup*)> restore_child = [&](ComputerGroup* item)
    {
        for (int i = 0; i < item->childCount(); ++i)
        {
            ComputerGroup* child_item = reinterpret_cast<ComputerGroup*>(item->child(i));

            if (child_item->IsExpanded())
                child_item->setExpanded(true);

            restore_child(child_item);
        }
    };

    restore_child(this);
}

} // namespace aspia
