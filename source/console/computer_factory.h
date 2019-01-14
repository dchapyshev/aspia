//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CONSOLE__COMPUTER_FACTORY_H
#define ASPIA_CONSOLE__COMPUTER_FACTORY_H

#include "base/macros_magic.h"
#include "proto/address_book.pb.h"

namespace aspia {

class ComputerFactory
{
public:
    static proto::address_book::Computer defaultComputer();

private:
    DISALLOW_COPY_AND_ASSIGN(ComputerFactory);
};

} // namespace aspia

#endif // ASPIA_CONSOLE__COMPUTER_FACTORY_H
