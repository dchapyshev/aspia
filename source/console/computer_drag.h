//
// PROJECT:         Aspia
// FILE:            console/computer_drag.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__COMPUTER_DRAG_H
#define _ASPIA_CONSOLE__COMPUTER_DRAG_H

#include <QDrag>

#include "console/computer_mime_data.h"

namespace aspia {

class ComputerDrag : public QDrag
{
public:
    ComputerDrag(QObject* dragSource = nullptr)
        : QDrag(dragSource)
    {
        // Nothing
    }

    void setComputerItem(ComputerItem* computer_item)
    {
        ComputerMimeData* mime_data = new ComputerMimeData();
        mime_data->setComputerItem(computer_item);
        setMimeData(mime_data);
    }
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__COMPUTER_DRAG_H
