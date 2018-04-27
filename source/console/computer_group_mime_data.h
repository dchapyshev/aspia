//
// PROJECT:         Aspia
// FILE:            console/computer_group_mime_data.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
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
