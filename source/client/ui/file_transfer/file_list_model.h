//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef CLIENT_UI_FILE_TRANSFER_FILE_LIST_MODEL_H
#define CLIENT_UI_FILE_TRANSFER_FILE_LIST_MODEL_H

#include "client/file_transfer.h"

#include <QAbstractItemModel>
#include <QIcon>

namespace client {

class FileListModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit FileListModel(QObject* parent = nullptr);

    void setMimeType(const QString& mime_type);
    QString mimeType() const { return mime_type_; }
    void setFileList(const proto::FileList& file_list);
    void setSortOrder(int column, Qt::SortOrder order);
    void clear();
    bool isFolder(const QModelIndex& index) const;
    QString nameAt(const QModelIndex& index) const;
    int64_t sizeAt(const QModelIndex& index) const;
    QModelIndex createFolder();

    // QAbstractItemModel implementation.
    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool canDropMimeData(const QMimeData* data, Qt::DropAction action,
                         int row, int column, const QModelIndex& parent) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action,
                      int row, int column, const QModelIndex& parent) override;
    Qt::DropActions supportedDropActions() const override;
    Qt::DropActions supportedDragActions() const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    void sort(int column, Qt::SortOrder order) override;

signals:
    void sig_nameChangeRequest(const QString& old_name, const QString& new_name);
    void sig_createFolderRequest(const QString& name);
    void sig_fileListDropped(const QString& folder_name, const std::vector<FileTransfer::Item>& files);

protected:
    void sortItems(int column, Qt::SortOrder order);
    static QString sizeToString(int64_t size);
    static QString timeToString(time_t time);

private:
    struct File
    {
        int64_t size = 0;
        time_t last_write = 0;
        QIcon icon;
        QString name;
        QString type;
    };

    struct Folder
    {
        QString name;
        time_t last_write = 0;
    };

    QList<Folder> folder_items_;
    QList<File> file_items_;

    const QIcon dir_icon_;
    const QString dir_type_;

    Qt::SortOrder current_order_ = Qt::AscendingOrder;
    int current_column_ = 0;

    QString mime_type_;

    DISALLOW_COPY_AND_ASSIGN(FileListModel);
};

} // namespace client

#endif // CLIENT_UI_FILE_TRANSFER_FILE_LIST_MODEL_H
