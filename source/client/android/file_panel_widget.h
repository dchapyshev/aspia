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

#include <QList>
#include <QWidget>

#include "client/file_remover.h"
#include "client/file_transfer.h"
#include "common/file_task.h"

namespace proto::file_transfer {
enum ErrorCode : int;
class DriveList;
class List;
} // namespace proto::file_transfer

class ComboBox;
class IconButton;
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

    // The side's name, shown as the root entry of the path combo box.
    void setTitle(const QString& title);

    // Replies routed from the window's client.
    void onDriveList(proto::file_transfer::ErrorCode error_code,
                     const proto::file_transfer::DriveList& drive_list);
    void onFileList(proto::file_transfer::ErrorCode error_code,
                    const proto::file_transfer::List& file_list);
    void onCreateDirectory(proto::file_transfer::ErrorCode error_code);

    // Re-requests the current location (the drive list at the root, otherwise the file list).
    void refresh();

    // Whether the other side has a destination folder open; controls whether the per-item menu
    // offers upload/download. Driven by the owning window from the other panel's path changes.
    void setTransferAllowed(bool allowed);

signals:
    void sig_driveListRequested();
    void sig_fileListRequested(const QString& path);
    void sig_createDirectoryRequested(const QString& path);
    void sig_sendRequested(const QList<FileTransfer::Item>& items);
    void sig_removeRequested(const FileRemover::TaskList& items);
    void sig_pathChanged(const QString& path);

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onLocationActivated(int index);
    void onUpClicked();
    void onNewFolderClicked();
    void onDeleteClicked();

private:
    void setPath(const QString& path);

    // Selects the location that owns the current path (its longest matching prefix).
    void selectCurrentLocation();

    // True if |path| is the root of one of the known locations (a drive or special folder).
    bool isLocationRoot(const QString& path) const;

    // The absolute path of |name| inside the current folder, inserting a separator if the current
    // path lacks a trailing one (location roots like "/storage/emulated/0" do).
    QString itemPath(const QString& name) const;

    // Enables the delete button only while a file or folder is selected.
    void updateActions();

    void showItemActions(QTreeWidgetItem* item);
    void showError(const QString& message);

    const FileTask::Target target_;

    ComboBox* path_combo_ = nullptr;
    IconButton* up_button_ = nullptr;
    IconButton* new_folder_button_ = nullptr;
    IconButton* delete_button_ = nullptr;
    TreeWidget* list_ = nullptr;

    // Shown as the root entry of the path combo box (the side's name, e.g. "This Device").
    QString root_name_;

    // Empty means the drive list (the root); otherwise a directory path ending with '/'.
    QString current_path_;

    // True while the other side has a destination folder open, so transfers from here are possible.
    bool transfer_allowed_ = false;

    Q_DISABLE_COPY_MOVE(FilePanelWidget)
};

#endif // CLIENT_ANDROID_FILE_PANEL_WIDGET_H
