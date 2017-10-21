//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_transfer_uploader.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_transfer_uploader.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"

namespace aspia {

FileTransferUploader::FileTransferUploader(std::shared_ptr<FileRequestSenderProxy> local_sender,
                                           std::shared_ptr<FileRequestSenderProxy> remote_sender,
                                           FileTransfer::Delegate* delegate)
    : FileTransfer(local_sender, remote_sender, delegate)
{
    thread_.Start(MessageLoop::TYPE_DEFAULT, this);
}

FileTransferUploader::~FileTransferUploader()
{
    thread_.Stop();
}

void FileTransferUploader::OnBeforeThreadRunning()
{
    runner_ = thread_.message_loop_proxy();
    DCHECK(runner_);
}

void FileTransferUploader::OnAfterThreadRunning()
{
    delegate_->OnTransferComplete();
}

void FileTransferUploader::Start(const FilePath& source_path,
                                 const FilePath& target_path,
                                 const FileTaskQueueBuilder::FileList& file_list)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &FileTransferUploader::Start, this, source_path, target_path, file_list));
        return;
    }

    FileTaskQueueBuilder::FinishCallback callback =
        std::bind(&FileTransferUploader::OnTaskQueueBuilded, this,
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    task_queue_builder_.Start(local_sender_, source_path, target_path, file_list, callback);
}

void FileTransferUploader::OnTaskQueueBuilded(FileTaskQueue& task_queue,
                                              int64_t task_object_size,
                                              int64_t task_object_count)
{
    UNUSED_PARAMETER(task_object_count);

    task_queue_.swap(task_queue);

    delegate_->OnTransferStarted(task_object_size);

    // Run first task.
    RunTask(task_queue_.front(), FileRequestSender::Overwrite::NO);
}

void FileTransferUploader::RunTask(const FileTask& task, FileRequestSender::Overwrite overwrite)
{
    delegate_->OnObjectTransferStarted(task.SourcePath(), task.Size());

    if (task.IsDirectory())
    {
        remote_sender_->SendCreateDirectoryRequest(This(), task.TargetPath());
    }
    else
    {
        remote_sender_->SendFileUploadRequest(This(), task.TargetPath(), overwrite);
    }
}

void FileTransferUploader::RunNextTask()
{
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
    RunTask(task_queue_.front(), FileRequestSender::Overwrite::NO);
}

void FileTransferUploader::OnUnableToCreateDirectoryAction(FileAction action)
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
                create_directory_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferUploader::OnCreateDirectoryReply(const FilePath& path,
                                                  proto::RequestStatus status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &FileTransferUploader::OnCreateDirectoryReply, this, path, status));
        return;
    }

    if (status == proto::REQUEST_STATUS_SUCCESS ||
        status == proto::REQUEST_STATUS_PATH_ALREADY_EXISTS)
    {
        RunNextTask();
        return;
    }

    if (create_directory_failure_action_ != FileAction::ASK)
    {
        OnUnableToCreateDirectoryAction(create_directory_failure_action_);
        return;
    }

    ActionCallback callback = std::bind(
        &FileTransferUploader::OnUnableToCreateDirectoryAction, this, std::placeholders::_1);

    delegate_->OnFileOperationFailure(path, status, std::move(callback));
}

void FileTransferUploader::OnUnableToCreateFileAction(FileAction action)
{
    switch (action)
    {
        case FileAction::ABORT:
            runner_->PostQuit();
            break;

        case FileAction::REPLACE:
        case FileAction::REPLACE_ALL:
        {
            if (action == FileAction::REPLACE_ALL)
                file_create_failure_action_ = action;

            RunTask(task_queue_.front(), FileRequestSender::Overwrite::YES);
        }
        break;

        case FileAction::SKIP:
        case FileAction::SKIP_ALL:
        {
            if (action == FileAction::SKIP_ALL)
                file_create_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferUploader::OnUnableToOpenFileAction(FileAction action)
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
                file_open_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferUploader::OnUnableToReadFileAction(FileAction action)
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
                file_read_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferUploader::OnFileUploadReply(const FilePath& file_path,
                                             proto::RequestStatus status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &FileTransferUploader::OnFileUploadReply, this, file_path, status));
        return;
    }

    if (status != proto::REQUEST_STATUS_SUCCESS)
    {
        if (file_create_failure_action_ == FileAction::ASK)
        {
            ActionCallback callback = std::bind(
                &FileTransferUploader::OnUnableToCreateFileAction, this, std::placeholders::_1);

            delegate_->OnFileOperationFailure(file_path, status, std::move(callback));
        }
        else
        {
            OnUnableToCreateFileAction(file_create_failure_action_);
        }

        return;
    }

    const FileTask& current_task = task_queue_.front();

    file_packetizer_ = FilePacketizer::Create(current_task.SourcePath());
    if (!file_packetizer_)
    {
        if (file_open_failure_action_ == FileAction::ASK)
        {
            ActionCallback callback = std::bind(
                &FileTransferUploader::OnUnableToOpenFileAction, this, std::placeholders::_1);

            delegate_->OnFileOperationFailure(current_task.SourcePath(),
                                              proto::REQUEST_STATUS_ACCESS_DENIED,
                                              std::move(callback));
        }
        else
        {
            OnUnableToOpenFileAction(file_open_failure_action_);
        }

        return;
    }

    std::unique_ptr<proto::FilePacket> file_packet = file_packetizer_->CreateNextPacket();
    if (!file_packet)
    {
        if (file_read_failure_action_ == FileAction::ASK)
        {
            ActionCallback callback = std::bind(
                &FileTransferUploader::OnUnableToReadFileAction, this, std::placeholders::_1);

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

    remote_sender_->SendFilePacket(This(), std::move(file_packet));
}

void FileTransferUploader::OnUnableToWriteFileAction(FileAction action)
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
                file_write_failure_action_ = action;

            RunNextTask();
        }
        break;

        default:
            DLOG(FATAL) << "Unexpected action: " << static_cast<int>(action);
            break;
    }
}

void FileTransferUploader::OnFilePacketSended(uint32_t flags, proto::RequestStatus status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileTransferUploader::OnFilePacketSended, this, flags, status));
        return;
    }

    if (!file_packetizer_)
    {
        LOG(ERROR) << "Unexpected reply: file upload data";
        runner_->PostQuit();
        return;
    }

    if (status != proto::RequestStatus::REQUEST_STATUS_SUCCESS)
    {
        if (file_write_failure_action_ == FileAction::ASK)
        {
            ActionCallback callback = std::bind(
                &FileTransferUploader::OnUnableToWriteFileAction, this, std::placeholders::_1);

            const FileTask& current_task = task_queue_.front();

            delegate_->OnFileOperationFailure(current_task.TargetPath(),
                                              status,
                                              std::move(callback));
        }
        else
        {
            OnUnableToWriteFileAction(file_write_failure_action_);
        }

        return;
    }

    delegate_->OnObjectTransfer(file_packetizer_->LeftSize());

    if (flags & proto::FilePacket::LAST_PACKET)
    {
        file_packetizer_.reset();
        RunNextTask();
        return;
    }

    std::unique_ptr<proto::FilePacket> file_packet = file_packetizer_->CreateNextPacket();
    if (!file_packet)
    {
        if (file_read_failure_action_ == FileAction::ASK)
        {
            ActionCallback callback = std::bind(
                &FileTransferUploader::OnUnableToReadFileAction, this, std::placeholders::_1);

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

    remote_sender_->SendFilePacket(This(), std::move(file_packet));
}

} // namespace aspia
