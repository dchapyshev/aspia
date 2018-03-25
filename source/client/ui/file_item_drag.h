//
// PROJECT:         Aspia
// FILE:            client/ui/file_item_drag.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_ITEM_DRAG_H
#define _ASPIA_CLIENT__UI__FILE_ITEM_DRAG_H

#include <QDrag>

#include "base/macros.h"

namespace aspia {

class FileItem;
class FileItemMimeData;

class FileItemDrag : public QDrag
{
public:
    FileItemDrag(QObject* drag_source = nullptr);
    virtual ~FileItemDrag();

    void setFileItem(FileItem* file_item);

private:
    DISALLOW_COPY_AND_ASSIGN(FileItemDrag);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_ITEM_DRAG_H
