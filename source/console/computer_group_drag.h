//
// PROJECT:         Aspia
// FILE:            console/computer_group_drag.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__COMPUTER_GROUP_DRAG_H
#define _ASPIA_CONSOLE__COMPUTER_GROUP_DRAG_H

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

    void setComputerGroup(ComputerGroup* computer_group)
    {
        ComputerGroupMimeData* mime_data = new ComputerGroupMimeData();
        mime_data->setComputerGroup(computer_group);
        setMimeData(mime_data);
    }
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__COMPUTER_GROUP_DRAG_H
