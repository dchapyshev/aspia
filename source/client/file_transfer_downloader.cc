//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_transfer_downloader.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_transfer_downloader.h"
#include "base/files/file_util.h"
#include "base/logging.h"

namespace aspia {

FileTransferDownloader::FileTransferDownloader(
    std::shared_ptr<FileRequestSenderProxy> local_sender,
    std::shared_ptr<FileRequestSenderProxy> remote_sender,
    FileTransfer::Delegate* delegate)
    : FileTransfer(local_sender, remote_sender, delegate)
{
    thread_.Start(MessageLoop::TYPE_DEFAULT, this);
}

FileTransferDownloader::~FileTransferDownloader()
{
    thread_.Stop();
}

void FileTransferDownloader::OnBeforeThreadRunning()
{
    runner_ = thread_.message_loop_proxy();
    DCHECK(runner_);
}

void FileTransferDownloader::OnAfterThreadRunning()
{
    delegate_->OnTransferComplete();
}

void FileTransferDownloader::Start(const FilePath& source_path,
                                   const FilePath& target_path,
                                   const FileList& file_list)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &FileTransferDownloader::Start, this, source_path, target_path, file_list));
        return;
    }

    FileTaskQueueBuilder::FinishCallback callback =
        std::bind(&FileTransferDownloader::OnTaskQueueBuilded, this,
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    task_queue_builder_.Start(
        remote_sender_, source_path, target_path, file_list, std::move(callback));
}

void FileTransferDownloader::OnTaskQueueBuilded(FileTaskQueue& task_queue,
                                                int64_t task_object_size,
                                                int64_t task_object_count)
{
    task_queue_.swap(task_queue);

    delegate_->OnTransferStarted(task_object_size);

    // Run first task.
    RunTask(task_queue_.front());
}

void FileTransferDownloader::OnUnableToCreateDirectoryAction(Action action)
{
    switch (action)
    {
        case Action::ABORT:
            runner_->PostQuit();
            break;

        case Action::SKIP:
        case Action::SKIP_ALL:
        {
            if (action == Action::SKIP_ALL)
                create_directory_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferDownloader::RunTask(const FileTask& task)
{
    DCHECK(runner_->BelongsToCurrentThread());

    delegate_->OnObjectTransferStarted(task.SourcePath(), task.Size());

    if (task.IsDirectory())
    {
        local_sender_->SendCreateDirectoryRequest(This(), task.TargetPath());
    }
    else
    {
        remote_sender_->SendFileDownloadRequest(This(), task.SourcePath());
    }
}

void FileTransferDownloader::RunNextTask()
{
    DCHECK(runner_->BelongsToCurrentThread());

    // Delete the task only after confirmation of its successful execution.
    if (!task_queue_.empty())
    {
        delegate_->OnObjectTransfer(0);
        task_queue_.pop_front();
    }

    if (task_queue_.empty())
    {
        runner_->PostQuit();
        return;
    }

    // Run next task.
    RunTask(task_queue_.front());
}

void FileTransferDownloader::OnCreateDirectoryReply(const FilePath& path,
                                                    proto::RequestStatus status)
{
    if (status != proto::REQUEST_STATUS_SUCCESS)
    {
        if (create_directory_failure_action_ == Action::ASK)
        {
            ActionCallback callback =
                std::bind(&FileTransferDownloader::OnUnableToCreateDirectoryAction, this,
                          std::placeholders::_1);

            const FileTask& current_task = task_queue_.front();

            delegate_->OnFileOperationFailure(current_task.TargetPath(),
                                              proto::REQUEST_STATUS_ACCESS_DENIED,
                                              std::move(callback));
        }
        else
        {
            OnUnableToCreateDirectoryAction(create_directory_failure_action_);
        }
    }
    else
    {
        RunNextTask();
    }
}

void FileTransferDownloader::CreateDepacketizer(const FilePath& file_path, bool overwrite)
{
    proto::RequestStatus status = proto::REQUEST_STATUS_SUCCESS;

    do
    {
        if (!overwrite)
        {
            if (PathExists(file_path))
            {
                status = proto::REQUEST_STATUS_PATH_ALREADY_EXISTS;
                break;
            }
        }

        file_depacketizer_ = FileDepacketizer::Create(file_path, overwrite);
        if (!file_depacketizer_)
        {
            status = proto::REQUEST_STATUS_ACCESS_DENIED;
            break;
        }
    }
    while (false);

    if (status == proto::REQUEST_STATUS_SUCCESS)
    {
        remote_sender_->SendFilePacketRequest(This());
        return;
    }

    if (file_create_failure_action_ == Action::ASK)
    {
        ActionCallback callback = std::bind(
            &FileTransferDownloader::OnUnableToCreateFileAction, this, std::placeholders::_1);

        delegate_->OnFileOperationFailure(file_path, status, std::move(callback));
    }
    else
    {
        OnUnableToCreateFileAction(file_create_failure_action_);
    }
}

void FileTransferDownloader::OnUnableToCreateFileAction(Action action)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &FileTransferDownloader::OnUnableToCreateFileAction, this, action));
        return;
    }

    switch (action)
    {
        case Action::ABORT:
            runner_->PostQuit();
            break;

        case Action::REPLACE:
        case Action::REPLACE_ALL:
        {
            if (action == Action::REPLACE_ALL)
                file_create_failure_action_ = action;

            const FileTask& current_task = task_queue_.front();
            CreateDepacketizer(current_task.TargetPath(), true);
        }
        break;

        case Action::SKIP:
        case Action::SKIP_ALL:
        {
            if (action == Action::SKIP_ALL)
                file_create_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferDownloader::OnUnableToOpenFileAction(Action action)
{
    switch (action)
    {
        case Action::ABORT:
            runner_->PostQuit();
            break;

        case Action::SKIP:
        case Action::SKIP_ALL:
        {
            if (action == Action::SKIP_ALL)
                file_open_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferDownloader::OnFileDownloadReply(const FilePath& file_path,
                                                 proto::RequestStatus status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &FileTransferDownloader::OnFileDownloadReply, this, file_path, status));
        return;
    }

    DCHECK(!task_queue_.empty());
    const FileTask& current_task = task_queue_.front();

    if (status != proto::REQUEST_STATUS_SUCCESS)
    {
        if (file_open_failure_action_ == Action::ASK)
        {
            ActionCallback callback = std::bind(
                &FileTransferDownloader::OnUnableToOpenFileAction, this, std::placeholders::_1);

            delegate_->OnFileOperationFailure(current_task.SourcePath(),
                                              status,
                                              std::move(callback));
        }
        else
        {
            OnUnableToOpenFileAction(file_open_failure_action_);
        }

        return;
    }

    CreateDepacketizer(current_task.TargetPath(), false);
}

void FileTransferDownloader::OnUnableToReadFileAction(Action action)
{
    switch (action)
    {
        case Action::ABORT:
            runner_->PostQuit();
            break;

        case Action::SKIP:
        case Action::SKIP_ALL:
        {
            if (action == Action::SKIP_ALL)
                file_read_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferDownloader::OnUnableToWriteFileAction(Action action)
{
    switch (action)
    {
        case Action::ABORT:
            runner_->PostQuit();
            break;

        case Action::SKIP:
        case Action::SKIP_ALL:
        {
            if (action == Action::SKIP_ALL)
                file_write_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferDownloader::OnFilePacketReceived(std::shared_ptr<proto::FilePacket> file_packet,
                                                  proto::RequestStatus status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &FileTransferDownloader::OnFilePacketReceived, this, file_packet, status));
        return;
    }

    if (!file_depacketizer_)
    {
        LOG(ERROR) << "Unexpected file packet";
        runner_->PostQuit();
        return;
    }

    if (status != proto::REQUEST_STATUS_SUCCESS)
    {
        if (file_read_failure_action_ == Action::ASK)
        {
            ActionCallback callback = std::bind(
                &FileTransferDownloader::OnUnableToReadFileAction, this, std::placeholders::_1);

            const FileTask& current_task = task_queue_.front();

            delegate_->OnFileOperationFailure(current_task.SourcePath(),
                                              proto::REQUEST_STATUS_ACCESS_DENIED,
                                              std::move(callback));
        }
        else
        {
            OnUnableToReadFileAction(file_read_failure_action_);
        }

        return;
    }

    if (!file_depacketizer_->ReadNextPacket(*file_packet))
    {
        if (file_write_failure_action_ == Action::ASK)
        {
            ActionCallback callback = std::bind(
                &FileTransferDownloader::OnUnableToWriteFileAction, this, std::placeholders::_1);

            const FileTask& current_task = task_queue_.front();

            delegate_->OnFileOperationFailure(current_task.TargetPath(),
                                              proto::REQUEST_STATUS_ACCESS_DENIED,
                                              std::move(callback));
        }
        else
        {
            OnUnableToWriteFileAction(file_write_failure_action_);
        }

        return;
    }

    delegate_->OnObjectTransfer(file_depacketizer_->LeftSize());

    if (file_packet->flags() & proto::FilePacket::LAST_PACKET)
    {
        file_depacketizer_.reset();
        RunNextTask();
    }
    else
    {
        remote_sender_->SendFilePacketRequest(This());
    }
}

} // namespace aspia
