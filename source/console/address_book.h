//
// PROJECT:         Aspia
// FILE:            console/address_book.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__ADDRESS_BOOK_H
#define _ASPIA_CONSOLE__ADDRESS_BOOK_H

#include "console/computer_group.h"

class AddressBook : public ComputerGroup
{
public:
    static std::unique_ptr<AddressBook> Create();
    static std::unique_ptr<AddressBook> Open(const std::string& buffer);

    std::string Serialize() const;
    void RestoreAppearance();

private:
    AddressBook(proto::ComputerGroup* root_group);

    DISALLOW_COPY_AND_ASSIGN(AddressBook);
};

#endif // _ASPIA_CONSOLE__ADDRESS_BOOK_H
