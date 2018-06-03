//
// PROJECT:         Aspia
// FILE:            client/ui/file_item_mime_data.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/file_item_mime_data.h"

namespace aspia {

// static
QString FileItemMimeData::mimeType()
{
    return QStringLiteral("application/file_list");
}

void FileItemMimeData::setFileList(const QList<FileTransfer::Item>& file_list)
{
    file_list_ = file_list;
    setData(mimeType(), QByteArray());
}

} // namespace aspia
