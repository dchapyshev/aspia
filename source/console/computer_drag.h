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

#ifndef CONSOLE_COMPUTER_DRAG_H
#define CONSOLE_COMPUTER_DRAG_H

#include <QDrag>

#include "console/computer_mime_data.h"

namespace console {

class ComputerDrag final : public QDrag
{
public:
    explicit ComputerDrag(QObject* drag_source = nullptr)
        : QDrag(drag_source)
    {
        // Nothing
    }

    void setComputerItem(ComputerItem* computer_item, const QString& mime_type)
    {
        ComputerMimeData* mime_data = new ComputerMimeData();
        mime_data->setComputerItem(computer_item, mime_type);
        setMimeData(mime_data);
    }
};

} // namespace console

#endif // CONSOLE_COMPUTER_DRAG_H
