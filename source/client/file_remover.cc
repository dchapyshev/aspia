//
// PROJECT:         Aspia
// FILE:            client/file_remover.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_remover.h"
#include "base/logging.h"

namespace aspia {

FileRemover::FileRemover(std::shared_ptr<FileRequestSenderProxy> sender, Delegate* delegate)
    : delegate_(delegate),
      sender_(sender)
{
    thread_.Start(MessageLoop::TYPE_DEFAULT, this);
}

FileRemover::~FileRemover()
{
    thread_.Stop();
}

void FileRemover::OnBeforeThreadRunning()
{
    runner_ = thread_.message_loop_proxy();
    DCHECK(runner_);
}

void FileRemover::OnAfterThreadRunning()
{
    delegate_->OnRemovingComplete();
}

void FileRemover::Start(const std::experimental::filesystem::path& path,
                        const FileTaskQueueBuilder::FileList& file_list)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileRemover::Start, this, path, file_list));
        return;
    }

    FileTaskQueueBuilder::FinishCallback callback =
        std::bind(&FileRemover::OnTaskQueueBuilded, this,
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    task_queue_builder_.Start(sender_, path, file_list, callback);
}

void FileRemover::OnTaskQueueBuilded(FileTaskQueue& task_queue,
                                     int64_t /* task_object_size */,
                                     int64_t task_object_count)
{
    task_queue_.swap(task_queue);

    delegate_->OnRemovingStarted(task_object_count);

    // When deleting, we bypass the task queue from the end.
    RunTask(task_queue_.back());
}

void FileRemover::RunTask(const FileTask& task)
{
    DCHECK(runner_->BelongsToCurrentThread());

    delegate_->OnRemoveObject(task.SourcePath());

    sender_->SendRemoveRequest(This(), task.SourcePath());
}

void FileRemover::RunNextTask()
{
    DCHECK(runner_->BelongsToCurrentThread());

    // Delete the task only after confirmation of its successful execution.
    if (!task_queue_.empty())
    {
        task_queue_.pop_back();
    }

    if (task_queue_.empty())
    {
        runner_->PostQuit();
        return;
    }

    // Run next task.
    RunTask(task_queue_.back());
}

void FileRemover::OnRemoveObjectFailureAction(FileAction action)
{
    switch (action)
    {
        case FileAction::ABORT:
            runner_->PostQuit();
            break;

        case FileAction::SKIP:
        case FileAction::SKIP_ALL:
        {
            if (action == FileAction::SKIP_ALL)
                file_remove_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileRemover::OnRemoveReply(const std::experimental::filesystem::path& path,
                                proto::file_transfer::Status status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileRemover::OnRemoveReply, this, path, status));
        return;
    }

    if (status == proto::file_transfer::STATUS_SUCCESS)
    {
        RunNextTask();
        return;
    }

    if (file_remove_failure_action_ == FileAction::ASK)
    {
        ActionCallback callback = std::bind(
            &FileRemover::OnRemoveObjectFailureAction, this, std::placeholders::_1);

        delegate_->OnRemoveObjectFailure(path, status, callback);
    }
    else
    {
        OnRemoveObjectFailureAction(file_remove_failure_action_);
    }
}

} // namespace aspia
