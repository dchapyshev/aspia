//
// PROJECT:         Aspia
// FILE:            address_book/computer_group_mime_data.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_MIME_DATA_H
#define _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_MIME_DATA_H

#include <QtCore/QMimeData>

#include "address_book/computer_group.h"

class ComputerGroupMimeData : public QMimeData
{
public:
    ComputerGroupMimeData() = default;
    virtual ~ComputerGroupMimeData() = default;

    static QString mimeType()
    {
        return "application/computer_group";
    }

    void setComputerGroup(ComputerGroup* computer_group)
    {
        computer_group_ = computer_group;
        setData(mimeType(), QByteArray());
    }

    ComputerGroup* computerGroup() const
    {
        return computer_group_;
    }

private:
    ComputerGroup* computer_group_ = nullptr;
};

#endif // _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_MIME_DATA_H
