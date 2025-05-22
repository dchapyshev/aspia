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

#ifndef CLIENT_UI_FILE_TRANSFER_FILE_TRANSFER_SESSION_WINDOW_H
#define CLIENT_UI_FILE_TRANSFER_FILE_TRANSFER_SESSION_WINDOW_H

#include "client/file_remover.h"
#include "client/file_transfer.h"
#include "client/ui/session_window.h"

#include <QPointer>

namespace Ui {
class FileTransferSessionWindow;
} // namespace Ui

namespace client {

class FilePanel;
class FileRemoveDialog;
class FileTransferDialog;

class FileTransferSessionWindow final : public SessionWindow
{
    Q_OBJECT

public:
    explicit FileTransferSessionWindow(QWidget* parent = nullptr);
    ~FileTransferSessionWindow() final;

    // SessionWindow implementation.
    Client* createClient() final;

    QByteArray saveState() const;
    void restoreState(const QByteArray& state);

public slots:
    void onShowWindow();
    void onErrorOccurred(proto::FileError error_code);
    void onDriveList(common::FileTask::Target target,
                     proto::FileError error_code,
                     const proto::DriveList& drive_list);
    void onFileList(common::FileTask::Target target,
                    proto::FileError error_code,
                    const proto::FileList& file_list);
    void onCreateDirectory(common::FileTask::Target target, proto::FileError error_code);
    void onRename(common::FileTask::Target target, proto::FileError error_code);

    void refresh();

signals:
    void sig_driveListRequest(common::FileTask::Target target);
    void sig_fileListRequest(common::FileTask::Target target, const QString& path);
    void sig_createDirectoryRequest(common::FileTask::Target target, const QString& path);
    void sig_renameRequest(common::FileTask::Target target, const QString& old_path, const QString& new_path);
    void sig_removeRequest(FileRemover* remover);
    void sig_transferRequest(FileTransfer* transfer);

protected:
    // SessionWindow implementation.
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

private:
    void transferItems(FileTransfer::Type type,
                       const QString& source_path,
                       const QString& target_path,
                       const QList<FileTransfer::Item>& items);

    void initPanel(common::FileTask::Target target,
                   const QString& title,
                   const QString& mime_type,
                   FilePanel* panel);

    std::unique_ptr<Ui::FileTransferSessionWindow> ui;

    QPointer<FileRemoveDialog> remove_dialog_;
    QPointer<FileTransferDialog> transfer_dialog_;

    DISALLOW_COPY_AND_ASSIGN(FileTransferSessionWindow);
};

} // namespace client

#endif // CLIENT_UI_FILE_TRANSFER_FILE_TRANSFER_SESSION_WINDOW_H
