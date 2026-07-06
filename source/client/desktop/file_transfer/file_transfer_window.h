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

#ifndef CLIENT_DESKTOP_FILE_TRANSFER_FILE_TRANSFER_WINDOW_H
#define CLIENT_DESKTOP_FILE_TRANSFER_FILE_TRANSFER_WINDOW_H

#include <memory>

#include "client/file_remover.h"
#include "client/file_transfer.h"
#include "client/desktop/client_window.h"

namespace Ui {
class FileTransferWindow;
} // namespace Ui

class FilePanel;

class FileTransferWindow final : public ClientWindow
{
    Q_OBJECT

public:
    explicit FileTransferWindow(QWidget* parent = nullptr);
    ~FileTransferWindow() final;

    // ClientWindow implementation.
    Client* createClient() final;
    QByteArray saveState() const final;
    void restoreState(const QByteArray& state) final;

public slots:
    void onShowWindow();
    void onErrorOccurred(proto::file_transfer::ErrorCode error_code);
    void onDriveList(FileTask::Target target,
                     proto::file_transfer::ErrorCode error_code,
                     const proto::file_transfer::DriveList& drive_list);
    void onFileList(FileTask::Target target,
                    proto::file_transfer::ErrorCode error_code,
                    const proto::file_transfer::List& file_list);
    void onCreateDirectory(FileTask::Target target, proto::file_transfer::ErrorCode error_code);
    void onRename(FileTask::Target target, proto::file_transfer::ErrorCode error_code);

    void refresh();

signals:
    void sig_driveListRequest(FileTask::Target target);
    void sig_fileListRequest(FileTask::Target target, const QString& path);
    void sig_createDirectoryRequest(FileTask::Target target, const QString& path);
    void sig_renameRequest(FileTask::Target target, const QString& old_path, const QString& new_path);
    void sig_removeRequest(FileRemover* remover);
    void sig_transferRequest(FileTransfer* transfer);

protected:
    // ClientWindow implementation.
    void onInternalReset() final;

    // QWidget implementation.
    void closeEvent(QCloseEvent* event) final;

private slots:
    void removeItems(FilePanel* sender, const FileRemover::TaskList& items);
    void sendItems(FilePanel* sender, const QList<FileTransfer::Item>& items);
    void receiveItems(FilePanel* sender,
                      const QString& target_folder,
                      const QList<FileTransfer::Item>& items);
    void onPathChanged(FilePanel* sender, const QString& path);
    void onTransferWidgetFinished();
    void onRemoveWidgetFinished();

private:
    void transferItems(FileTransfer::Type type,
                       const QString& source_path,
                       const QString& target_path,
                       const QList<FileTransfer::Item>& items);

    void initPanel(FileTask::Target target,
                   const QString& title,
                   const QString& mime_type,
                   FilePanel* panel);

    void setFilePanelsEnabled(bool enabled);

    std::unique_ptr<Ui::FileTransferWindow> ui;

    Q_DISABLE_COPY_MOVE(FileTransferWindow)
};

#endif // CLIENT_DESKTOP_FILE_TRANSFER_FILE_TRANSFER_WINDOW_H
