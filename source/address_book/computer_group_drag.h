//
// PROJECT:         Aspia
// FILE:            address_book/computer_group_drag.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_DRAG_H
#define _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_DRAG_H

#include <QtGui/QDrag>

#include "address_book/computer_group_mime_data.h"

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

#endif // _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_DRAG_H
