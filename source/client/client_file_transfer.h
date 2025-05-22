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

#ifndef CLIENT_CLIENT_FILE_TRANSFER_H
#define CLIENT_CLIENT_FILE_TRANSFER_H

#include "client/client.h"
#include "client/file_remover.h"
#include "client/file_transfer.h"
#include "common/file_task_factory.h"
#include "common/file_worker.h"

#include <queue>

namespace client {

class ClientFileTransfer final : public Client
{
    Q_OBJECT

public:
    explicit ClientFileTransfer(QObject* parent = nullptr);
    ~ClientFileTransfer() final;

public slots:
    void onTask(base::local_shared_ptr<common::FileTask> task);
    void onDriveListRequest(common::FileTask::Target target);
    void onFileListRequest(common::FileTask::Target target, const QString& path);
    void onCreateDirectoryRequest(common::FileTask::Target target, const QString& path);
    void onRenameRequest(common::FileTask::Target target,
                         const QString& old_path,
                         const QString& new_path);
    void onRemoveRequest(client::FileRemover* remover);
    void onTransferRequest(client::FileTransfer* transfer);

signals:
    void sig_errorOccurred(proto::FileError error_code);
    void sig_driveListReply(common::FileTask::Target target,
                            proto::FileError error_code,
                            const proto::DriveList& drive_list);
    void sig_fileListReply(common::FileTask::Target target,
                      proto::FileError error_code,
                      const proto::FileList& file_list);
    void sig_createDirectoryReply(common::FileTask::Target target, proto::FileError error_code);
    void sig_renameReply(common::FileTask::Target target, proto::FileError error_code);

protected:
    // Client implementation.
    void onSessionStarted() final;
    void onSessionMessageReceived(const QByteArray& buffer) final;
    void onSessionMessageWritten(size_t pending) final;

private slots:
    void onTaskDone(base::local_shared_ptr<common::FileTask> task);

private:
    void doNextRemoteTask();

    common::FileTaskFactory* taskFactory(common::FileTask::Target target);

    QPointer<common::FileTaskFactory> local_task_factory_;
    QPointer<common::FileTaskFactory> remote_task_factory_;

    std::queue<base::local_shared_ptr<common::FileTask>> remote_task_queue_;
    common::FileWorker local_worker_;

    QPointer<FileRemover> remover_;
    QPointer<FileTransfer> transfer_;

    DISALLOW_COPY_AND_ASSIGN(ClientFileTransfer);
};

} // namespace client

#endif // CLIENT_CLIENT_FILE_TRANSFER_H
