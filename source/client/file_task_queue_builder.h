//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_task_queue_builder.h
// LICENSE:         Mozilla Public License Version 2.0
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

    using FileList = std::list<proto::FileList::Item>;

    void Start(std::shared_ptr<FileRequestSenderProxy> sender,
               const FilePath& source_path,
               const FilePath& target_path,
               const FileList& file_list,
               FinishCallback callback);

private:
    // FileReplyReceiver implementation.
    void OnFileListReply(const FilePath& path,
                         std::shared_ptr<proto::FileList> file_list,
                         proto::RequestStatus status) final;

    void AddIncomingTask(const FilePath& source_path,
                         const FilePath& target_path,
                         const proto::FileList::Item& file);
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
