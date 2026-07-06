//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_DESKTOP_FILE_TRANSFER_FILE_LIST_H
#define CLIENT_DESKTOP_FILE_TRANSFER_FILE_LIST_H

#include <QTreeView>

#include "client/file_transfer.h"

class AddressBarModel;
class FileListModel;

namespace proto::file_transfer {
class List;
} // namespace proto::file_transfer

class FileList final : public QTreeView
{
    Q_OBJECT

public:
    explicit FileList(QWidget* parent = nullptr);
    ~FileList() final = default;

    void showDriveList(AddressBarModel* model);
    void showFileList(const proto::file_transfer::List& file_list);
    void setMimeType(const QString& mime_type);
    bool isDriveListShown() const;
    bool isFileListShown() const;
    void createFolder();

    void restoreState(const QByteArray& state);
    QByteArray saveState() const;

signals:
    void sig_nameChangeRequest(const QString& old_name, const QString& new_name);
    void sig_createFolderRequest(const QString& name);
    void sig_fileListDropped(const QString& folder_name, const QList<FileTransfer::Item>& files);

protected:
    // QTreeView implementation.
    void keyPressEvent(QKeyEvent* event) final;
    void mouseDoubleClickEvent(QMouseEvent* event) final;

private:
    void saveColumnsState();

    FileListModel* model_;

    QByteArray drive_list_state_;
    QByteArray file_list_state_;

    Q_DISABLE_COPY_MOVE(FileList)
};

#endif // CLIENT_DESKTOP_FILE_TRANSFER_FILE_LIST_H
