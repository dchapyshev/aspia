//
// PROJECT:         Aspia
// FILE:            client/ui/file_item_drag.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_ITEM_DRAG_H
#define _ASPIA_CLIENT__UI__FILE_ITEM_DRAG_H

#include <QDrag>

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
    Q_DISABLE_COPY(FileItemDrag)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_ITEM_DRAG_H
