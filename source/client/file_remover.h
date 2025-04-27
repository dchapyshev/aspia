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

#ifndef CLIENT_FILE_REMOVER_H
#define CLIENT_FILE_REMOVER_H

#include "base/location.h"
#include "common/file_task.h"
#include "common/file_task_producer.h"
#include "proto/file_transfer.pb.h"

#include <deque>
#include <string>

#include <QObject>

namespace common {
class FileTaskFactory;
class FileTaskConsumerProxy;
class FileTaskProducerProxy;
} // namespace common

namespace client {

class FileRemoveQueueBuilder;

class FileRemover final
    : public QObject,
      public common::FileTaskProducer
{
    Q_OBJECT

public:
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

    FileRemover(common::FileTask::Target target, const TaskList& items, QObject* parent = nullptr);
    ~FileRemover() final;

public slots:
    void start(std::shared_ptr<common::FileTaskConsumerProxy> task_consumer_proxy);
    void stop();
    void setAction(Action action);

signals:
    void sig_started();
    void sig_finished();
    void sig_progressChanged(const std::string& name, int percentage);
    void sig_errorOccurred(const std::string& path,
                           proto::FileError error_code,
                           uint32_t available_actions);

protected:
    // common::FileTaskProducer implementation.
    void onTaskDone(std::shared_ptr<common::FileTask> task) final;

private:
    void doNextTask();
    void doCurrentTask();
    void onFinished(const base::Location& location);

    std::shared_ptr<common::FileTaskConsumerProxy> task_consumer_proxy_;
    std::shared_ptr<common::FileTaskProducerProxy> task_producer_proxy_;
    std::unique_ptr<common::FileTaskFactory> task_factory_;

    std::unique_ptr<FileRemoveQueueBuilder> queue_builder_;

    TaskList tasks_;

    Action failure_action_ = ACTION_ASK;
    size_t tasks_count_ = 0;

    DISALLOW_COPY_AND_ASSIGN(FileRemover);
};

} // namespace client

#endif // CLIENT_FILE_REMOVER_H
