//
// PROJECT:         Aspia
// FILE:            client/file_task_queue_builder.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TASK_QUEUE_BUILDER_H
#define _ASPIA_CLIENT__FILE_TASK_QUEUE_BUILDER_H

#include "client/file_request_sender_proxy.h"
#include "client/file_reply_receiver.h"
#include "client/file_task.h"

namespace aspia {

class FileTaskQueueBuilder : private FileReplyReceiver
{
public:
    FileTaskQueueBuilder() = default;
    ~FileTaskQueueBuilder() = default;

    using FinishCallback = std::function<void(FileTaskQueue& task_queue,
                                              int64_t task_object_size,
                                              int64_t task_object_count)>;

    using FileList = std::list<proto::file_transfer::FileList::Item>;

    void Start(std::shared_ptr<FileRequestSenderProxy> sender,
               const std::experimental::filesystem::path& source_path,
               const std::experimental::filesystem::path& target_path,
               const FileList& file_list,
               const FinishCallback& callback);

    void Start(std::shared_ptr<FileRequestSenderProxy> sender,
               const std::experimental::filesystem::path& path,
               const FileList& file_list,
               const FinishCallback& callback);

private:
    // FileReplyReceiver implementation.
    void OnFileListReply(const std::experimental::filesystem::path& path,
                         std::shared_ptr<proto::file_transfer::FileList> file_list,
                         proto::file_transfer::Status status) final;

    void AddIncomingTask(const std::experimental::filesystem::path& source_path,
                         const std::experimental::filesystem::path& target_path,
                         const proto::file_transfer::FileList::Item& file);
    void FrontIncomingToBackPending();
    void ProcessNextIncommingTask();

    std::shared_ptr<FileRequestSenderProxy> sender_;

    FinishCallback finish_callback_;

    // The total size of all files in queue.
    int64_t task_object_size_ = 0;

    // Number of objects in the queue (directories and files).
    int64_t task_object_count_ = 0;

    FileTaskQueue pending_task_queue_;
    FileTaskQueue incoming_task_queue_;

    DISALLOW_COPY_AND_ASSIGN(FileTaskQueueBuilder);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TASK_QUEUE_BUILDER_H
