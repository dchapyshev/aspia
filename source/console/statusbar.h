//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CONSOLE_STATUSBAR_H
#define CONSOLE_STATUSBAR_H

#include <QLabel>
#include <QPointer>
#include <QStatusBar>
#include <QTimer>

#include "base/macros_magic.h"
#include "proto/address_book.h"

namespace console {

class StatusBar final : public QStatusBar
{
    Q_OBJECT

public:
    explicit StatusBar(QWidget* parent);
    ~StatusBar() final = default;

    void setCurrentComputerGroup(const proto::address_book::ComputerGroup& computer_group);
    void clear();

    void setUpdateState(bool enable);

private:
    QTimer* animation_timer_;
    int animation_index_ = 0;
    QPointer<QLabel> status_label_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(StatusBar);
};

} // namespace console

#endif // CONSOLE_STATUSBAR_H
