//
// SmartCafe Project
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

#ifndef CONSOLE_COMPUTER_GROUP_MIME_DATA_H
#define CONSOLE_COMPUTER_GROUP_MIME_DATA_H

#include <QMimeData>

#include "console/computer_group_item.h"

namespace console {

class ComputerGroupMimeData final : public QMimeData
{
public:
    ComputerGroupMimeData() = default;
    virtual ~ComputerGroupMimeData() final = default;

    void setComputerGroupItem(ComputerGroupItem* computer_group_item, const QString& mime_type)
    {
        computer_group_item_ = computer_group_item;
        setData(mime_type, QByteArray());
    }

    ComputerGroupItem* computerGroup() const
    {
        return computer_group_item_;
    }

private:
    ComputerGroupItem* computer_group_item_ = nullptr;
};

} // namespace console

#endif // CONSOLE_COMPUTER_GROUP_MIME_DATA_H
