//
// PROJECT:         Aspia
// FILE:            client/file_remover.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REMOVER_H
#define _ASPIA_CLIENT__FILE_REMOVER_H

#include "base/message_loop/message_loop_thread.h"
#include "client/file_request_sender_proxy.h"
#include "client/file_reply_receiver.h"
#include "client/file_task_queue_builder.h"
#include "client/file_constants.h"
#include "proto/file_transfer_session_message.pb.h"

namespace aspia {

class FileRemover
    : private FileReplyReceiver,
      private MessageLoopThread::Delegate
{
public:
    using ActionCallback = std::function<void(FileAction action)>;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void OnRemovingStarted(int64_t object_count) = 0;
        virtual void OnRemovingComplete() = 0;
        virtual void OnRemoveObject(const FilePath& object_path) = 0;
        virtual void OnRemoveObjectFailure(const FilePath& object_path,
                                           proto::RequestStatus status,
                                           ActionCallback callback) = 0;
    };

    FileRemover(std::shared_ptr<FileRequestSenderProxy> sender, Delegate* delegate);
    ~FileRemover();

    void Start(const FilePath& path, const FileTaskQueueBuilder::FileList& file_list);

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() final;
    void OnAfterThreadRunning() final;

    // FileReplyReceiver implementation.
    void OnRemoveReply(const FilePath& path, proto::RequestStatus status) final;

    void OnTaskQueueBuilded(FileTaskQueue& task_queue,
                            int64_t task_object_size,
                            int64_t task_object_count);
    void RunTask(const FileTask& task);
    void RunNextTask();
    void OnRemoveObjectFailureAction(FileAction action);

    Delegate* delegate_;

    MessageLoopThread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    std::shared_ptr<FileRequestSenderProxy> sender_;

    FileTaskQueueBuilder task_queue_builder_;
    FileTaskQueue task_queue_;

    FileAction file_remove_failure_action_ = FileAction::ASK;

    DISALLOW_COPY_AND_ASSIGN(FileRemover);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REMOVER_H
