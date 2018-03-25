//
// PROJECT:         Aspia
// FILE:            client/ui/file_item_drag.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/file_item_drag.h"

#include "client/ui/file_item_mime_data.h"

namespace aspia {

FileItemDrag::FileItemDrag(QObject* drag_source)
    : QDrag(drag_source)
{
    // Nothing
}

FileItemDrag::~FileItemDrag() = default;

void FileItemDrag::setFileItem(FileItem* file_item)
{
    FileItemMimeData* mime_data = new FileItemMimeData();
    mime_data->setFileItem(file_item);
    setMimeData(mime_data);
}

} // namespace aspia
