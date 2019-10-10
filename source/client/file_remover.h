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

#ifndef CLIENT__FILE_REMOVER_H
#define CLIENT__FILE_REMOVER_H

#include "base/macros_magic.h"
#include "common/file_request_producer.h"
#include "common/file_task_target.h"
#include "proto/file_transfer.pb.h"

#include <deque>

namespace base {
class TaskRunner;
} // namespace base

namespace common {
class FileRequestConsumerProxy;
class FileRequestProducerProxy;
} // namespace common

namespace client {

class FileRemoveQueueBuilder;
class FileRemoveWindowProxy;
class FileRemoverProxy;
class FileRequestFactory;

class FileRemover : public common::FileRequestProducer
{
public:
    FileRemover(std::shared_ptr<base::TaskRunner> io_task_runner,
                std::shared_ptr<FileRemoveWindowProxy> remove_window_proxy,
                std::shared_ptr<common::FileRequestConsumerProxy> request_consumer_proxy,
                common::FileTaskTarget target);
    ~FileRemover();

    enum Action
    {
        ACTION_ASK      = 0,
        ACTION_ABORT    = 1,
        ACTION_SKIP     = 2,
        ACTION_SKIP_ALL = 4
    };

    class Task
    {
    public:
        Task(std::string&& path, bool is_directory);

        Task(const Task& other) = default;
        Task& operator=(const Task& other) = default;

        Task(Task&& other) noexcept;
        Task& operator=(Task&& other) noexcept;

        ~Task() = default;

        const std::string& path() const { return path_; }
        bool isDirectory() const { return is_directory_; }

    private:
        std::string path_;
        bool is_directory_;
    };


    using TaskList = std::deque<Task>;
    using FinishCallback = std::function<void()>;

    void start(const TaskList& items, FinishCallback callback);
    void stop();
    void setAction(Action action);

protected:
    // common::FileRequestProducer implementation.
    void onReply(std::shared_ptr<common::FileRequest> request) override;

private:
    void doNextTask();
    void onFinished();

    std::shared_ptr<FileRemoverProxy> remover_proxy_;
    std::shared_ptr<FileRemoveWindowProxy> remove_window_proxy_;
    std::shared_ptr<common::FileRequestConsumerProxy> request_consumer_proxy_;
    std::shared_ptr<common::FileRequestProducerProxy> request_producer_proxy_;
    std::unique_ptr<FileRequestFactory> request_factory_;

    std::unique_ptr<FileRemoveQueueBuilder> queue_builder_;

    TaskList tasks_;
    FinishCallback finish_callback_;

    Action failure_action_ = ACTION_ASK;
    size_t tasks_count_ = 0;

    DISALLOW_COPY_AND_ASSIGN(FileRemover);
};

} // namespace client

#endif // CLIENT__FILE_REMOVER_H
