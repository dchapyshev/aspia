//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__FILE_REMOVE_QUEUE_BUILDER_H
#define CLIENT__FILE_REMOVE_QUEUE_BUILDER_H

#include "client/file_remover.h"

namespace client {

// The class prepares the task queue to perform the deletion.
class FileRemoveQueueBuilder : public common::FileRequestProducer
{
public:
    FileRemoveQueueBuilder(
        std::shared_ptr<common::FileRequestConsumerProxy>& request_consumer_proxy,
        common::FileTaskTarget target);
    ~FileRemoveQueueBuilder();

    using FinishCallback = std::function<void(proto::FileError)>;

    // Starts building of the task queue.
    void start(const FileRemover::TaskList& items, const FinishCallback& callback);

    FileRemover::TaskList takeQueue();

protected:
    // FileRequestProducer implementation.
    void onReply(std::shared_ptr<common::FileRequest> request) override;

private:
    void doPendingTasks();
    void onAborted(proto::FileError error_code);

    std::shared_ptr<common::FileRequestConsumerProxy> request_consumer_proxy_;
    std::shared_ptr<common::FileRequestProducerProxy> request_producer_proxy_;
    std::unique_ptr<FileRequestFactory> request_factory_;

    FinishCallback callback_;

    FileRemover::TaskList pending_tasks_;
    FileRemover::TaskList tasks_;

    DISALLOW_COPY_AND_ASSIGN(FileRemoveQueueBuilder);
};

} // namespace client

#endif // CLIENT__FILE_REMOVE_QUEUE_BUILDER_H
