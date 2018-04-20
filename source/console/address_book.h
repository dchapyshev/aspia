//
// PROJECT:         Aspia
// FILE:            console/address_book.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__ADDRESS_BOOK_H
#define _ASPIA_CONSOLE__ADDRESS_BOOK_H

#include "console/computer_group.h"

namespace aspia {

class AddressBook : public ComputerGroup
{
public:
    static std::unique_ptr<AddressBook> Create();
    static std::unique_ptr<AddressBook> Open(const std::string& buffer);

    std::string Serialize() const;
    void RestoreAppearance();

private:
    explicit AddressBook(proto::ComputerGroup* root_group);

    Q_DISABLE_COPY(AddressBook)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__ADDRESS_BOOK_H
