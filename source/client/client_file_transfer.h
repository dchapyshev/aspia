//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__CLIENT_FILE_TRANSFER_H
#define CLIENT__CLIENT_FILE_TRANSFER_H

#include "client/client.h"
#include "client/file_control.h"
#include "common/file_task_consumer.h"
#include "common/file_task_producer.h"

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

class FileControlProxy;
class FileManagerWindow;
class FileManagerWindowProxy;

class ClientFileTransfer
    : public Client,
      public FileControl,
      public common::FileTaskConsumer,
      public common::FileTaskProducer
{
public:
    explicit ClientFileTransfer(std::shared_ptr<base::TaskRunner> ui_task_runner);
    ~ClientFileTransfer();

    void setFileManagerWindow(FileManagerWindow* file_manager_window);

    // FileTaskConsumer implementation.
    void doTask(std::shared_ptr<common::FileTask> task) override;

protected:
    // Client implementation.
    void onSessionStarted(const base::Version& peer_version) override;
    void onSessionStopped() override;

    // net::Listener implementation.
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten() override;

    // FileTaskProducer implementation.
    void onTaskDone(std::shared_ptr<common::FileTask> task) override;

private:
    void doNextRemoteTask();

    common::FileTaskFactory* taskFactory(common::FileTask::Target target);

    // FileControl implementation.
    void driveList(common::FileTask::Target target) override;
    void fileList(common::FileTask::Target target, const std::string& path) override;
    void createDirectory(common::FileTask::Target target, const std::string& path) override;
    void rename(common::FileTask::Target target,
                const std::string& old_path,
                const std::string& new_path) override;
    void remove(common::FileTask::Target target,
                std::shared_ptr<FileRemoveWindowProxy> remove_window_proxy,
                const FileRemover::TaskList& items) override;
    void transfer(std::shared_ptr<FileTransferWindowProxy> transfer_window_proxy,
                  FileTransfer::Type transfer_type,
                  const std::string& source_path,
                  const std::string& target_path,
                  const std::vector<FileTransfer::Item>& items) override;

    std::shared_ptr<common::FileTaskConsumerProxy> task_consumer_proxy_;
    std::shared_ptr<common::FileTaskProducerProxy> task_producer_proxy_;

    std::unique_ptr<common::FileTaskFactory> local_task_factory_;
    std::unique_ptr<common::FileTaskFactory> remote_task_factory_;

    base::ScalableQueue<std::shared_ptr<common::FileTask>> remote_task_queue_;
    std::unique_ptr<common::FileWorker> local_worker_;

    std::shared_ptr<FileControlProxy> file_control_proxy_;
    std::unique_ptr<FileManagerWindowProxy> file_manager_window_proxy_;
    std::unique_ptr<FileRemover> remover_;
    std::unique_ptr<FileTransfer> transfer_;

    DISALLOW_COPY_AND_ASSIGN(ClientFileTransfer);
};

} // namespace client

#endif // CLIENT__CLIENT_FILE_TRANSFER_H
