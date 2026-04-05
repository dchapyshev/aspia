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

#ifndef CONSOLE_COMPUTER_FACTORY_H
#define CONSOLE_COMPUTER_FACTORY_H

#include "proto/address_book.h"
#include "proto/desktop_control.h"

namespace console {

class ComputerFactory
{
public:
    static void setDefaultDesktopConfig(proto::address_book::DesktopConfig* config);
    static proto::address_book::Computer defaultComputer();

    static proto::control::Config toClientConfig(const proto::address_book::DesktopConfig& config);

private:
    Q_DISABLE_COPY_MOVE(ComputerFactory)
};

} // namespace console

#endif // CONSOLE_COMPUTER_FACTORY_H
