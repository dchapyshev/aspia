//
// PROJECT:         Aspia
// FILE:            client/ui/file_item.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_ITEM_H
#define _ASPIA_CLIENT__UI__FILE_ITEM_H

#include <QTreeWidget>

#include "proto/file_transfer_session.pb.h"

namespace aspia {

class FileItem : public QTreeWidgetItem
{
public:
    explicit FileItem(const proto::file_transfer::FileList::Item& item);
    explicit FileItem(const QString& directory_name);
    ~FileItem();

    QString initialName() const;
    QString currentName() const;
    bool isDirectory() const;
    qint64 fileSize() const;
    time_t lastModified() const;

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
