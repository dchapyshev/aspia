//
// PROJECT:         Aspia
// FILE:            ui/address_book/address_book_secure_util.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/address_book/address_book_secure_util.h"

#include "base/logging.h"
#include "crypto/secure_memory.h"

namespace aspia {

void SecureClearComputer(proto::Computer* computer)
{
    DCHECK(computer);

    SecureMemZero(computer->mutable_name());
    SecureMemZero(computer->mutable_address());
    SecureMemZero(computer->mutable_comment());
    SecureMemZero(computer->mutable_username());

    computer->Clear();
}

void SecureClearComputerGroup(proto::ComputerGroup* computer_group)
{
    DCHECK(computer_group);

    for (int i = 0; i < computer_group->computer_size(); ++i)
    {
        SecureClearComputer(computer_group->mutable_computer(i));
    }

    for (int i = 0; i < computer_group->group_size(); ++i)
    {
        SecureClearComputerGroup(computer_group->mutable_group(i));
    }

    SecureMemZero(computer_group->mutable_name());
    SecureMemZero(computer_group->mutable_comment());

    computer_group->Clear();
}

} // namespace aspia
