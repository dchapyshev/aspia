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

#ifndef CLIENT__FILE_TRANSFER_H
#define CLIENT__FILE_TRANSFER_H

#include "base/waitable_timer.h"
#include "common/file_request_producer.h"
#include "common/file_task_target.h"
#include "client/file_transfer_task.h"
#include "proto/file_transfer.pb.h"

#include <queue>

namespace base {
class TaskRunner;
} // namespace base

namespace common {
class FileRequestConsumerProxy;
class FileRequestProducerProxy;
} // namespace common

namespace client {

class FileRequestFactory;
class FileTransferProxy;
class FileTransferQueueBuilder;
class FileTransferWindowProxy;

class FileTransfer : public common::FileRequestProducer
{
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
              size(size),
              is_directory(is_directory)
        {
            // Nothing
        }

        Item(std::string&& name, int64_t size, bool is_directory)
            : name(std::move(name)),
              size(size),
              is_directory(is_directory)
        {
            // Nothing
        }

        std::string name;
        bool is_directory;
        int64_t size;
    };

    using TaskList = std::deque<FileTransferTask>;
    using FinishCallback = std::function<void()>;

    FileTransfer(std::shared_ptr<base::TaskRunner>& io_task_runner,
                 std::shared_ptr<FileTransferWindowProxy>& transfer_window_proxy,
                 std::shared_ptr<common::FileRequestConsumerProxy>& request_consumer_proxy,
                 Type type);
    ~FileTransfer();

    void start(const std::string& source_path,
               const std::string& target_path,
               const std::vector<Item>& items,
               const FinishCallback& finish_callback);
    void stop();

    void setAction(Error::Type error_type, Error::Action action);

protected:
    // common::FileRequestProducer implementation.
    void onReply(std::shared_ptr<common::FileRequest> request) override;

private:
    FileTransferTask& frontTask();
    void targetReply(const proto::FileRequest& request, const proto::FileReply& reply);
    void sourceReply(const proto::FileRequest& request, const proto::FileReply& reply);
    void doFrontTask(bool overwrite);
    void doNextTask();
    void onError(Error::Type type, proto::FileError code, const std::string& path = std::string());
    void setActionForErrorType(Error::Type error_type, Error::Action action);
    void onFinished();

    std::shared_ptr<base::TaskRunner> io_task_runner_;
    std::shared_ptr<FileTransferProxy> transfer_proxy_;
    std::shared_ptr<FileTransferWindowProxy> transfer_window_proxy_;
    std::shared_ptr<common::FileRequestConsumerProxy> request_consumer_proxy_;
    std::shared_ptr<common::FileRequestProducerProxy> request_producer_proxy_;
    std::unique_ptr<FileRequestFactory> request_factory_source_;
    std::unique_ptr<FileRequestFactory> request_factory_target_;

    base::WaitableTimer cancel_timer_;

    // The map contains available actions for the error and the current action.
    std::map<Error::Type, Error::Action> actions_;
    std::unique_ptr<FileTransferQueueBuilder> queue_builder_;
    TaskList tasks_;
    const Type type_;

    FinishCallback finish_callback_;

    int64_t total_size_ = 0;
    int64_t total_transfered_size_ = 0;
    int64_t task_transfered_size_ = 0;

    int total_percentage_ = 0;
    int task_percentage_ = 0;

    bool is_canceled_ = false;

    DISALLOW_COPY_AND_ASSIGN(FileTransfer);
};

} // namespace client

#endif // CLIENT__FILE_TRANSFER_H
