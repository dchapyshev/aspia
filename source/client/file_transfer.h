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

#ifndef CLIENT_FILE_TRANSFER_H
#define CLIENT_FILE_TRANSFER_H

#include "base/location.h"
#include "common/file_task.h"
#include "common/file_task_producer.h"
#include "proto/file_transfer.h"

#include <deque>
#include <map>

#include <QTimer>

namespace common {
class FileTaskConsumerProxy;
class FileTaskProducerProxy;
class FileTaskFactory;
} // namespace common

namespace client {

class FileTransferQueueBuilder;

class FileTransfer final
    : public QObject,
      public common::FileTaskProducer
{
    Q_OBJECT

public:
    enum class Type
    {
        DOWNLOADER,
        UPLOADER
    };

    class Error
    {
    public:
        enum class Type
        {
            QUEUE,
            CREATE_DIRECTORY,
            CREATE_FILE,
            OPEN_FILE,
            ALREADY_EXISTS,
            WRITE_FILE,
            READ_FILE,
            OTHER
        };

        enum Action
        {
            ACTION_ASK = 0,
            ACTION_ABORT = 1,
            ACTION_SKIP = 2,
            ACTION_SKIP_ALL = 4,
            ACTION_REPLACE = 8,
            ACTION_REPLACE_ALL = 16
        };

        Error(Type type, proto::FileError code, const std::string& path)
            : type_(type),
              code_(code),
              path_(path)
        {
            // Nothing
        }

        ~Error() = default;

        Type type() const { return type_; }
        proto::FileError code() const { return code_; }
        const std::string& path() const { return path_; }

        uint32_t availableActions() const;
        Action defaultAction() const;

    private:
        const Type type_;
        const proto::FileError code_;
        const std::string path_;
    };

    struct Item
    {
        Item(const std::string& name, int64_t size, bool is_directory)
            : name(name),
              is_directory(is_directory),
              size(size)
        {
            // Nothing
        }

        Item(std::string&& name, int64_t size, bool is_directory)
            : name(std::move(name)),
              is_directory(is_directory),
              size(size)
        {
            // Nothing
        }

        std::string name;
        bool is_directory;
        int64_t size;
    };

    class Task
    {
    public:
        Task(std::string&& source_path, std::string&& target_path,
             bool is_directory, int64_t size);

        Task(const Task& other) = default;
        Task& operator=(const Task& other) = default;

        Task(Task&& other) noexcept;
        Task& operator=(Task&& other) noexcept;

        ~Task() = default;

        const std::string& sourcePath() const { return source_path_; }
        const std::string& targetPath() const { return target_path_; }
        bool isDirectory() const { return is_directory_; }
        int64_t size() const { return size_; }

        bool overwrite() const { return overwrite_; }
        void setOverwrite(bool value) { overwrite_ = value; }

    private:
        std::string source_path_;
        std::string target_path_;
        bool is_directory_;
        bool overwrite_ = false;
        int64_t size_;
    };

    using TaskList = std::deque<Task>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Milliseconds = std::chrono::milliseconds;

    FileTransfer(Type type,
                 const std::string& source_path,
                 const std::string& target_path,
                 const std::vector<Item>& items,
                 QObject* parent = nullptr);
    ~FileTransfer() final;

    void start(std::shared_ptr<common::FileTaskConsumerProxy> task_consumer_proxy);
    void stop();

    void setAction(Error::Type error_type, Error::Action action);

signals:
    void sig_started();
    void sig_finished();
    void sig_progressChanged(int total, int current);
    void sig_currentItemChanged(const std::string& source_path, const std::string& target_path);
    void sig_currentSpeedChanged(int64_t speed);
    void sig_errorOccurred(const FileTransfer::Error& error);

protected:
    // common::FileTaskProducer implementation.
    void onTaskDone(std::shared_ptr<common::FileTask> task) final;

private:
    Task& frontTask();
    void targetReply(const proto::FileRequest& request, const proto::FileReply& reply);
    void sourceReply(const proto::FileRequest& request, const proto::FileReply& reply);
    void doFrontTask(bool overwrite);
    void doNextTask();
    void doUpdateSpeed();
    void onError(Error::Type type, proto::FileError code, const std::string& path = std::string());
    void setActionForErrorType(Error::Type error_type, Error::Action action);
    void onFinished(const base::Location& location);

    const Type type_;
    const std::string source_path_;
    const std::string target_path_;
    const std::vector<Item> items_;

    std::shared_ptr<common::FileTaskConsumerProxy> task_consumer_proxy_;
    std::shared_ptr<common::FileTaskProducerProxy> task_producer_proxy_;
    std::unique_ptr<common::FileTaskFactory> task_factory_source_;
    std::unique_ptr<common::FileTaskFactory> task_factory_target_;

    QTimer cancel_timer_;

    // The map contains available actions for the error and the current action.
    std::map<Error::Type, Error::Action> actions_;
    std::unique_ptr<FileTransferQueueBuilder> queue_builder_;
    TaskList tasks_;

    int64_t total_size_ = 0;
    int64_t total_transfered_size_ = 0;
    int64_t task_transfered_size_ = 0;

    int total_percentage_ = 0;
    int task_percentage_ = 0;

    bool is_canceled_ = false;

    QTimer speed_update_timer_;
    TimePoint begin_time_;
    int64_t bytes_per_time_ = 0;
    int64_t speed_ = 0;

    DISALLOW_COPY_AND_ASSIGN(FileTransfer);
};

} // namespace client

#endif // CLIENT_FILE_TRANSFER_H
