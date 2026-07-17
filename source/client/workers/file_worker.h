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

#ifndef CLIENT_WORKERS_FILE_WORKER_H
#define CLIENT_WORKERS_FILE_WORKER_H

#include <QPointer>
#include <QQueue>

#include "base/serialization.h"
#include "base/threading/worker.h"
#include "client/file_remover.h"
#include "client/file_transfer.h"
#include "common/file_task_factory.h"
#include "proto/file_transfer.h"

class FileRequestHandler;

class FileWorker final : public Worker
{
    Q_OBJECT

public:
    FileWorker();
    ~FileWorker() final;

public slots:
    void onDriveListRequest(FileTask::Target target);
    void onFileListRequest(FileTask::Target target, const QString& path);
    void onCreateDirectoryRequest(FileTask::Target target, const QString& path);
    void onRenameRequest(FileTask::Target target, const QString& old_path, const QString& new_path);
    void onRemoveRequest(FileRemover* remover);
    void onTransferRequest(FileTransfer* transfer);

signals:
    void sig_errorOccurred(proto::file_transfer::ErrorCode error_code);
    void sig_driveListReply(FileTask::Target target,
                            proto::file_transfer::ErrorCode error_code,
                            const proto::file_transfer::DriveList& drive_list);
    void sig_fileListReply(FileTask::Target target,
                           proto::file_transfer::ErrorCode error_code,
                           const proto::file_transfer::List& file_list);
    void sig_createDirectoryReply(FileTask::Target target, proto::file_transfer::ErrorCode error_code);
    void sig_renameReply(FileTask::Target target, proto::file_transfer::ErrorCode error_code);

    // Sends an outgoing session message to the network worker.
    void sig_sendMessage(quint8 channel_id, const QByteArray& buffer);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private slots:
    void onChannelMessage(const QByteArray& buffer);
    void onTask(const FileTask& task);
    void onTaskDone(const FileTask& task);

private:
    void doNextRemoteTask();
    FileTaskFactory* taskFactory(FileTask::Target target);

    QPointer<FileTaskFactory> local_task_factory_;
    QPointer<FileTaskFactory> remote_task_factory_;

    QQueue<FileTask> remote_task_queue_;
    FileRequestHandler* local_handler_ = nullptr;

    QPointer<FileRemover> remover_;
    QPointer<FileTransfer> transfer_;

    SerializerImpl serializer_;

    Q_DISABLE_COPY_MOVE(FileWorker)
};

#endif // CLIENT_WORKERS_FILE_WORKER_H
