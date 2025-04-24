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

#ifndef CLIENT_FILE_REMOVE_QUEUE_BUILDER_H
#define CLIENT_FILE_REMOVE_QUEUE_BUILDER_H

#include "client/file_remover.h"
#include "proto/file_transfer.pb.h"

namespace client {

// The class prepares the task queue to perform the deletion.
class FileRemoveQueueBuilder final
    : public QObject,
      public common::FileTaskProducer
{
    Q_OBJECT

public:
    FileRemoveQueueBuilder(
        std::shared_ptr<common::FileTaskConsumerProxy> task_consumer_proxy,
        common::FileTask::Target target,
        QObject* parent = nullptr);
    ~FileRemoveQueueBuilder() final;

    using FinishCallback = std::function<void(proto::FileError)>;

    // Starts building of the task queue.
    void start(const FileRemover::TaskList& items, const FinishCallback& callback);

    FileRemover::TaskList takeQueue();

protected:
    // FileTaskProducer implementation.
    void onTaskDone(std::shared_ptr<common::FileTask> task) final;

private:
    void doPendingTasks();
    void onAborted(proto::FileError error_code);

    std::shared_ptr<common::FileTaskConsumerProxy> task_consumer_proxy_;
    std::shared_ptr<common::FileTaskProducerProxy> task_producer_proxy_;
    std::unique_ptr<common::FileTaskFactory> task_factory_;

    FinishCallback callback_;

    FileRemover::TaskList pending_tasks_;
    FileRemover::TaskList tasks_;

    DISALLOW_COPY_AND_ASSIGN(FileRemoveQueueBuilder);
};

} // namespace client

#endif // CLIENT_FILE_REMOVE_QUEUE_BUILDER_H
