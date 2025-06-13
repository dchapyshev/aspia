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

#ifndef CONSOLE_COMPUTER_MIME_DATA_H
#define CONSOLE_COMPUTER_MIME_DATA_H

#include <QMimeData>

#include "console/computer_item.h"

namespace console {

class ComputerMimeData final : public QMimeData
{
public:
    ComputerMimeData() = default;
    virtual ~ComputerMimeData() final = default;

    void setComputerItem(ComputerItem* computer_item, const QString& mime_type)
    {
        computer_item_ = computer_item;
        setData(mime_type, QByteArray());
    }

    ComputerItem* computerItem() const
    {
        return computer_item_;
    }

private:
    ComputerItem* computer_item_ = nullptr;
};

} // namespace console

#endif // CONSOLE_COMPUTER_MIME_DATA_H
