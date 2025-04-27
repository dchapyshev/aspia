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
#include "common/file_task.h"
#include "common/file_task_consumer.h"
#include "common/file_task_producer.h"

#include <queue>

namespace common {
class FileTaskConsumerProxy;
class FileTaskProducerProxy;
class FileTaskFactory;
class FileWorker;
} // namespace common

namespace proto {
class FileReply;
} // namespace proto

namespace client {

class ClientFileTransfer final
    : public Client,
      public common::FileTaskConsumer,
      public common::FileTaskProducer
{
    Q_OBJECT

public:
    explicit ClientFileTransfer(std::shared_ptr<base::TaskRunner> io_task_runner, QObject* parent = nullptr);
    ~ClientFileTransfer() final;

    // FileTaskConsumer implementation.
    void doTask(std::shared_ptr<common::FileTask> task) final;

public slots:
    void onDriveListRequest(common::FileTask::Target target);
    void onFileListRequest(common::FileTask::Target target, const std::string& path);
    void onCreateDirectoryRequest(common::FileTask::Target target, const std::string& path);
    void onRenameRequest(common::FileTask::Target target,
                         const std::string& old_path,
                         const std::string& new_path);
    void onRemoveRequest(FileRemover* remover);
    void onTransferRequest(FileTransfer* transfer);

signals:
    void sig_started();
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
    void onSessionMessageReceived(uint8_t channel_id, const QByteArray& buffer) final;
    void onSessionMessageWritten(uint8_t channel_id, size_t pending) final;

    // FileTaskProducer implementation.
    void onTaskDone(std::shared_ptr<common::FileTask> task) final;

private:
    void doNextRemoteTask();

    common::FileTaskFactory* taskFactory(common::FileTask::Target target);

    std::shared_ptr<common::FileTaskConsumerProxy> task_consumer_proxy_;
    std::shared_ptr<common::FileTaskProducerProxy> task_producer_proxy_;

    std::unique_ptr<common::FileTaskFactory> local_task_factory_;
    std::unique_ptr<common::FileTaskFactory> remote_task_factory_;

    std::queue<std::shared_ptr<common::FileTask>> remote_task_queue_;
    std::unique_ptr<common::FileWorker> local_worker_;

    std::unique_ptr<FileRemover> remover_;
    std::unique_ptr<FileTransfer> transfer_;

    DISALLOW_COPY_AND_ASSIGN(ClientFileTransfer);
};

} // namespace client

#endif // CLIENT_CLIENT_FILE_TRANSFER_H
