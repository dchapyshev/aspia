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

#ifndef CLIENT_ANDROID_FILE_TRANSFER_WINDOW_H
#define CLIENT_ANDROID_FILE_TRANSFER_WINDOW_H

#include <QWidget>

#include <memory>

#include "base/scoped_qpointer.h"
#include "client/client.h"
#include "client/config.h"
#include "client/file_remover.h"
#include "client/file_transfer.h"
#include "common/file_task.h"

namespace proto::file_transfer {
enum ErrorCode : int;
class DriveList;
class List;
} // namespace proto::file_transfer

class AppBar;
class Button;
class ClientFileTransfer;
class FilePanelWidget;
class Label;
class Router;
class SessionState;
class QHBoxLayout;
class QWidget;

// File transfer session screen for the Android client: a top app bar and two file panels (this
// device and the remote host). Opened as a non-full-screen page in the main window's root stack and
// driven by a ClientFileTransfer running on the I/O thread.
class FileTransferWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit FileTransferWindow(const HostConfig& host, QWidget* parent = nullptr);
    ~FileTransferWindow() final;

signals:
    void sig_closed();

    // Routed to the client (queued).
    void sig_driveListRequest(FileTask::Target target);
    void sig_fileListRequest(FileTask::Target target, const QString& path);
    void sig_createDirectoryRequest(FileTask::Target target, const QString& path);
    void sig_removeRequest(FileRemover* remover);
    void sig_transferRequest(FileTransfer* transfer);

protected:
    // QWidget implementation.
    void resizeEvent(QResizeEvent* event) final;

private slots:
    void onStatusChanged(Client::Status status, const QVariant& data);
    void onErrorOccurred(proto::file_transfer::ErrorCode error_code);
    void onDriveList(FileTask::Target target, proto::file_transfer::ErrorCode error_code,
                     const proto::file_transfer::DriveList& drive_list);
    void onFileList(FileTask::Target target, proto::file_transfer::ErrorCode error_code,
                    const proto::file_transfer::List& file_list);
    void onCreateDirectory(FileTask::Target target, proto::file_transfer::ErrorCode error_code);

private:
    // Prompts for the all-files-access permission needed to browse this device's storage.
    void ensureStoragePermission();

    void start();
    void fetchConnectionOffer();
    void requestConnectionOffer(Router* router);
    void startNewClient();
    void initPanel(FilePanelWidget* panel, FileTask::Target target);
    void sendItems(FilePanelWidget* sender, const QList<FileTransfer::Item>& items);
    void removeItems(FilePanelWidget* sender, const FileRemover::TaskList& items);
    void setActivePanel(int index);
    void applyLayout();
    void setStatusText(const QString& text);

    HostConfig host_;
    std::shared_ptr<SessionState> session_state_;
    ScopedQPointer<ClientFileTransfer> client_;

    AppBar* app_bar_ = nullptr;
    Label* status_ = nullptr;
    FilePanelWidget* local_panel_ = nullptr;
    FilePanelWidget* remote_panel_ = nullptr;
    QWidget* segment_ = nullptr;
    Button* local_tab_ = nullptr;
    Button* remote_tab_ = nullptr;
    QHBoxLayout* panels_layout_ = nullptr;

    // On a narrow screen only one panel is shown at a time; 0 = this device, 1 = remote host.
    int active_panel_ = 1;
    bool connected_ = false;

    Q_DISABLE_COPY_MOVE(FileTransferWindow)
};

#endif // CLIENT_ANDROID_FILE_TRANSFER_WINDOW_H
