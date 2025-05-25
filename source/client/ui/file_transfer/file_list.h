//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_UI_FILE_TRANSFER_FILE_LIST_H
#define CLIENT_UI_FILE_TRANSFER_FILE_LIST_H

#include "client/file_transfer.h"
#include "proto/file_transfer.h"

#include <QTreeView>

namespace client {

class AddressBarModel;
class FileListModel;

class FileList final : public QTreeView
{
    Q_OBJECT

public:
    explicit FileList(QWidget* parent = nullptr);
    ~FileList() final = default;

    void showDriveList(AddressBarModel* model);
    void showFileList(const proto::FileList& file_list);
    void setMimeType(const QString& mime_type);
    bool isDriveListShown() const;
    bool isFileListShown() const;
    void createFolder();

    void restoreState(const QByteArray& state);
    QByteArray saveState() const;

signals:
    void sig_nameChangeRequest(const QString& old_name, const QString& new_name);
    void sig_createFolderRequest(const QString& name);
    void sig_fileListDropped(const QString& folder_name, const QList<client::FileTransfer::Item>& files);

protected:
    // QTreeView implemenation.
    void keyPressEvent(QKeyEvent* event) final;
    void mouseDoubleClickEvent(QMouseEvent* event) final;

private:
    void saveColumnsState();

    FileListModel* model_;

    QByteArray drive_list_state_;
    QByteArray file_list_state_;

    DISALLOW_COPY_AND_ASSIGN(FileList);
};

} // namespace client

#endif // CLIENT_UI_FILE_TRANSFER_FILE_LIST_H
