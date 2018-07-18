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

#ifndef _ASPIA_CONSOLE__COMPUTER_GROUP_MIME_DATA_H
#define _ASPIA_CONSOLE__COMPUTER_GROUP_MIME_DATA_H

#include <QMimeData>

#include "console/computer_group_item.h"

namespace aspia {

class ComputerGroupMimeData : public QMimeData
{
public:
    ComputerGroupMimeData() = default;
    virtual ~ComputerGroupMimeData() = default;

    static QString mimeType()
    {
        return QStringLiteral("application/computer_group");
    }

    void setComputerGroupItem(ComputerGroupItem* computer_group_item)
    {
        computer_group_item_ = computer_group_item;
        setData(mimeType(), QByteArray());
    }

    ComputerGroupItem* computerGroup() const
    {
        return computer_group_item_;
    }

private:
    ComputerGroupItem* computer_group_item_ = nullptr;
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__COMPUTER_GROUP_MIME_DATA_H
