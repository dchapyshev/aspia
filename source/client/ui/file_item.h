//
// PROJECT:         Aspia
// FILE:            client/ui/file_item.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_ITEM_H
#define _ASPIA_CLIENT__UI__FILE_ITEM_H

#include <QTreeWidget>

#include "protocol/file_transfer_session.pb.h"

namespace aspia {

class FileItem : public QTreeWidgetItem
{
public:
    explicit FileItem(const proto::file_transfer::FileList::Item& item);
    explicit FileItem(const QString& directory_name);
    ~FileItem() = default;

    QString initialName() const { return name_; }
    QString currentName() const;
    bool isDirectory() const { return is_directory_; }
    qint64 fileSize() const { return size_; }
    time_t lastModified() const { return last_modified_; }

protected:
    bool operator<(const QTreeWidgetItem& other) const override;

private:
    QString name_;
    bool is_directory_;
    qint64 size_ = 0;
    time_t last_modified_ = 0;

    Q_DISABLE_COPY(FileItem)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_ITEM_H
