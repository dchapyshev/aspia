//
// PROJECT:         Aspia Remote Desktop
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

void FileRemover::Start(const FilePath& target_path,
                        const FileTaskQueueBuilder::FileList& file_list)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileRemover::Start, this, target_path, file_list));
        return;
    }

    FileTaskQueueBuilder::FinishCallback callback =
        std::bind(&FileRemover::OnTaskQueueBuilded, this,
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    task_queue_builder_.Start(sender_, FilePath(), target_path, file_list, std::move(callback));
}

void FileRemover::OnTaskQueueBuilded(FileTaskQueue& task_queue,
                                     int64_t task_object_size,
                                     int64_t task_object_count)
{
    task_queue_.swap(task_queue_);

    delegate_->OnRemovingStarted(task_object_count);

    RunTask(task_queue_.back());
}

void FileRemover::RunTask(const FileTask& task)
{
    DCHECK(runner_->BelongsToCurrentThread());

    delegate_->OnRemoveObject(task.TargetPath());

    sender_->SendRemoveRequest(This(), task.TargetPath());
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

void FileRemover::OnRemoveReply(const FilePath& path, proto::RequestStatus status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileRemover::OnRemoveReply, this, path, status));
        return;
    }

    if (status != proto::REQUEST_STATUS_SUCCESS)
    {
        // TODO
        return;
    }

    RunNextTask();
}

} // namespace aspia
