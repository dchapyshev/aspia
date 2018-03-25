//
// PROJECT:         Aspia
// FILE:            client/ui/file_item_mime_data.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/file_item_mime_data.h"

namespace aspia {

FileItemMimeData::FileItemMimeData() = default;
FileItemMimeData::~FileItemMimeData() = default;

// static
QString FileItemMimeData::mimeType()
{
    return "application/file_item";
}

void FileItemMimeData::setFileItem(FileItem* file_item)
{
    file_item_ = file_item;
    setData(mimeType(), QByteArray());
}

FileItem* FileItemMimeData::fileItem() const
{
    return file_item_;
}

} // namespace aspia
