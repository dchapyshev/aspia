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

#ifndef ASPIA_CONSOLE__COMPUTER_GROUP_DRAG_H_
#define ASPIA_CONSOLE__COMPUTER_GROUP_DRAG_H_

#include <QDrag>

#include "console/computer_group_mime_data.h"

namespace aspia {

class ComputerGroupDrag : public QDrag
{
public:
    ComputerGroupDrag(QObject* dragSource = nullptr)
        : QDrag(dragSource)
    {
        // Nothing
    }

    void setComputerGroupItem(ComputerGroupItem* computer_group_item)
    {
        ComputerGroupMimeData* mime_data = new ComputerGroupMimeData();
        mime_data->setComputerGroupItem(computer_group_item);
        setMimeData(mime_data);
    }
};

} // namespace aspia

#endif // ASPIA_CONSOLE__COMPUTER_GROUP_DRAG_H_
