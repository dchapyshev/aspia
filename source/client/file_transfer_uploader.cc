//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_transfer_uploader.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_transfer_uploader.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"

namespace aspia {

FileTransferUploader::FileTransferUploader(std::shared_ptr<FileRequestSenderProxy> sender,
                                           FileTransfer::Delegate* delegate)
    : FileTransfer(std::move(sender), delegate)
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

uint64_t FileTransferUploader::BuildTaskListForDirectoryContent(const FilePath& source_path,
                                                                const FilePath& target_path)
{
    FileEnumerator enumerator(source_path,
                              false,
                              FileEnumerator::FILES | FileEnumerator::DIRECTORIES);
    uint64_t size = 0;

    for (;;)
    {
        FilePath source_object_path = enumerator.Next();
        if (source_object_path.empty())
            break;

        FilePath target_object_path;

        target_object_path.assign(target_path);
        target_object_path.append(source_object_path.filename());

        FileEnumerator::FileInfo info = enumerator.GetInfo();

        if (info.IsDirectory())
        {
            task_queue_.emplace(source_object_path, target_object_path, 0, true);
            size += BuildTaskListForDirectoryContent(source_object_path, target_object_path);
        }
        else
        {
            int64_t file_size = info.GetSize();
            task_queue_.emplace(source_object_path, target_object_path, file_size, false);
            size += file_size;
        }
    }

    return size;
}

void FileTransferUploader::Start(const FilePath& source_path,
                                 const FilePath& target_path,
                                 const FileList& file_list)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &FileTransferUploader::Start, this, source_path, target_path, file_list));
        return;
    }

    uint64_t total_size = 0;

    for (const auto& file : file_list)
    {
        FilePath source_object_path;

        source_object_path.assign(source_path);
        source_object_path.append(std::experimental::filesystem::u8path(file.name()));

        FilePath target_object_path;

        target_object_path.assign(target_path);
        target_object_path.append(std::experimental::filesystem::u8path(file.name()));

        if (file.is_directory())
        {
            task_queue_.emplace(source_object_path, target_object_path, 0, true);

            total_size += BuildTaskListForDirectoryContent(source_object_path, target_object_path);
        }
        else
        {
            int64_t file_size = 0;
            GetFileSize(source_object_path, file_size);

            task_queue_.emplace(source_object_path, target_object_path, file_size, false);

            total_size += file_size;
        }
    }

    delegate_->OnTransferStarted(total_size);

    // Run first task.
    RunTask(task_queue_.front(), FileRequestSender::Overwrite::NO);
}

void FileTransferUploader::RunTask(const FileTask& task, FileRequestSender::Overwrite overwrite)
{
    delegate_->OnObjectTransferStarted(task.SourcePath(), task.Size());

    if (task.IsDirectory())
    {
        sender_->SendCreateDirectoryRequest(This(), task.TargetPath());
    }
    else
    {
        sender_->SendFileUploadRequest(This(), task.TargetPath(), overwrite);
    }
}

void FileTransferUploader::RunNextTask()
{
    // Delete the task only after confirmation of its successful execution.
    if (!task_queue_.empty())
    {
        delegate_->OnObjectTransfer(0);
        task_queue_.pop();
    }

    if (task_queue_.empty())
    {
        runner_->PostQuit();
        return;
    }

    // Run next task.
    RunTask(task_queue_.front(), FileRequestSender::Overwrite::NO);
}

void FileTransferUploader::OnDriveListReply(std::unique_ptr<proto::DriveList> drive_list,
                                            proto::RequestStatus status)
{
    DLOG(FATAL) << "Unexpectedly received: drive list";
}

void FileTransferUploader::OnFileListReply(const FilePath& path,
                                           std::unique_ptr<proto::FileList> file_list,
                                           proto::RequestStatus status)
{
    DLOG(FATAL) << "Unexpectedly received: file list";
}

void FileTransferUploader::OnDirectorySizeReply(const FilePath& path,
                                                uint64_t size,
                                                proto::RequestStatus status)
{
    DLOG(FATAL) << "Unexpectedly received: directory size";
}

void FileTransferUploader::OnUnableToCreateDirectoryAction(Action action)
{
    switch (action)
    {
        case Action::ABORT:
            runner_->PostQuit();
            break;

        case Action::SKIP:
            RunNextTask();
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

    if (create_directory_failure_action_ != Action::ASK)
    {
        OnUnableToCreateDirectoryAction(create_directory_failure_action_);
        return;
    }

    ActionCallback callback = std::bind(
        &FileTransferUploader::OnUnableToCreateDirectoryAction, this, std::placeholders::_1);

    delegate_->OnFileOperationFailure(path, status, std::move(callback));
}

void FileTransferUploader::OnRemoveReply(const FilePath& path, proto::RequestStatus status)
{
    DLOG(FATAL) << "Unexpectedly reply: remove";
}

void FileTransferUploader::OnRenameReply(const FilePath& old_name,
                                         const FilePath& new_name,
                                         proto::RequestStatus status)
{
    DLOG(FATAL) << "Unexpectedly reply: rename";
}

void FileTransferUploader::OnUnableToCreateFileAction(Action action)
{
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

            RunTask(task_queue_.front(), FileRequestSender::Overwrite::YES);
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

void FileTransferUploader::OnUnableToOpenFileAction(Action action)
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

void FileTransferUploader::OnUnableToReadFileAction(Action action)
{
    switch (action)
    {
        case Action::ABORT:
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
        if (file_create_failure_action_ == Action::ASK)
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

    DCHECK(!file_packetizer_);

    file_packetizer_ = FilePacketizer::Create(current_task.SourcePath());
    if (!file_packetizer_)
    {
        if (file_open_failure_action_ == Action::ASK)
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
        if (file_read_failure_action_ == Action::ASK)
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

    sender_->SendFilePacket(This(), std::move(file_packet));
}

void FileTransferUploader::OnFileDownloadReply(const FilePath& file_path,
                                               proto::RequestStatus status)
{
    DLOG(FATAL) << "Unexpectedly received: download reply";
}

void FileTransferUploader::OnUnableToWriteFileAction(Action action)
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
        if (file_write_failure_action_ == Action::ASK)
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
        if (file_read_failure_action_ == Action::ASK)
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

    sender_->SendFilePacket(This(), std::move(file_packet));
}

void FileTransferUploader::OnFilePacketReceived(std::unique_ptr<proto::FilePacket> file_packet,
                                                proto::RequestStatus status)
{
    DLOG(FATAL) << "Unexpectedly reply: file packet received";
}

} // namespace aspia
