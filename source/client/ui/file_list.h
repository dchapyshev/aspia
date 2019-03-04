//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__UI__FILE_LIST_H
#define CLIENT__UI__FILE_LIST_H

#include "client/file_transfer.h"
#include "proto/file_transfer.pb.h"

#include <QTreeView>

namespace client {

class AddressBarModel;
class FileListModel;

class FileList : public QTreeView
{
    Q_OBJECT

public:
    explicit FileList(QWidget* parent = nullptr);
    ~FileList() = default;

    void showDriveList(AddressBarModel* model);
    void showFileList(const proto::file_transfer::FileList& file_list);
    void setMimeType(const QString& mime_type);
    bool isDriveListShown() const;
    bool isFileListShown() const;
    void createFolder();

    void restoreState(const QByteArray& state);
    QByteArray saveState() const;

signals:
    void nameChangeRequest(const QString& old_name, const QString& new_name);
    void createFolderRequest(const QString& name);
    void fileListDropped(const QString& folder_name, const QList<FileTransfer::Item>& files);

protected:
    // QTreeView implemenation.
    void keyPressEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void saveColumnsState();

    FileListModel* model_;

    QByteArray drive_list_state_;
    QByteArray file_list_state_;

    DISALLOW_COPY_AND_ASSIGN(FileList);
};

} // namespace client

#endif // CLIENT__UI__FILE_LIST_H
