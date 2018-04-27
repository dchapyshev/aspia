//
// PROJECT:         Aspia
// FILE:            console/computer_mime_data.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__COMPUTER_MIME_DATA_H
#define _ASPIA_CONSOLE__COMPUTER_MIME_DATA_H

#include <QMimeData>

#include "console/computer_item.h"

namespace aspia {

class ComputerMimeData : public QMimeData
{
public:
    ComputerMimeData() = default;
    virtual ~ComputerMimeData() = default;

    static QString mimeType()
    {
        return QStringLiteral("application/computer");
    }

    void setComputerItem(ComputerItem* computer_item)
    {
        computer_item_ = computer_item;
        setData(mimeType(), QByteArray());
    }

    ComputerItem* computerItem() const
    {
        return computer_item_;
    }

private:
    ComputerItem* computer_item_ = nullptr;
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__COMPUTER_MIME_DATA_H
