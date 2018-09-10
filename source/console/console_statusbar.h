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

#ifndef ASPIA_CONSOLE__CONSOLE_STATUSBAR_H_
#define ASPIA_CONSOLE__CONSOLE_STATUSBAR_H_

#include <QStatusBar>

#include "base/macros_magic.h"
#include "protocol/address_book.pb.h"

namespace aspia {

class ConsoleStatusBar : public QStatusBar
{
    Q_OBJECT

public:
    explicit ConsoleStatusBar(QWidget* parent);
    ~ConsoleStatusBar() = default;

    void setCurrentComputerGroup(const proto::address_book::ComputerGroup& computer_group);
    void clear();

private:
    DISALLOW_COPY_AND_ASSIGN(ConsoleStatusBar);
};

} // namespace aspia

#endif // ASPIA_CONSOLE__CONSOLE_STATUSBAR_H_
