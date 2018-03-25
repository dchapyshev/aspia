//
// PROJECT:         Aspia
// FILE:            client/ui/file_item_mime_data.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_ITEM_MIME_DATA_H
#define _ASPIA_CLIENT__UI__FILE_ITEM_MIME_DATA_H

#include <QMimeData>

#include "base/macros.h"

namespace aspia {

class FileItem;

class FileItemMimeData : public QMimeData
{
public:
    FileItemMimeData();
    virtual ~FileItemMimeData();

    static QString mimeType();

    void setFileItem(FileItem* file_item);
    FileItem* fileItem() const;

private:
    FileItem* file_item_;

    DISALLOW_COPY_AND_ASSIGN(FileItemMimeData);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_ITEM_MIME_DATA_H
