//
// PROJECT:         Aspia
// FILE:            client/ui/file_item_drag.cc
// LICENSE:         GNU General Public License 3
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

void FileItemDrag::setFileList(const QList<FileTransfer::Item>& file_list)
{
    FileItemMimeData* mime_data = new FileItemMimeData();
    mime_data->setFileList(file_list);
    setMimeData(mime_data);
}

} // namespace aspia
