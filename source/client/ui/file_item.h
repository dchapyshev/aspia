//
// PROJECT:         Aspia
// FILE:            client/ui/file_item.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_ITEM_H
#define _ASPIA_CLIENT__UI__FILE_ITEM_H

#include <QTreeWidget>

#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

class FileItem : public QTreeWidgetItem
{
public:
    FileItem(const proto::file_transfer::FileList::Item& item);
    ~FileItem() = default;

    bool isDirectory() const { return is_directory_; }
    bool fileSize() const { return size_; }
    time_t lastModified() const { return last_modified_; }

protected:
    bool operator<(const QTreeWidgetItem& other) const override;

private:
    bool is_directory_;
    qint64 size_;
    time_t last_modified_;

    DISALLOW_COPY_AND_ASSIGN(FileItem);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_ITEM_H
