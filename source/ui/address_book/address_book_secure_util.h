//
// PROJECT:         Aspia
// FILE:            ui/address_book/address_book_secure_util.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__ADDRESS_BOOK__ADDRESS_BOOK_SECURE_UTIL_H
#define _ASPIA_UI__ADDRESS_BOOK__ADDRESS_BOOK_SECURE_UTIL_H

#include "proto/address_book.pb.h"

namespace aspia {

void SecureClearComputer(proto::Computer* computer);
void SecureClearComputerGroup(proto::ComputerGroup* computer_group);

} // namespace aspia

#endif // _ASPIA_UI__ADDRESS_BOOK__ADDRESS_BOOK_SECURE_UTIL_H
