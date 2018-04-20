//
// PROJECT:         Aspia
// FILE:            client/ui/file_item_mime_data.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_ITEM_MIME_DATA_H
#define _ASPIA_CLIENT__UI__FILE_ITEM_MIME_DATA_H

#include <QMimeData>

#include "client/file_transfer.h"

namespace aspia {

class FileItem;

class FileItemMimeData : public QMimeData
{
public:
    FileItemMimeData();
    virtual ~FileItemMimeData();

    static QString mimeType();

    void setFileList(const QList<FileTransfer::Item>& file_list);
    QList<FileTransfer::Item> fileList() const;

private:
    QList<FileTransfer::Item> file_list_;

    Q_DISABLE_COPY(FileItemMimeData)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_ITEM_MIME_DATA_H
