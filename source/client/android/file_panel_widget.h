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

#ifndef CLIENT_ANDROID_FILE_PANEL_WIDGET_H
#define CLIENT_ANDROID_FILE_PANEL_WIDGET_H

#include <QWidget>

#include "client/file_remover.h"
#include "client/file_transfer.h"
#include "common/file_task.h"

namespace proto::file_transfer {
enum ErrorCode : int;
class DriveList;
class List;
} // namespace proto::file_transfer

class Button;
class IconButton;
class Label;
class TreeWidget;
class QTreeWidgetItem;

// One side of the file transfer screen: a path bar, a toolbar and a touch list showing either the
// drive list (at the root) or the contents of the current directory. Browsing is local to the panel;
// data requests are emitted as signals and answered by the owning window.
class FilePanelWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit FilePanelWidget(FileTask::Target target, QWidget* parent = nullptr);
    ~FilePanelWidget() final;

    FileTask::Target target() const { return target_; }
    const QString& currentPath() const { return current_path_; }

    void setTitle(const QString& title);

    // Hidden in portrait, where the panel switcher already names the side.
    void setHeaderVisible(bool visible);

    // Replies routed from the window's client.
    void onDriveList(proto::file_transfer::ErrorCode error_code,
                     const proto::file_transfer::DriveList& drive_list);
    void onFileList(proto::file_transfer::ErrorCode error_code,
                    const proto::file_transfer::List& file_list);
    void onCreateDirectory(proto::file_transfer::ErrorCode error_code);

    // Re-requests the current location (the drive list at the root, otherwise the file list).
    void refresh();

signals:
    void sig_driveListRequested();
    void sig_fileListRequested(const QString& path);
    void sig_createDirectoryRequested(const QString& path);
    void sig_sendRequested(const QList<FileTransfer::Item>& items);
    void sig_removeRequested(const FileRemover::TaskList& items);

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onUpClicked();
    void onNewFolderClicked();

private:
    // Opens a breadcrumb popup to jump straight to the root or any parent of the current directory.
    void showPathMenu();

    void setPath(const QString& path);
    void showItemActions(QTreeWidgetItem* item);
    void showError(const QString& message);

    const FileTask::Target target_;

    Label* title_ = nullptr;
    Button* path_button_ = nullptr;
    IconButton* up_button_ = nullptr;
    TreeWidget* list_ = nullptr;

    // Empty means the drive list (the root); otherwise a directory path ending with '/'.
    QString current_path_;

    Q_DISABLE_COPY_MOVE(FilePanelWidget)
};

#endif // CLIENT_ANDROID_FILE_PANEL_WIDGET_H
