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

#ifndef CLIENT_UI_FILE_TRANSFER_FILE_PANEL_H
#define CLIENT_UI_FILE_TRANSFER_FILE_PANEL_H

#include "client/file_remover.h"
#include "client/file_transfer.h"
#include "proto/file_transfer.h"
#include "ui_file_panel.h"

namespace client {

class FilePanel final : public QWidget
{
    Q_OBJECT

public:
    explicit FilePanel(QWidget* parent = nullptr);
    ~FilePanel() final;

    void onDriveList(proto::file_transfer::ErrorCode error_code, const proto::file_transfer::DriveList& drive_list);
    void onFileList(proto::file_transfer::ErrorCode error_code, const proto::file_transfer::List& file_list);
    void onCreateDirectory(proto::file_transfer::ErrorCode error_code);
    void onRename(proto::file_transfer::ErrorCode error_code);

    void setPanelName(const QString& name);
    void setMimeType(const QString& mime_type);
    void setTransferAllowed(bool allowed);
    void setTransferEnabled(bool enabled);

    QString currentPath() const { return ui.address_bar->currentPath(); }

    QByteArray saveState() const;
    void restoreState(const QByteArray& state);

signals:
    void sig_driveList();
    void sig_fileList(const QString& path);
    void sig_rename(const QString& old_name, const QString& new_name);
    void sig_createDirectory(const QString& path);
    void sig_removeItems(client::FilePanel* sender, const client::FileRemover::TaskList& items);
    void sig_sendItems(client::FilePanel* sender, const QList<client::FileTransfer::Item>& items);
    void sig_receiveItems(client::FilePanel* sender,
                          const QString& folder,
                          const QList<client::FileTransfer::Item>& items);
    void sig_pathChanged(client::FilePanel* sender, const QString& path);

public slots:
    void refresh();

protected:
    // QWidget implementation.
    void keyPressEvent(QKeyEvent* event) final;

private slots:
    void onPathChanged(const QString& path);
    void onListItemActivated(const QModelIndex& index);
    void onListSelectionChanged();
    void onListContextMenu(const QPoint& point);
    void onNameChangeRequest(const QString& old_name, const QString& new_name);
    void onCreateFolderRequest(const QString& name);

    void toChildFolder(const QString& child_name);
    void toParentFolder();
    void addFolder();
    void removeSelected();
    void sendSelected();

private:
    void showError(const QString& message);

    Ui::FilePanel ui;

    bool transfer_allowed_ = false;
    bool transfer_enabled_ = false;

    Q_DISABLE_COPY(FilePanel)
};

} // namespace client

#endif // CLIENT_UI_FILE_TRANSFER_FILE_PANEL_H
